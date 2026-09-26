// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Linq;
using System.Text;
using Generator.Documentation.Cpp;
using Generator.IO;

namespace Generator.Constants.Cpp {
	readonly struct CppConstantsWriter {
		readonly GenTypes genTypes;
		readonly IdentifierConverter idConverter;
		readonly CppDocCommentWriter docWriter;
		readonly CppDeprecatedWriter deprecatedWriter;

		public CppConstantsWriter(GenTypes genTypes, IdentifierConverter idConverter, CppDocCommentWriter docWriter, CppDeprecatedWriter deprecatedWriter) {
			this.genTypes = genTypes;
			this.idConverter = idConverter;
			this.docWriter = docWriter;
			this.deprecatedWriter = deprecatedWriter;
		}

		/// <summary>
		/// Writes a <c>struct Name { static constexpr T X = ...; ... };</c>
		/// </summary>
		public void Write(FileWriter writer, ConstantsType constantsType, Action<FileWriter>? writeExtra = null) {
			docWriter.WriteSummary(writer, constantsType.Documentation.GetComment(TargetLanguage.Cpp), constantsType.RawName);
			writer.WriteLine($"struct {constantsType.Name(idConverter)} {{");
			var sb = new StringBuilder();
			using (writer.Indent()) {
				foreach (var constant in constantsType.Constants) {
					if (ShouldIgnore(constant))
						continue;
					docWriter.WriteSummary(writer, constant.Documentation.GetComment(TargetLanguage.Cpp), constantsType.RawName);
					deprecatedWriter.WriteDeprecated(writer, constant);
					sb.Clear();
					sb.Append("static constexpr ");
					sb.Append(GetType(constant.Kind));
					sb.Append(' ');
					sb.Append(constant.Name(idConverter));
					sb.Append(" = ");
					sb.Append(GetValue(constant));
					sb.Append(';');
					writer.WriteLine(sb.ToString());
				}
				writeExtra?.Invoke(writer);
			}
			writer.WriteLine("};");
		}

		static bool ShouldIgnore(Constant constant) {
			if (constant.DeclaringType.TypeId == TypeIds.SymbolFlags)
				return constant.ValueUInt64 == (ulong)Enums.Formatter.SymbolFlags.HasSymbolSize;
			return false;
		}

		string GetType(ConstantKind kind) =>
			kind switch {
				ConstantKind.Char => "char",
				ConstantKind.String => "const char*",
				ConstantKind.Int32 or ConstantKind.UInt32 => "std::uint32_t",
				ConstantKind.UInt64 => "std::uint64_t",
				ConstantKind.Index => "std::size_t",
				ConstantKind.Register or ConstantKind.MemorySize => EnumUtils.GetEnumType(genTypes, kind).Name(idConverter),
				_ => throw new InvalidOperationException(),
			};

		string GetValue(Constant constant) {
			switch (constant.Kind) {
			case ConstantKind.Char:
				var c = (char)constant.ValueUInt64;
				if (c == '\'' || c == '\\')
					return "'\\" + c.ToString() + "'";
				return "'" + c.ToString() + "'";

			case ConstantKind.String:
				if (constant.RefValue is string s)
					return "\"" + CppConstants.EscapeString(s) + "\"";
				throw new InvalidOperationException();

			case ConstantKind.Int32:
			case ConstantKind.UInt32:
			case ConstantKind.Index:
				if (constant.UseHex)
					return NumberFormatter.FormatHexUInt32WithSep((uint)constant.ValueUInt64).Replace('_', '\'');
				return ((uint)constant.ValueUInt64).ToString();

			case ConstantKind.UInt64:
				if (constant.UseHex)
					return NumberFormatter.FormatHexUInt64WithSep(constant.ValueUInt64).Replace('_', '\'') + "ULL";
				return constant.ValueUInt64.ToString() + "ULL";

			case ConstantKind.Register:
			case ConstantKind.MemorySize:
				var enumType = EnumUtils.GetEnumType(genTypes, constant.Kind);
				var enumValue = enumType.Values.First(a => a.Value == constant.ValueUInt64);
				return idConverter.ToDeclTypeAndValue(enumValue);

			default:
				throw new InvalidOperationException();
			}
		}
	}
}
