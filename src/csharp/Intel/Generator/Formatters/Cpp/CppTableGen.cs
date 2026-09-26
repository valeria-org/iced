// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Generator.Constants;
using Generator.Enums;
using Generator.IO;
using Generator.Tables;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Generates the formatter tables that aren't part of the serialized instruction tables:
	/// <list type="bullet">
	/// <item><c>src/internal/formatter/&lt;syntax&gt;/mem_size_tbl_data.hpp</c>: memory size keywords + broadcast data (all 5 formatters)</item>
	/// <item><c>src/internal/formatter/fmt_consts.hpp</c> + <c>src/formatter/fmt_consts.cpp</c>: generated regions (formatter string constants)</item>
	/// <item><c>src/formatter/regs_tbl.cpp</c> + <c>src/internal/formatter/regs_tbl.hpp</c>: register names</item>
	/// <item><c>src/formatter/fmt_flow_control.cpp</c>: <c>get_flow_control()</c></item>
	/// </list>
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppTableGen : TableGen {
		readonly IdentifierConverter idConverter;

		public CppTableGen(GeneratorContext generatorContext)
			: base(generatorContext.Types) {
			idConverter = CppIdentifierConverter.Create();
		}

		protected override void Generate(MemorySizeDef[] defs) {
			var fmtConsts1 = new Dictionary<string, string>(StringComparer.Ordinal);
			var fmtConsts2 = new Dictionary<string, string[]>(StringComparer.Ordinal);
			GenerateFast(defs);
			GenerateGas(defs, fmtConsts1);
			GenerateIntel(defs, fmtConsts1, fmtConsts2);
			GenerateMasm(defs, fmtConsts1, fmtConsts2);
			GenerateNasm(defs, fmtConsts1, fmtConsts2);
			GenerateFmtStrings(fmtConsts1, fmtConsts2);
		}

		void GenerateFmtStrings(Dictionary<string, string> fmtConsts1, Dictionary<string, string[]> fmtConsts2) {
			var consts1 = fmtConsts1.OrderBy(a => a.Key, StringComparer.Ordinal).ToArray();
			var consts2 = fmtConsts2.OrderBy(a => a.Key, StringComparer.Ordinal).ToArray();
			var headerFilename = CppConstants.GetInternalFilename(genTypes, "formatter", "fmt_consts.hpp");
			var srcFilename = CppConstants.GetSrcFilename(genTypes, "formatter", "fmt_consts.cpp");
			new FileUpdater(TargetLanguage.Cpp, "FormatterConstantsDef", headerFilename).Generate(writer => {
				foreach (var kv in consts1)
					writer.WriteLine($"FormatterString {kv.Key};");
			});
			new FileUpdater(TargetLanguage.Cpp, "FormatterConstantsInit", srcFilename).Generate(writer => {
				foreach (var kv in consts1)
					writer.WriteLine($", {kv.Key}(\"{CppConstants.EscapeString(kv.Value)}\")");
			});
			new FileUpdater(TargetLanguage.Cpp, "FormatterArrayConstantsDef", headerFilename).Generate(writer => {
				foreach (var kv in consts2)
					writer.WriteLine($"std::array<const FormatterString*, 2> {kv.Key};");
			});
			new FileUpdater(TargetLanguage.Cpp, "FormatterArrayConstantsInit", srcFilename).Generate(writer => {
				foreach (var kv in consts2)
					writer.WriteLine($", {kv.Key}{{{{{string.Join(", ", kv.Value.Select(a => $"&c.{a}"))}}}}}");
			});
		}

		static void Add(Dictionary<string, string> fmtConsts1, BroadcastToKind bcst) {
			var s = bcst.ToString();
			if (!s.StartsWith("b", StringComparison.Ordinal))
				throw new InvalidOperationException();
			var value = s[1..];
			fmtConsts1[s] = value;
		}

		static void AddKeywords(Dictionary<string, string> fmtConsts1, Dictionary<string, string[]> fmtConsts2, string name) {
			var parts = name.Split('_');
			if (parts.Length > 2)
				throw new InvalidOperationException();
			foreach (var kw in parts)
				fmtConsts1[kw] = kw;
			if (parts.Length == 2)
				fmtConsts2[name] = parts;
		}

		static string GetMemSizeTblNamespace(string syntax) => $"{CppConstants.InternalNamespace}::{syntax}";

		void WriteMemSizeTblHeader(string syntax, bool needConsts, Action<FileWriter> write) {
			var filename = CppConstants.GetInternalFilename(genTypes, "formatter", syntax, "mem_size_tbl_data.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include <cstddef>");
				writer.WriteLine("#include <cstdint>");
				if (needConsts) {
					writer.WriteLine("#include \"internal/formatter/fmt_consts.hpp\"");
					writer.WriteLine("#include \"internal/iced_assert.hpp\"");
				}
				writer.WriteLine();
				var ns = GetMemSizeTblNamespace(syntax);
				CppConstants.WriteNamespaceBegin(writer, ns);
				write(writer);
				CppConstants.WriteNamespaceEnd(writer, ns);
			}
		}

		static void WriteByteArray(FileWriter writer, string name, string elemType, IEnumerable<uint> values, int count, Func<uint, string> toString) {
			writer.WriteLine("// clang-format off");
			writer.WriteLine($"inline constexpr {elemType} {name}[{count}] = {{");
			using (writer.Indent()) {
				foreach (var value in values)
					writer.WriteLine($"{toString(value)},");
			}
			writer.WriteLine("};");
			writer.WriteLine("// clang-format on");
		}

		void WriteBcstToData(FileWriter writer, MemorySizeDef[] defs) {
			var icedConstants = genTypes.GetConstantsType(TypeIds.IcedConstants);
			int first = (int)icedConstants[IcedConstants.FirstBroadcastMemorySizeName].ValueUInt64;
			int len = defs.Length - first;
			writer.WriteLine("/// `BroadcastToKind` of each broadcast `MemorySize` (index = `MemorySize` - `IcedConstants::FIRST_BROADCAST_MEMORY_SIZE`)");
			WriteByteArray(writer, "BCST_TO_DATA", "std::uint8_t", defs.Skip(first).Select(a => checked((uint)(byte)a.BroadcastToKind.Value)), len, a => $"0x{a:X2}");
		}

		void WriteBroadcastToKindSwitch(FileWriter writer) {
			var broadcastToKindValues = genTypes[TypeIds.BroadcastToKind].Values;
			writer.WriteLine("/// Converts a `BroadcastToKind` value to its string (eg. `1to8`)");
			writer.WriteLine("inline const FormatterString& get_bcst_to_string(const FormatterConstants& c, std::uint32_t bcst_to_kind) noexcept {");
			using (writer.Indent()) {
				writer.WriteLine("switch (bcst_to_kind) {");
				foreach (var kw in broadcastToKindValues) {
					var bcst = (BroadcastToKind)kw.Value;
					if (bcst == BroadcastToKind.None)
						writer.WriteLine($"case 0x{kw.Value:X2}: return c.empty;");
					else
						writer.WriteLine($"case 0x{kw.Value:X2}: return c.{kw.RawName};");
				}
				writer.WriteLine("default: ICED_UNREACHABLE();");
				writer.WriteLine("}");
			}
			writer.WriteLine("}");
		}

		void GenerateFast(MemorySizeDef[] defs) {
			var switchValues = defs.Select(a => a.Fast).Distinct().OrderBy(a => a.Value).Select(kw => {
				var s = (FastMemoryKeywords)kw.Value == FastMemoryKeywords.None ? string.Empty : (kw.RawName + "_").Replace('_', ' ');
				return (kw, s);
			}).ToArray();
			var maxMemSizeLen = switchValues.Max(a => a.s.Length);
			// If this fails, the C++ fast formatter must also be updated, see FastStringMemorySize
			const int FastStringMemorySize = 16;
			if (maxMemSizeLen > FastStringMemorySize)
				throw new InvalidOperationException();
			WriteMemSizeTblHeader("fast", false, writer => {
				writer.WriteLine("/// Index into `MEM_SIZE_TBL_STRINGS` of each `MemorySize`");
				WriteByteArray(writer, "MEM_SIZE_TBL_DATA", "std::uint8_t", defs.Select(a => checked((uint)(byte)a.Fast.Value)), defs.Length, a => $"0x{a:X2}");
				writer.WriteLine();
				writer.WriteLine($"constexpr std::size_t MEM_SIZE_TBL_STRINGS_COUNT = {switchValues.Length};");
				writer.WriteLine($"constexpr std::size_t MEM_SIZE_TBL_STRING_SIZE = {FastStringMemorySize};");
				writer.WriteLine("/// Memory keywords: a length byte followed by `MEM_SIZE_TBL_STRING_SIZE` chars (padded with spaces)");
				writer.WriteLine("// clang-format off");
				writer.WriteLine($"inline constexpr char MEM_SIZE_TBL_STRINGS[MEM_SIZE_TBL_STRINGS_COUNT][1 + MEM_SIZE_TBL_STRING_SIZE + 1] = {{");
				using (writer.Indent()) {
					var paddedString = new char[FastStringMemorySize];
					foreach (var (kw, s) in switchValues) {
						for (int i = 0; i < paddedString.Length; i++)
							paddedString[i] = ' ';
						for (int i = 0; i < s.Length; i++)
							paddedString[i] = s[i];
						writer.WriteLine($"\"\\x{s.Length:X2}\" \"{new string(paddedString)}\",");
					}
				}
				writer.WriteLine("};");
				writer.WriteLine("// clang-format on");
				writer.WriteLine($"constexpr std::size_t MAX_MEMORY_SIZE_STR_LEN = {maxMemSizeLen};");
			});
		}

		void GenerateGas(MemorySizeDef[] defs, Dictionary<string, string> fmtConsts1) {
			var broadcastToKindValues = genTypes[TypeIds.BroadcastToKind].Values;
			foreach (var kw in broadcastToKindValues) {
				var bcst = (BroadcastToKind)kw.Value;
				if (bcst != BroadcastToKind.None)
					Add(fmtConsts1, bcst);
			}
			WriteMemSizeTblHeader("gas", true, writer => {
				WriteBcstToData(writer, defs);
				writer.WriteLine();
				WriteBroadcastToKindSwitch(writer);
			});
		}

		void WriteMemoryKeywordsSwitch(FileWriter writer, EnumValue[] keywords, Func<EnumValue, bool> isNone, Dictionary<string, string> fmtConsts1, Dictionary<string, string[]> fmtConsts2) {
			writer.WriteLine("/// Converts a memory keywords value to its keywords (eg. `dword ptr`)");
			writer.WriteLine("inline FormatterStringSlice get_memory_keywords(const FormatterArrayConstants& ac, std::uint32_t memory_keywords) noexcept {");
			using (writer.Indent()) {
				writer.WriteLine("switch (memory_keywords) {");
				foreach (var kw in keywords) {
					if (isNone(kw))
						writer.WriteLine($"case 0x{kw.Value:X2}: return ac.nothing;");
					else {
						AddKeywords(fmtConsts1, fmtConsts2, kw.RawName);
						writer.WriteLine($"case 0x{kw.Value:X2}: return ac.{kw.RawName};");
					}
				}
				writer.WriteLine("default: ICED_UNREACHABLE();");
				writer.WriteLine("}");
			}
			writer.WriteLine("}");
		}

		void GenerateIntel(MemorySizeDef[] defs, Dictionary<string, string> fmtConsts1, Dictionary<string, string[]> fmtConsts2) {
			var intelKeywords = genTypes[TypeIds.IntelMemoryKeywords].Values;
			const int BroadcastToKindShift = 5;
			const int MemoryKeywordsMask = 0x1F;
			var data = defs.Select(def => {
				uint value = def.Intel.Value | (def.BroadcastToKind.Value << BroadcastToKindShift);
				if (value > 0xFF || def.Intel.Value > MemoryKeywordsMask)
					throw new InvalidOperationException();
				return value;
			}).ToArray();
			WriteMemSizeTblHeader("intel", true, writer => {
				writer.WriteLine($"constexpr std::uint32_t {idConverter.Constant(nameof(BroadcastToKindShift))} = {BroadcastToKindShift};");
				writer.WriteLine($"constexpr std::uint32_t {idConverter.Constant(nameof(MemoryKeywordsMask))} = {MemoryKeywordsMask};");
				writer.WriteLine();
				writer.WriteLine("/// Memory keywords (bits 0-4) and `BroadcastToKind` (bits 5-7) of each `MemorySize`");
				WriteByteArray(writer, "MEM_SIZE_TBL_DATA", "std::uint8_t", data, data.Length, a => $"0x{a:X2}");
				writer.WriteLine();
				WriteMemoryKeywordsSwitch(writer, intelKeywords, kw => (IntelMemoryKeywords)kw.Value == IntelMemoryKeywords.None, fmtConsts1, fmtConsts2);
				writer.WriteLine();
				WriteBroadcastToKindSwitch(writer);
			});
		}

		void GenerateMasm(MemorySizeDef[] defs, Dictionary<string, string> fmtConsts1, Dictionary<string, string[]> fmtConsts2) {
			var masmKeywords = genTypes[TypeIds.MasmMemoryKeywords].Values;
			var sizeToIndex = new Dictionary<uint, uint>();
			uint index = 0;
			foreach (var size in defs.Select(a => a.Size).Distinct().OrderBy(a => a))
				sizeToIndex[size] = index++;
			const int SizeKindShift = 5;
			const int MemoryKeywordsMask = 0x1F;
			var data = defs.Select(def => {
				uint value = def.Masm.Value | (sizeToIndex[def.Size] << SizeKindShift);
				if (value > 0xFFFF || def.Masm.Value > MemoryKeywordsMask)
					throw new InvalidOperationException();
				return value;
			}).ToArray();
			WriteMemSizeTblHeader("masm", true, writer => {
				writer.WriteLine($"constexpr std::uint32_t {idConverter.Constant(nameof(SizeKindShift))} = {SizeKindShift};");
				writer.WriteLine($"constexpr std::uint32_t {idConverter.Constant(nameof(MemoryKeywordsMask))} = {MemoryKeywordsMask};");
				writer.WriteLine();
				writer.WriteLine("/// Memory sizes in bytes (index = size kind)");
				var sizes = sizeToIndex.Select(a => a.Key).OrderBy(a => a).ToArray();
				WriteByteArray(writer, "SIZES", "std::uint16_t", sizes, sizes.Length, a => a.ToString());
				writer.WriteLine();
				writer.WriteLine("/// Memory keywords (bits 0-4) and size kind (bits 5-15, index into `SIZES`) of each `MemorySize`");
				WriteByteArray(writer, "MEM_SIZE_TBL_DATA", "std::uint16_t", data, data.Length, a => $"0x{a:X4}");
				writer.WriteLine();
				WriteMemoryKeywordsSwitch(writer, masmKeywords, kw => (MasmMemoryKeywords)kw.Value == MasmMemoryKeywords.None, fmtConsts1, fmtConsts2);
			});
			AddKeywords(fmtConsts1, fmtConsts2, "mmword_ptr");
		}

		void GenerateNasm(MemorySizeDef[] defs, Dictionary<string, string> fmtConsts1, Dictionary<string, string[]> fmtConsts2) {
			var nasmKeywords = genTypes[TypeIds.NasmMemoryKeywords].Values;
			WriteMemSizeTblHeader("nasm", true, writer => {
				WriteBcstToData(writer, defs);
				writer.WriteLine();
				writer.WriteLine("/// Memory keyword of each `MemorySize`");
				WriteByteArray(writer, "MEM_SIZE_TBL_DATA", "std::uint8_t", defs.Select(a => checked((uint)(byte)a.Nasm.Value)), defs.Length, a => $"0x{a:X2}");
				writer.WriteLine();
				writer.WriteLine("/// Converts a memory keyword value to its keyword (eg. `dword`)");
				writer.WriteLine("inline const FormatterString& get_memory_keyword(const FormatterConstants& c, std::uint32_t memory_keyword) noexcept {");
				using (writer.Indent()) {
					writer.WriteLine("switch (memory_keyword) {");
					foreach (var kw in nasmKeywords) {
						if ((NasmMemoryKeywords)kw.Value == NasmMemoryKeywords.None)
							writer.WriteLine($"case 0x{kw.Value:X2}: return c.empty;");
						else {
							AddKeywords(fmtConsts1, fmtConsts2, kw.RawName);
							writer.WriteLine($"case 0x{kw.Value:X2}: return c.{kw.RawName};");
						}
					}
					writer.WriteLine("default: ICED_UNREACHABLE();");
					writer.WriteLine("}");
				}
				writer.WriteLine("}");
				writer.WriteLine();
				WriteBroadcastToKindSwitch(writer);
			});
		}

		protected override void GenerateRegisters(string[] registers) {
			// Must match FastStringRegister in the C++ fast formatter
			const int FastStringRegisterSize = 8;

			foreach (var reg in registers) {
				if (reg.Length > FastStringRegisterSize) {
					// Requires updating the C++ fast formatter's `FastStringRegister` to match the new aligned size
					throw new InvalidOperationException();
				}
			}
			var lastReg = registers[^1];
			int extraPadding = FastStringRegisterSize - lastReg.Length;
			if (extraPadding < 0)
				throw new InvalidOperationException();
			int totalLen = registers.Length + registers.Sum(a => a.Length) + extraPadding;
			int maxLen = registers.Max(a => a.Length);

			const string ns = CppConstants.InternalNamespace + "::regs_tbl";
			var headerFilename = CppConstants.GetInternalFilename(genTypes, "formatter", "regs_tbl.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(headerFilename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include <cstddef>");
				writer.WriteLine("#include <cstdint>");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, ns);
				writer.WriteLine($"constexpr std::size_t MAX_STRING_LENGTH = {maxLen};");
				writer.WriteLine($"constexpr std::size_t VALID_STRING_LENGTH = {FastStringRegisterSize};");
				writer.WriteLine($"constexpr std::size_t PADDING_SIZE = {extraPadding};");
				writer.WriteLine($"constexpr std::size_t REGS_DATA_SIZE = {totalLen};");
				writer.WriteLine("/// Register names (one per `Register` value): a length byte followed by the ASCII chars. The last string is followed by");
				writer.WriteLine("/// padding so it's possible to read `VALID_STRING_LENGTH` bytes from any string.");
				writer.WriteLine("extern const std::uint8_t REGS_DATA[REGS_DATA_SIZE];");
				CppConstants.WriteNamespaceEnd(writer, ns);
			}

			var srcFilename = CppConstants.GetSrcFilename(genTypes, "formatter", "regs_tbl.cpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(srcFilename))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"internal/formatter/regs_tbl.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, ns);
				writer.WriteLine("// clang-format off");
				writer.WriteLine("extern const std::uint8_t REGS_DATA[REGS_DATA_SIZE] = {");
				using (writer.Indent()) {
					foreach (var register in registers) {
						var bytes = Encoding.UTF8.GetBytes(register);
						writer.Write($"0x{bytes.Length:X2}");
						foreach (var b in bytes)
							writer.Write($", 0x{b:X2}");
						writer.Write(",");
						writer.WriteCommentLine(register);
					}
					writer.WriteCommentLine("Padding so it's possible to read FastStringRegister::SIZE bytes from the last value");
					if (extraPadding > 0) {
						for (int i = 0; i < extraPadding; i++)
							writer.WriteByte(0);
						writer.WriteLine();
					}
					else
						writer.WriteCommentLine("No padding needed");
				}
				writer.WriteLine("};");
				writer.WriteLine("// clang-format on");
				CppConstants.WriteNamespaceEnd(writer, ns);
			}
		}

		protected override void GenerateFormatterFlowControl((EnumValue flowCtrl, EnumValue[] code)[] infos) {
			var filename = CppConstants.GetSrcFilename(genTypes, "formatter", "fmt_flow_control.cpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"internal/formatter/fmt_utils.hpp\"");
				writer.WriteLine("#include \"internal/iced_assert.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine("FormatterFlowControl get_flow_control(const Instruction& instruction) noexcept {");
				using (writer.Indent()) {
					writer.WriteLine("// clang-format off");
					writer.WriteLine("switch (instruction.code()) {");
					foreach (var info in infos) {
						if (info.code.Length == 0)
							continue;
						foreach (var c in info.code)
							writer.WriteLine($"case {idConverter.ToDeclTypeAndValue(c)}:");
						using (writer.Indent())
							writer.WriteLine($"return {idConverter.ToDeclTypeAndValue(info.flowCtrl)};");
					}
					writer.WriteLine("default:");
					using (writer.Indent())
						writer.WriteLine("ICED_UNREACHABLE();");
					writer.WriteLine("}");
					writer.WriteLine("// clang-format on");
				}
				writer.WriteLine("}");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}
	}
}
