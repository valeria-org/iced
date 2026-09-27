// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Generator.Enums;
using Generator.IO;

namespace Generator.Decoder.Cpp {
	/// <summary>
	/// Writes the decoder handlers of one decoder table (legacy, VEX, EVEX, XOP, MVEX) as constant data (the other languages
	/// serialize them and create the handlers at runtime).
	/// <para/>
	/// Each handler definition (<c>[kind, args...]</c>) becomes a <c>constexpr</c> handler object created by the C++ factory fn
	/// with the same name as the kind (<c>src/internal/decoder/&lt;ctors&gt;_ctors.hpp</c>, eg. <c>legacy_ctors::Ev_Gv_3a()</c>)
	/// called with the definition's args: enum values, bools, ints, pointers to other handlers (defined before it) and braced lists
	/// of handler pointers (the named handler arrays, eg. the 8 handlers of a <c>Group</c> handler). Identical handlers are only
	/// created once. The <c>Invalid</c>, <c>Invalid_NoModRM</c> and <c>Null</c> kinds and <c>null</c> are the singletons
	/// <c>INVALID_HANDLER</c>, <c>INVALID_NO_MODRM_HANDLER</c> and <c>NULL_HANDLER</c>. The top level 0x100-entry tables are
	/// <c>HandlerEntry</c> arrays with external linkage.
	/// </summary>
	sealed class CppDecoderTableWriter {
		const int MaxArrayElemsPerLine = 8;

		public string TableName { get; }
		string Namespace => $"{CppConstants.InternalNamespace}::decoder_data_{TableName}";
		string CtorsNamespace => $"{ctorsName}_ctors";

		readonly IdentifierConverter idConverter;
		readonly DecoderTableSerializerInfo info;
		readonly string ctorsName;
		// Table name -> handler expression (a pointer) or the handler array (list of pointer expressions)
		readonly Dictionary<string, (string? handler, List<string>? handlers)> namedData = new(StringComparer.Ordinal);
		// Handler ctor call -> variable name
		readonly Dictionary<string, string> exprToVarName = new(StringComparer.Ordinal);
		readonly List<string> lines = new();
		readonly List<(string name, int count)> publicTables = new();

		public CppDecoderTableWriter(string tableName, string ctorsName, DecoderTableSerializerInfo info) {
			idConverter = CppIdentifierConverter.Create();
			TableName = tableName;
			this.ctorsName = ctorsName;
			this.info = info;
		}

		public int HandlerCount => exprToVarName.Count;

		void Create() {
			if (lines.Count != 0)
				return;
			var publicNames = new HashSet<string>(info.TableIndexNames, StringComparer.Ordinal);
			foreach (var (name, handlers) in info.TablesToSerialize) {
				if (DecoderTableUtils.IsHandler(handlers)) {
					if (publicNames.Contains(name))
						throw new InvalidOperationException();
					namedData.Add(name, (GetHandler(handlers), null));
				}
				else {
					var list = handlers.Select(a => GetHandler(a)).ToList();
					namedData.Add(name, (null, list));
					if (publicNames.Contains(name)) {
						if (list.Count != 0x100)
							throw new InvalidOperationException();
						// Eg. Handlers_0F38 -> HANDLERS_0F38 (Constant() would return HANDLERS_0_F38)
						var constName = name.ToUpperInvariant();
						publicTables.Add((constName, list.Count));
						lines.Add($"constexpr HandlerEntry {constName}[0x{list.Count:X}] = {{");
						for (int i = 0; i < list.Count; i++)
							lines.Add($"\tto_handler_entry({list[i]}), // 0x{i:X2}");
						lines.Add("};");
					}
				}
			}
			if (publicTables.Count != info.TableIndexNames.Length)
				throw new InvalidOperationException();
		}

		string GetHandler(object? data) {
			switch (data) {
			case null:
				_ = info.NullValue; // Throws if null isn't allowed
				return "&NULL_HANDLER";

			case string name:
				if (!namedData.TryGetValue(name, out var named) || named.handler is null)
					throw new InvalidOperationException($"Invalid handler name: {name}");
				return named.handler;

			case object?[] handler when DecoderTableUtils.IsHandler(handler, out var kind):
				switch (kind.RawName) {
				case "Invalid":
					if (handler.Length != 1)
						throw new InvalidOperationException();
					return "&INVALID_HANDLER";
				case "Invalid_NoModRM":
					if (handler.Length != 1)
						throw new InvalidOperationException();
					return "&INVALID_NO_MODRM_HANDLER";
				case "Null":
					if (handler.Length != 1)
						throw new InvalidOperationException();
					return "&NULL_HANDLER";
				case "Invalid2":
				case "Dup":
				case "HandlerReference":
				case "ArrayReference":
					throw new InvalidOperationException();
				}
				var args = new List<string>();
				for (int i = 1; i < handler.Length; i++)
					args.Add(GetArg(handler[i]));
				var expr = $"{CtorsNamespace}::{kind.Name(idConverter)}({string.Join(", ", args)})";
				if (!exprToVarName.TryGetValue(expr, out var varName)) {
					varName = $"H{exprToVarName.Count}";
					exprToVarName.Add(expr, varName);
					lines.Add($"constexpr auto {varName} = {expr};");
				}
				return "&" + varName;

			default:
				throw new InvalidOperationException();
			}
		}

		string GetArg(object? data) {
			switch (data) {
			case null:
			case object?[]:
				return GetHandler(data);

			case string name:
				if (!namedData.TryGetValue(name, out var named))
					throw new InvalidOperationException($"Invalid table name: {name}");
				if (named.handler is not null)
					return named.handler;
				return FormatArray(named.handlers!);

			case EnumValue enumValue:
				return FormatEnumValue(enumValue);

			case OrEnumValue orEnumValue:
				return string.Join(" | ", orEnumValue.Values.Select(a => FormatEnumValue(a)));

			case bool value:
				return value ? "true" : "false";

			case int value:
				if ((uint)value < 10)
					return value.ToString();
				return "0x" + value.ToString("X");

			default:
				throw new InvalidOperationException();
			}
		}

		string FormatEnumValue(EnumValue value) {
			var typeId = value.DeclaringType.TypeId;
			if (typeId == TypeIds.Code || typeId == TypeIds.Register || typeId == TypeIds.TupleType)
				return CppConstants.ToEnumValue(idConverter, value);
			if (typeId == TypeIds.DecoderOptions || typeId == TypeIds.HandlerFlags || typeId == TypeIds.LegacyHandlerFlags) {
				if (!value.DeclaringType.IsFlags)
					throw new InvalidOperationException();
				return $"{value.DeclaringType.Name(idConverter)}::{idConverter.Constant(value.RawName)}";
			}
			throw new InvalidOperationException();
		}

		// A braced list of handler pointers. Big arrays use several lines (8 handlers per line)
		static string FormatArray(List<string> handlers) {
			if (handlers.Count <= MaxArrayElemsPerLine)
				return "{" + string.Join(", ", handlers) + "}";
			var sb = new StringBuilder();
			sb.Append('{');
			for (int i = 0; i < handlers.Count; i += MaxArrayElemsPerLine) {
				sb.Append("\n\t");
				sb.Append(string.Join(", ", handlers.Skip(i).Take(MaxArrayElemsPerLine)));
				sb.Append($", // 0x{i:X2}");
			}
			sb.Append('\n');
			sb.Append('}');
			return sb.ToString();
		}

		public void WriteSource(FileWriter writer) {
			Create();
			writer.WriteFileHeader();
			writer.WriteLine($"#include \"internal/decoder/data_{TableName}.hpp\"");
			writer.WriteLine($"#include \"internal/decoder/{ctorsName}_ctors.hpp\"");
			writer.WriteLine();
			CppConstants.WriteNamespaceBegin(writer, Namespace);
			writer.WriteLine("// clang-format off");
			foreach (var line in lines) {
				foreach (var l in line.Split('\n'))
					writer.WriteLine(l);
			}
			writer.WriteLine("// clang-format on");
			CppConstants.WriteNamespaceEnd(writer, Namespace);
		}

		public void WriteHeader(FileWriter writer) {
			Create();
			CppConstants.WriteHeaderFileHeader(writer);
			writer.WriteLine("#include \"iced_x86/decoder.hpp\"");
			writer.WriteLine();
			CppConstants.WriteNamespaceBegin(writer, Namespace);
			writer.WriteLine($"// {HandlerCount} handlers (constant data)");
			foreach (var (name, count) in publicTables)
				writer.WriteLine($"extern const HandlerEntry {name}[0x{count:X}];");
			CppConstants.WriteNamespaceEnd(writer, Namespace);
		}
	}
}
