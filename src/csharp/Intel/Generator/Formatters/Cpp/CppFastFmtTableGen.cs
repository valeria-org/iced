// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Generator.Enums;
using Generator.Enums.Formatter.Fast;
using Generator.IO;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Generates the fast formatter's constant instruction tables (the other languages serialize them and create
	/// the tables at runtime):
	/// <list type="bullet">
	/// <item><c>MNEMONICS</c>: all mnemonics (a length byte followed by the ASCII chars) + padding</item>
	/// <item><c>MNEMONIC_OFFSETS</c>: offset of each <c>Code</c>'s mnemonic in <c>MNEMONICS</c></item>
	/// <item><c>CODE_FLAGS</c>: <c>FastFmtFlags</c> of each <c>Code</c></item>
	/// </list>
	/// The tables are declared in the public header <c>iced_x86/internal/fast_fmt.hpp</c> (the fast formatter is a template).
	/// </summary>
	sealed class CppFastFmtTableGen {
		// Must match FastStringMnemonic in the C++ fast formatter
		const int FastStringMnemonicSize = 20;
		const string Namespace = CppConstants.InternalNamespace + "::fast";

		readonly GenTypes genTypes;
		readonly FastFmtInstructionDef[] defs;
		readonly IdentifierConverter idConverter;

		public CppFastFmtTableGen(GenTypes genTypes, FastFmtInstructionDef[] defs) {
			this.genTypes = genTypes;
			this.defs = defs;
			idConverter = CppIdentifierConverter.Create();
		}

		public void Generate() {
			var expectedLength = genTypes[TypeIds.Code].Values.Length;
			if (defs.Length != expectedLength)
				throw new InvalidOperationException($"Found {defs.Length} elements, expected {expectedLength}");

			// Mnemonics are stored in the order they're first used (Code order) so the most common
			// instructions' mnemonics (legacy instructions have small Code values) are close to each other.
			var mnemonics = new List<string>();
			var mnemonicToOffset = new Dictionary<string, int>(StringComparer.Ordinal);
			var offsets = new int[defs.Length];
			int size = 0;
			for (int i = 0; i < defs.Length; i++) {
				var def = defs[i];
				if (def.Code.Value != (uint)i)
					throw new InvalidOperationException();
				var mnemonic = def.Mnemonic;
				if (mnemonic.Length == 0 || mnemonic.Length > FastStringMnemonicSize || Encoding.UTF8.GetByteCount(mnemonic) != mnemonic.Length) {
					// Requires updating the C++ fast formatter's `FastStringMnemonic` to match the new aligned size
					throw new InvalidOperationException();
				}
				if (!mnemonicToOffset.TryGetValue(mnemonic, out var offset)) {
					offset = size;
					mnemonicToOffset.Add(mnemonic, offset);
					mnemonics.Add(mnemonic);
					size += 1 + mnemonic.Length;
				}
				offsets[i] = offset;
			}
			int maxLen = mnemonics.Max(a => a.Length);
			int padding = FastStringMnemonicSize - mnemonics[^1].Length;
			int totalSize = size + padding;
			if (totalSize > ushort.MaxValue + 1)
				throw new InvalidOperationException("Offsets must fit in a u16");

			var headerFilename = CppConstants.GetInternalFilename(genTypes, "formatter", "fast", "fmt_data.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(headerFilename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include <cstddef>");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, Namespace);
				writer.WriteLine("// `MNEMONICS`, `MNEMONIC_OFFSETS` and `CODE_FLAGS` are declared in `iced_x86/internal/fast_fmt.hpp`");
				writer.WriteLine();
				writer.WriteLine("/// Number of mnemonics in `MNEMONICS`");
				writer.WriteLine($"constexpr std::size_t MNEMONICS_COUNT = {mnemonics.Count};");
				writer.WriteLine("/// Length of the longest mnemonic");
				writer.WriteLine($"constexpr std::size_t MAX_MNEMONIC_LEN = {maxLen};");
				writer.WriteLine("/// Number of readable bytes after each length byte (`FastStringMnemonic::SIZE`)");
				writer.WriteLine($"constexpr std::size_t MNEMONIC_VALID_STRING_LENGTH = {FastStringMnemonicSize};");
				writer.WriteLine("/// Number of padding bytes after the last mnemonic");
				writer.WriteLine($"constexpr std::size_t MNEMONICS_PADDING_SIZE = {padding};");
				writer.WriteLine("/// Size of `MNEMONICS` in bytes");
				writer.WriteLine($"constexpr std::size_t MNEMONICS_SIZE = {totalSize};");
				CppConstants.WriteNamespaceEnd(writer, Namespace);
			}

			var fastFmtFlags = genTypes[TypeIds.FastFmtFlags];
			var fastFmtFlagsName = fastFmtFlags.Name(idConverter);
			string FlagName(EnumValue value) => $"{fastFmtFlagsName}::{idConverter.Constant(value.RawName)}";

			var srcFilename = CppConstants.GetSrcFilename(genTypes, "formatter", "fast", "fmt_data.cpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(srcFilename))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"internal/formatter/fast/fmt_data.hpp\"");
				writer.WriteLine("#include \"iced_x86/internal/fast_fmt.hpp\"");
				writer.WriteLine("#include \"internal/formatter/fast/fast_fmt_flags.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, Namespace);
				writer.WriteLine("// clang-format off");
				writer.WriteLine("extern const std::uint8_t MNEMONICS[MNEMONICS_SIZE] = {");
				using (writer.Indent()) {
					int offset = 0;
					foreach (var mnemonic in mnemonics) {
						writer.WriteByte((byte)mnemonic.Length);
						foreach (var c in mnemonic)
							writer.WriteByte((byte)c);
						writer.WriteCommentLine($"0x{offset:X4} = \"{mnemonic}\"");
						offset += 1 + mnemonic.Length;
					}
					writer.WriteCommentLine("Padding so it's possible to read FastStringMnemonic::SIZE bytes from the last value");
					if (padding > 0) {
						for (int i = 0; i < padding; i++)
							writer.WriteByte((byte)' ');
						writer.WriteLine();
					}
					else
						writer.WriteCommentLine("No padding needed");
				}
				writer.WriteLine("};");
				writer.WriteLine();
				writer.WriteLine("extern const std::uint16_t MNEMONIC_OFFSETS[IcedConstants::CODE_ENUM_COUNT] = {");
				using (writer.Indent()) {
					for (int i = 0; i < defs.Length; i++)
						writer.WriteLine($"0x{offsets[i]:X4},// {defs[i].Code.Name(idConverter)} = \"{defs[i].Mnemonic}\"");
				}
				writer.WriteLine("};");
				writer.WriteLine();
				writer.WriteLine("extern const std::uint8_t CODE_FLAGS[IcedConstants::CODE_ENUM_COUNT] = {");
				using (writer.Indent()) {
					var noneValue = fastFmtFlags[nameof(FastFmtFlags.None)];
					foreach (var def in defs) {
						var values = def.Flags switch {
							EnumValue value => new[] { value },
							OrEnumValue orValue => orValue.Values,
							_ => throw new InvalidOperationException(),
						};
						uint flagsValue = 0;
						foreach (var value in values) {
							if (value.RawName == nameof(FastFmtFlags.HasVPrefix) || value.RawName == nameof(FastFmtFlags.SameAsPrev))
								throw new InvalidOperationException();
							flagsValue |= value.Value;
						}
						if (flagsValue > byte.MaxValue)
							throw new InvalidOperationException();
						var nonZero = values.Where(a => a.Value != 0).ToArray();
						var s = nonZero.Length == 0 ? FlagName(noneValue) : string.Join(" | ", nonZero.Select(FlagName));
						writer.WriteLine($"{s},// {def.Code.Name(idConverter)}");
					}
				}
				writer.WriteLine("};");
				writer.WriteLine("// clang-format on");
				CppConstants.WriteNamespaceEnd(writer, Namespace);
			}
		}
	}
}
