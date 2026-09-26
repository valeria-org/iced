// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Generator.Enums;
using Generator.IO;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Generates the constant instruction info table of a C++ syntax formatter (gas/intel/masm/nasm):
	/// <c>src/formatter/&lt;syntax&gt;/fmt_data.cpp</c> + <c>src/internal/formatter/&lt;syntax&gt;/fmt_data.hpp</c>.
	/// <para>
	/// Rust creates a <c>Box&lt;dyn InstrInfo&gt;</c> (+ <c>String</c>s) per <c>Code</c> at runtime (the ctor kind + args are
	/// deserialized from a table). C++ uses constant data instead: one 8-byte <c>InstrInfo</c> per <c>Code</c> (an
	/// <c>InstrInfoKind</c> + kind specific fields), all strings (incl. the mnemonics with a suffix that Rust creates at runtime)
	/// in <c>STRINGS</c> (C++ <c>FormatterString</c> data, referenced by u16 offsets) and <c>ARGS</c> (u16 values, extra data that
	/// doesn't fit in an <c>InstrInfo</c>). The C++ code dispatches on the kind (a switch) and reads the fields.
	/// </para>
	/// <para>
	/// The subclasses convert a ctor kind + args to an <c>InstrInfo</c>. The layout of each kind must match the C++ code
	/// (the constructors of the instr info classes in <c>src/formatter/&lt;syntax&gt;/fmt_tbl.cpp</c> or <c>info.cpp</c>).
	/// </para>
	/// </summary>
	abstract class CppInstrInfoTableGen {
		protected readonly GenTypes genTypes;
		readonly string syntax;
		readonly FmtInstructionDef[] defs;
		readonly CppFormatterStrings strings = new();
		readonly List<ushort> args = new();
		readonly Dictionary<string, int> argsToIndex = new(StringComparer.Ordinal);
		readonly HashSet<string> kinds;
		readonly string[] kindsArray;

		protected sealed class Entry {
			public readonly string Kind;
			public readonly int Mnemonic;
			public readonly int Arg1;
			public readonly int Arg2;
			public readonly int Arg3;

			public Entry(string kind, int mnemonic, int arg1, int arg2, int arg3) {
				Kind = kind;
				Mnemonic = mnemonic;
				Arg1 = arg1;
				Arg2 = arg2;
				Arg3 = arg3;
			}
		}

		/// <summary>
		/// Reads the args of a ctor (same order as the Rust/C# table deserializers read them)
		/// </summary>
		protected sealed class ArgReader {
			readonly object[] args;
			int index;

			public ArgReader(object[] args) => this.args = args;

			public bool CanRead => index < args.Length;

			object Next() {
				if (index >= args.Length)
					throw new InvalidOperationException();
				return args[index++];
			}

			public string S() => Next() is string s ? s : throw new InvalidOperationException();
			public char C() => Next() is char c ? c : throw new InvalidOperationException();
			public bool B() => Next() is bool b ? b : throw new InvalidOperationException();

			/// <summary>An <c>int</c> or an enum value</summary>
			public uint U() =>
				Next() switch {
					int i => checked((uint)i),
					IEnumValue e => e.Value,
					_ => throw new InvalidOperationException(),
				};
		}

		protected CppInstrInfoTableGen(GenTypes genTypes, string syntax, FmtInstructionDef[] defs, string[] kinds) {
			this.genTypes = genTypes;
			this.syntax = syntax;
			this.defs = defs;
			kindsArray = kinds;
			this.kinds = new HashSet<string>(kinds, StringComparer.Ordinal);
		}

		/// <summary>
		/// Creates the <c>InstrInfo</c> of an instruction
		/// </summary>
		protected abstract Entry Create(FmtInstructionDef def, string ctorKind, ArgReader r);

		protected Entry E(string kind, int mnemonic, int arg1 = 0, int arg2 = 0, int arg3 = 0) {
			if (!kinds.Contains(kind))
				throw new InvalidOperationException($"Unknown kind: {kind}");
			if ((uint)mnemonic > ushort.MaxValue || (uint)arg1 > ushort.MaxValue || (uint)arg2 > ushort.MaxValue || (uint)arg3 > byte.MaxValue)
				throw new InvalidOperationException($"Invalid value: {kind}");
			return new Entry(kind, mnemonic, arg1, arg2, arg3);
		}

		/// <summary>Adds a string and returns its offset</summary>
		protected int Str(string s) => strings.Add(s);

		/// <summary>Adds values to <c>ARGS</c> and returns the index of the first value</summary>
		protected int Args(params int[] values) {
			if (values.Length == 0)
				throw new InvalidOperationException();
			foreach (var v in values) {
				if ((uint)v > ushort.MaxValue)
					throw new InvalidOperationException();
			}
			var key = string.Join(",", values);
			if (argsToIndex.TryGetValue(key, out var index))
				return index;
			index = args.Count;
			foreach (var v in values)
				args.Add((ushort)v);
			if (args.Count > ushort.MaxValue + 1)
				throw new InvalidOperationException();
			argsToIndex.Add(key, index);
			return index;
		}

		/// <summary><c>s + c</c> (<c>c</c> is ignored if it's 0), Rust <c>add_suffix()</c></summary>
		protected static string AddSuffix(string s, char c) => c == '\0' ? s : s + c;

		/// <summary>
		/// Number of mnemonics of a Jcc/SETcc/etc instruction (see C++ <c>get_cc_mnemonics_count()</c>)
		/// </summary>
		protected static int GetCcMnemonicsCount(uint ccIndex) =>
			ccIndex switch {
				0 or 1 or 8 or 9 => 1,
				2 or 3 => 3,
				<= 15 => 2,
				_ => throw new InvalidOperationException(),
			};

		protected static void VerifyCcCount(uint ccIndex, int count) {
			if (GetCcMnemonicsCount(ccIndex) != count)
				throw new InvalidOperationException();
		}

		protected static void VerifyBitness(uint bitness) {
			if (bitness != 16 && bitness != 32 && bitness != 64)
				throw new InvalidOperationException();
		}

		protected static int ToInt(bool b) => b ? 1 : 0;

		string Namespace => $"{CppConstants.InternalNamespace}::{syntax}";

		public void Generate() {
			var codeValues = genTypes[TypeIds.Code].Values;
			if (defs.Length != codeValues.Length)
				throw new InvalidOperationException();
			// The empty string is at offset 0
			if (Str(string.Empty) != 0)
				throw new InvalidOperationException();
			var idConverter = CppIdentifierConverter.Create();
			var entries = new Entry[defs.Length];
			for (int i = 0; i < defs.Length; i++) {
				var def = defs[i];
				if (def.Code.Value != (uint)i)
					throw new InvalidOperationException();
				var r = new ArgReader(def.Args);
				entries[i] = Create(def, def.CtorKind.RawName, r);
				if (r.CanRead)
					throw new InvalidOperationException($"Not all args were read: {def.CtorKind.RawName}");
			}
			var usedKinds = new HashSet<string>(entries.Select(a => a.Kind), StringComparer.Ordinal);
			foreach (var kind in kindsArray) {
				if (!usedKinds.Contains(kind))
					throw new InvalidOperationException($"Unused kind: {kind}");
			}

			var headerFilename = CppConstants.GetInternalFilename(genTypes, "formatter", syntax, "fmt_data.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(headerFilename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include <cstddef>");
				writer.WriteLine("#include <cstdint>");
				writer.WriteLine();
				writer.WriteLine("#include \"iced_x86/iced_constants.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, Namespace);
				writer.WriteLine("/// The kind of an `InstrInfo` (the fields' meaning depend on it)");
				writer.WriteLine("enum class InstrInfoKind : std::uint8_t {");
				using (writer.Indent()) {
					foreach (var kind in kindsArray)
						writer.WriteLine($"{kind},");
				}
				writer.WriteLine("};");
				writer.WriteLine();
				writer.WriteLine("/// Creates the `InstrOpInfo` of an instruction (Rust: `Box<dyn InstrInfo>`). Constant data, one per `Code` value.");
				writer.WriteLine("/// Strings are offsets in `STRINGS` (`FormatterString` data), `ARGS` has extra data (see `kind`)");
				writer.WriteLine("struct InstrInfo {");
				using (writer.Indent()) {
					writer.WriteLine("/// Offset in `STRINGS` of the (first) mnemonic");
					writer.WriteLine("std::uint16_t mnemonic;");
					writer.WriteLine("/// Depends on `kind`");
					writer.WriteLine("std::uint16_t arg1;");
					writer.WriteLine("/// Depends on `kind`");
					writer.WriteLine("std::uint16_t arg2;");
					writer.WriteLine("InstrInfoKind kind;");
					writer.WriteLine("/// Depends on `kind`");
					writer.WriteLine("std::uint8_t arg3;");
				}
				writer.WriteLine("};");
				writer.WriteLine("static_assert(sizeof(InstrInfo) == 8, \"\");");
				writer.WriteLine();
				writer.WriteLine("/// All strings (`FormatterString` data: a length byte followed by the lowercase and the uppercase chars)");
				writer.WriteLine("extern const char STRINGS[];");
				writer.WriteLine("/// Extra data used by some `InstrInfoKind`s");
				writer.WriteLine("extern const std::uint16_t ARGS[];");
				writer.WriteLine("/// The `InstrInfo` of each `Code` value (Rust: `ALL_INFOS`)");
				writer.WriteLine("extern const InstrInfo INSTR_INFOS[IcedConstants::CODE_ENUM_COUNT];");
				CppConstants.WriteNamespaceEnd(writer, Namespace);
			}

			var srcFilename = CppConstants.GetSrcFilename(genTypes, "formatter", syntax, "fmt_data.cpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(srcFilename))) {
				writer.WriteFileHeader();
				writer.WriteLine($"#include \"internal/formatter/{syntax}/fmt_data.hpp\"");
				writer.WriteLine();
				writer.WriteLine("#include \"internal/encoder/const_init.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, Namespace);
				strings.Write(writer, "ICED_CONSTINIT const char", "STRINGS");
				writer.WriteLine();
				writer.WriteLine("// clang-format off");
				writer.WriteLine($"ICED_CONSTINIT const std::uint16_t ARGS[{Math.Max(1, args.Count)}] = {{");
				using (writer.Indent()) {
					if (args.Count == 0)
						writer.WriteLine("0,");
					const int PerLine = 16;
					for (int i = 0; i < args.Count; i += PerLine) {
						var sb = new StringBuilder();
						for (int j = i; j < Math.Min(args.Count, i + PerLine); j++) {
							if (j != i)
								sb.Append(' ');
							sb.Append($"0x{args[j]:X4},");
						}
						writer.Write(sb.ToString());
						writer.WriteCommentLine($"{i}");
					}
				}
				writer.WriteLine("};");
				writer.WriteLine();
				writer.WriteLine("// mnemonic, arg1, arg2, kind, arg3");
				writer.WriteLine("ICED_CONSTINIT const InstrInfo INSTR_INFOS[IcedConstants::CODE_ENUM_COUNT] = {");
				using (writer.Indent()) {
					for (int i = 0; i < entries.Length; i++) {
						var e = entries[i];
						writer.Write($"{{0x{e.Mnemonic:X4}, 0x{e.Arg1:X4}, 0x{e.Arg2:X4}, InstrInfoKind::{e.Kind}, 0x{e.Arg3:X2}}},");
						writer.WriteCommentLine($"{defs[i].Code.ToStringValue(idConverter)}: {defs[i].Mnemonic}");
					}
				}
				writer.WriteLine("};");
				writer.WriteLine("// clang-format on");
				CppConstants.WriteNamespaceEnd(writer, Namespace);
			}
		}
	}
}
