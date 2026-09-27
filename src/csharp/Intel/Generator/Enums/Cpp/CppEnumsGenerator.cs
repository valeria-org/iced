// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Linq;
using Generator.Constants;
using Generator.Constants.Cpp;
using Generator.Documentation.Cpp;
using Generator.IO;

namespace Generator.Enums.Cpp {
	/// <summary>
	/// Generates one C++ header per enum.
	/// <list type="bullet">
	/// <item>Public enums: <c>include/iced_x86/&lt;name&gt;.hpp</c>, namespace <c>iced_x86</c></item>
	/// <item>Internal enums: <c>src/internal/&lt;component&gt;/&lt;name&gt;.hpp</c>, namespace <c>iced_x86::internal[::sub]</c></item>
	/// <item>Test enums: <c>tests/generated/&lt;name&gt;.hpp</c>, namespace <c>iced_x86::tests</c></item>
	/// </list>
	/// Normal enums are written as <c>enum class X : uintN_t</c>. Flags enums are written as
	/// <c>struct X { static constexpr std::uint32_t A = ...; };</c> (same as the Rust code).
	/// Public (and test) normal enums also get a <c>const char* to_string(X)</c> function whose
	/// implementation is written to <c>src/generated/enum_names.cpp</c> (or <c>tests/generated/test_enum_names.cpp</c>).
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppEnumsGenerator : EnumsGenerator {
		readonly IdentifierConverter idConverter;
		readonly CppDocCommentWriter docWriter;
		readonly CppDeprecatedWriter deprecatedWriter;
		readonly CppConstantsWriter constantsWriter;
		readonly Dictionary<TypeId, EnumFileInfo?> toFileInfo;
		readonly List<(EnumFileInfo info, EnumType enumType)> namedEnums = new();

		enum FileKind {
			Public,
			Internal,
			Test,
		}

		sealed class EnumFileInfo {
			public readonly FileKind Kind;
			public readonly string Filename;
			public readonly string IncludePath;
			public readonly string Namespace;
			public readonly string? UnderlyingType;

			public EnumFileInfo(FileKind kind, string filename, string includePath, string ns, string? underlyingType) {
				Kind = kind;
				Filename = filename;
				IncludePath = includePath;
				Namespace = ns;
				UnderlyingType = underlyingType;
			}
		}

		public CppEnumsGenerator(GeneratorContext generatorContext)
			: base(generatorContext.Types) {
			idConverter = CppIdentifierConverter.Create();
			docWriter = new CppDocCommentWriter(idConverter);
			deprecatedWriter = new CppDeprecatedWriter(idConverter);
			constantsWriter = new CppConstantsWriter(genTypes, idConverter, docWriter, deprecatedWriter);

			toFileInfo = new();

			// Public enums
			Pub(TypeIds.Code, "code");
			Pub(TypeIds.CodeSize, "code_size");
			Pub(TypeIds.ConditionCode, "condition_code");
			Pub(TypeIds.CpuidFeature, "cpuid_feature");
			Pub(TypeIds.DecoderError, "decoder_error");
			Pub(TypeIds.DecoderOptions, "decoder_options");
			Pub(TypeIds.MemorySize, "memory_size");
			Pub(TypeIds.Register, "register");
			Pub(TypeIds.TupleType, "tuple_type");
			Pub(TypeIds.Mnemonic, "mnemonic");
			Pub(TypeIds.MemorySizeOptions, "memory_size_options");
			Pub(TypeIds.NumberBase, "number_base");
			Pub(TypeIds.FormatMnemonicOptions, "format_mnemonic_options");
			Pub(TypeIds.PrefixKind, "prefix_kind");
			Pub(TypeIds.DecoratorKind, "decorator_kind");
			Pub(TypeIds.NumberKind, "number_kind");
			Pub(TypeIds.FormatterTextKind, "formatter_text_kind");
			Pub(TypeIds.SymbolFlags, "symbol_flags");
			Pub(TypeIds.CC_b, "cc_b");
			Pub(TypeIds.CC_ae, "cc_ae");
			Pub(TypeIds.CC_e, "cc_e");
			Pub(TypeIds.CC_ne, "cc_ne");
			Pub(TypeIds.CC_be, "cc_be");
			Pub(TypeIds.CC_a, "cc_a");
			Pub(TypeIds.CC_p, "cc_p");
			Pub(TypeIds.CC_np, "cc_np");
			Pub(TypeIds.CC_l, "cc_l");
			Pub(TypeIds.CC_ge, "cc_ge");
			Pub(TypeIds.CC_le, "cc_le");
			Pub(TypeIds.CC_g, "cc_g");
			Pub(TypeIds.RoundingControl, "rounding_control");
			Pub(TypeIds.OpKind, "op_kind");
			Pub(TypeIds.EncodingKind, "encoding_kind");
			Pub(TypeIds.FlowControl, "flow_control");
			Pub(TypeIds.OpCodeOperandKind, "op_code_operand_kind");
			Pub(TypeIds.RflagsBits, "rflags_bits");
			Pub(TypeIds.OpAccess, "op_access");
			Pub(TypeIds.MandatoryPrefix, "mandatory_prefix");
			Pub(TypeIds.OpCodeTableKind, "op_code_table_kind");
			Pub(TypeIds.RepPrefixKind, "rep_prefix_kind");
			Pub(TypeIds.RelocKind, "reloc_kind");
			Pub(TypeIds.BlockEncoderOptions, "block_encoder_options");
			Pub(TypeIds.MvexConvFn, "mvex_conv_fn");
			Pub(TypeIds.MvexEHBit, "mvex_eh_bit");
			Pub(TypeIds.MvexRegMemConv, "mvex_reg_mem_conv");
			Pub(TypeIds.MvexTupleTypeLutKind, "mvex_tuple_type_lut_kind");
			Add(TypeIds.CodeAsmMemoryOperandSize, FileKind.Public, "code_asm/memory_operand_size", CppConstants.Namespace + "::code_asm");

			// Internal enums: shared
			Int(TypeIds.InstrScale, "instr_scale");
			Int(TypeIds.InstrFlags1, "instr_flags1");
			Int(TypeIds.MvexInstrFlags, "mvex_instr_flags");
			Int(TypeIds.VectorLength, "vector_length", underlyingType: "std::uint32_t");
			Int(TypeIds.MandatoryPrefixByte, "mandatory_prefix_byte", underlyingType: "std::uint32_t");
			Int(TypeIds.MvexInfoFlags1, "mvex_info_flags1");
			Int(TypeIds.MvexInfoFlags2, "mvex_info_flags2");

			// Internal enums: decoder
			Int(TypeIds.HandlerFlags, "decoder/handler_flags");
			Int(TypeIds.LegacyHandlerFlags, "decoder/legacy_handler_flags");
			Int(TypeIds.StateFlags, "decoder/state_flags");
			Int(TypeIds.OpSize, "decoder/op_size");

			// Internal enums: encoder
			Int(TypeIds.LegacyOpCodeTable, "encoder/legacy_op_code_table");
			Int(TypeIds.VexOpCodeTable, "encoder/vex_op_code_table");
			Int(TypeIds.XopOpCodeTable, "encoder/xop_op_code_table");
			Int(TypeIds.EvexOpCodeTable, "encoder/evex_op_code_table");
			Int(TypeIds.MvexOpCodeTable, "encoder/mvex_op_code_table");
			Int(TypeIds.DisplSize, "encoder/displ_size");
			Int(TypeIds.ImmSize, "encoder/imm_size");
			Int(TypeIds.EncoderFlags, "encoder/encoder_flags");
			Int(TypeIds.EncFlags1, "encoder/enc_flags1");
			Int(TypeIds.EncFlags2, "encoder/enc_flags2");
			Int(TypeIds.EncFlags3, "encoder/enc_flags3");
			Int(TypeIds.OpCodeInfoFlags1, "encoder/op_code_info_flags1");
			Int(TypeIds.OpCodeInfoFlags2, "encoder/op_code_info_flags2");
			Int(TypeIds.DecOptionValue, "encoder/dec_option_value");
			Int(TypeIds.InstrStrFmtOption, "encoder/instr_str_fmt_option");
			Int(TypeIds.WBit, "encoder/w_bit");
			Int(TypeIds.LBit, "encoder/l_bit");
			Int(TypeIds.LKind, "encoder/l_kind");

			// Internal enums: instruction info
			Int(TypeIds.CpuidFeatureInternal, "info/cpuid_feature_internal");
			Int(TypeIds.ImpliedAccess, "info/implied_access");
			Int(TypeIds.RflagsInfo, "info/rflags_info");
			Int(TypeIds.OpInfo0, "info/op_info0");
			Int(TypeIds.OpInfo1, "info/op_info1");
			Int(TypeIds.OpInfo2, "info/op_info2");
			Int(TypeIds.OpInfo3, "info/op_info3");
			Int(TypeIds.OpInfo4, "info/op_info4");
			Int(TypeIds.InfoFlags1, "info/info_flags1");
			Int(TypeIds.InfoFlags2, "info/info_flags2");

			// Internal enums: formatters
			Int(TypeIds.PseudoOpsKind, "formatter/pseudo_ops_kind");
			Int(TypeIds.FormatterFlowControl, "formatter/formatter_flow_control");
			Int(TypeIds.GasSizeOverride, "formatter/gas/size_override", "gas");
			Int(TypeIds.GasInstrOpInfoFlags, "formatter/gas/instr_op_info_flags", "gas");
			Int(TypeIds.GasInstrOpKind, "formatter/gas/instr_op_kind", "gas");
			Int(TypeIds.IntelSizeOverride, "formatter/intel/size_override", "intel");
			Int(TypeIds.IntelBranchSizeInfo, "formatter/intel/branch_size_info", "intel");
			Int(TypeIds.IntelInstrOpInfoFlags, "formatter/intel/instr_op_info_flags", "intel");
			Int(TypeIds.IntelInstrOpKind, "formatter/intel/instr_op_kind", "intel");
			Int(TypeIds.MasmInstrOpInfoFlags, "formatter/masm/instr_op_info_flags", "masm");
			Int(TypeIds.MasmInstrOpKind, "formatter/masm/instr_op_kind", "masm");
			Int(TypeIds.NasmSignExtendInfo, "formatter/nasm/sign_extend_info", "nasm");
			Int(TypeIds.NasmSizeOverride, "formatter/nasm/size_override", "nasm");
			Int(TypeIds.NasmBranchSizeInfo, "formatter/nasm/branch_size_info", "nasm");
			Int(TypeIds.NasmInstrOpInfoFlags, "formatter/nasm/instr_op_info_flags", "nasm");
			Int(TypeIds.NasmMemorySizeInfo, "formatter/nasm/memory_size_info", "nasm");
			Int(TypeIds.NasmFarMemorySizeInfo, "formatter/nasm/far_memory_size_info", "nasm");
			Int(TypeIds.NasmInstrOpKind, "formatter/nasm/instr_op_kind", "nasm");
			Int(TypeIds.FastFmtFlags, "formatter/fast/fast_fmt_flags", "fast");

			// Test enums
			Test(TypeIds.DecoderTestOptions, "decoder_test_options");
			Test(TypeIds.MemorySizeFlags, "memory_size_flags");
			Test(TypeIds.RegisterFlags, "register_flags");
			Test(TypeIds.OptionsProps, "options_props");
			Test(TypeIds.TestInstrFlags, "test_instr_flags");
			Test(TypeIds.MasmSymbolTestFlags, "masm_symbol_test_flags");

			// Not used by the C++ code
			toFileInfo.Add(TypeIds.FormatterSyntax, null);
			// The C++ decoder tables are generated constant data (see CppDecoderTableWriter): the handler kinds are the names of the
			// handler factory fns (src/internal/decoder/*_ctors.hpp)
			toFileInfo.Add(TypeIds.SerializedDataKind, null);
			toFileInfo.Add(TypeIds.LegacyOpCodeHandlerKind, null);
			toFileInfo.Add(TypeIds.VexOpCodeHandlerKind, null);
			toFileInfo.Add(TypeIds.EvexOpCodeHandlerKind, null);
			toFileInfo.Add(TypeIds.MvexOpCodeHandlerKind, null);
			// The C++ gas/intel/masm/nasm instruction info tables are generated constant data (see CppInstrInfoTableGen)
			toFileInfo.Add(TypeIds.GasCtorKind, null);
			toFileInfo.Add(TypeIds.IntelCtorKind, null);
			toFileInfo.Add(TypeIds.MasmCtorKind, null);
			toFileInfo.Add(TypeIds.NasmCtorKind, null);
		}

		void Pub(TypeId typeId, string name) => Add(typeId, FileKind.Public, name, CppConstants.Namespace);

		void Int(TypeId typeId, string path, string? subNs = null, string? underlyingType = null) =>
			Add(typeId, FileKind.Internal, path, subNs is null ? CppConstants.InternalNamespace : CppConstants.InternalNamespace + "::" + subNs, underlyingType);

		void Test(TypeId typeId, string name) => Add(typeId, FileKind.Test, "generated/" + name, CppConstants.TestsNamespace);

		void Add(TypeId typeId, FileKind kind, string path, string ns, string? underlyingType = null) {
			var parts = (path + ".hpp").Split('/');
			string filename, includePath;
			switch (kind) {
			case FileKind.Public:
				filename = CppConstants.GetIncludeFilename(genTypes, parts);
				includePath = "iced_x86/" + path + ".hpp";
				break;
			case FileKind.Internal:
				filename = CppConstants.GetInternalFilename(genTypes, parts);
				includePath = "internal/" + path + ".hpp";
				break;
			case FileKind.Test:
				filename = CppConstants.GetTestFilename(genTypes, parts);
				includePath = path + ".hpp";
				break;
			default:
				throw new InvalidOperationException();
			}
			toFileInfo.Add(typeId, new EnumFileInfo(kind, filename, includePath, ns, underlyingType));
		}

		/// <summary>
		/// Gets the include path (eg. <c>"iced_x86/code.hpp"</c> or <c>"internal/decoder/op_size.hpp"</c>) of an enum
		/// </summary>
		public string GetIncludePath(TypeId typeId) =>
			toFileInfo.TryGetValue(typeId, out var info) && info is not null ? info.IncludePath : throw new InvalidOperationException($"Unknown enum {typeId}");

		public override void Generate(EnumType enumType) {
			if (toFileInfo.TryGetValue(enumType.TypeId, out var info)) {
				if (info is not null)
					WriteFile(info, enumType);
			}
			else
				throw new InvalidOperationException($"Missing C++ enum file info: {enumType.TypeId}");
		}

		public override void GenerateEnd() {
			WriteNamesFile(FileKind.Public, CppConstants.GetSrcFilename(genTypes, "generated", "enum_names.cpp"));
			WriteNamesFile(FileKind.Test, CppConstants.GetTestFilename(genTypes, "generated", "test_enum_names.cpp"));
		}

		static bool IsNormalEnum(EnumType enumType) {
			uint expectedValue = 0;
			foreach (var value in enumType.Values) {
				if (value.Value != expectedValue)
					return false;
				expectedValue++;
			}
			return true;
		}

		static EnumValue[] GetValues(EnumType enumType) =>
			// Identical enum values aren't allowed so just remove them
			enumType.Values.Where(a => !a.DeprecatedInfo.IsDeprecatedAndRenamed).ToArray();

		public static string GetUnderlyingType(EnumType enumType) {
			uint maxValue = enumType.Values.Length == 0 ? 0 : enumType.Values.Max(a => a.Value);
			if (maxValue <= byte.MaxValue)
				return "std::uint8_t";
			if (maxValue <= ushort.MaxValue)
				return "std::uint16_t";
			return "std::uint32_t";
		}

		bool HasNames(EnumFileInfo info, EnumType enumType) =>
			!enumType.IsFlags && info.Kind != FileKind.Internal && IsNormalEnum(enumType);

		void WriteFile(EnumFileInfo info, EnumType enumType) {
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(info.Filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include <cstdint>");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, info.Namespace);
				if (enumType.IsFlags)
					constantsWriter.Write(writer, enumType.ToConstantsType(ConstantKind.UInt32));
				else
					WriteEnum(writer, info, enumType);
				CppConstants.WriteNamespaceEnd(writer, info.Namespace);
			}
			if (HasNames(info, enumType))
				namedEnums.Add((info, enumType));
		}

		void WriteEnum(FileWriter writer, EnumFileInfo info, EnumType enumType) {
			docWriter.WriteSummary(writer, enumType.Documentation.GetComment(TargetLanguage.Cpp), enumType.RawName);
			var enumTypeName = enumType.Name(idConverter);
			var underlyingType = info.UnderlyingType ?? GetUnderlyingType(enumType);
			writer.WriteLine($"enum class {enumTypeName} : {underlyingType} {{");
			using (writer.Indent()) {
				foreach (var value in GetValues(enumType)) {
					docWriter.WriteSummary(writer, value.Documentation.GetComment(TargetLanguage.Cpp), enumType.RawName);
					var deprecated = deprecatedWriter.GetAttribute(value);
					var attr = deprecated is null ? string.Empty : " " + deprecated;
					writer.WriteLine($"{value.Name(idConverter)}{attr} = {value.Value},");
				}
			}
			writer.WriteLine("};");

			if (HasNames(info, enumType)) {
				writer.WriteLine();
				writer.WriteLine($"/// Gets the name of a `{enumTypeName}` value (eg. for debugging). Returns an empty string if it's an invalid value.");
				writer.WriteLine($"const char* to_string({enumTypeName} value) noexcept;");
			}
		}

		void WriteNamesFile(FileKind kind, string filename) {
			var enums = namedEnums.Where(a => a.info.Kind == kind).OrderBy(a => a.info.IncludePath, StringComparer.Ordinal).ToArray();
			if (enums.Length == 0)
				return;
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				writer.WriteFileHeader();
				foreach (var (info, _) in enums)
					writer.WriteLine($"#include \"{info.IncludePath}\"");
				writer.WriteLine("#include <cstddef>");
				writer.WriteLine();
				foreach (var group in enums.GroupBy(a => a.info.Namespace)) {
					CppConstants.WriteNamespaceBegin(writer, group.Key);
					bool first = true;
					foreach (var (_, enumType) in group) {
						if (!first)
							writer.WriteLine();
						first = false;
						var enumTypeName = enumType.Name(idConverter);
						var values = enumType.Values;
						writer.WriteLine($"static const char* const {idConverter.Constant("GenNames" + enumType.RawName)}[{values.Length}] = {{");
						using (writer.Indent()) {
							foreach (var value in values)
								writer.WriteLine($"\"{CppConstants.EscapeString(value.Name(idConverter))}\",");
						}
						writer.WriteLine("};");
						writer.WriteLine($"const char* to_string({enumTypeName} value) noexcept {{");
						using (writer.Indent()) {
							writer.WriteLine("const auto index = static_cast<std::size_t>(value);");
							writer.WriteLine($"return index < {values.Length} ? {idConverter.Constant("GenNames" + enumType.RawName)}[index] : \"\";");
						}
						writer.WriteLine("}");
					}
					CppConstants.WriteNamespaceEnd(writer, group.Key);
					writer.WriteLine();
				}
			}
		}
	}
}
