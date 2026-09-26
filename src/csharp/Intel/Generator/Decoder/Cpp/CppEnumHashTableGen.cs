// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System.Linq;
using Generator.Enums;
using Generator.Tables.Cpp;

namespace Generator.Decoder.Cpp {
	/// <summary>
	/// Generates the string -> enum value tables used by the C++ unit tests (port of the Rust <c>EnumHashTableGen</c>):
	/// <c>tests/generated/from_str_conv_tables.hpp</c> + <c>tests/generated/from_str_conv_tables.cpp</c>
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppEnumHashTableGen {
		readonly IdentifierConverter idConverter;
		readonly GeneratorContext generatorContext;

		public CppEnumHashTableGen(GeneratorContext generatorContext) {
			idConverter = CppIdentifierConverter.Create();
			this.generatorContext = generatorContext;
		}

		public void Generate() {
			var genTypes = generatorContext.Types;
			var infos = new (string name, EnumType enumType, bool lowerCase)[] {
				("CODE_NAME_VALUES", genTypes[TypeIds.Code], false),
				("CPUID_FEATURE_NAME_VALUES", genTypes[TypeIds.CpuidFeature], false),
				("DECODER_ERROR_NAME_VALUES", genTypes[TypeIds.DecoderError], false),
				("DECODER_OPTIONS_NAME_VALUES", genTypes[TypeIds.DecoderOptions], false),
				("ENCODING_KIND_NAME_VALUES", genTypes[TypeIds.EncodingKind], false),
				("FLOW_CONTROL_NAME_VALUES", genTypes[TypeIds.FlowControl], false),
				("MEMORY_SIZE_NAME_VALUES", genTypes[TypeIds.MemorySize], false),
				("MNEMONIC_NAME_VALUES", genTypes[TypeIds.Mnemonic], false),
				("OP_CODE_OPERAND_KIND_NAME_VALUES", genTypes[TypeIds.OpCodeOperandKind], false),
				("REGISTER_NAME_VALUES", genTypes[TypeIds.Register], true),
				("TUPLE_TYPE_NAME_VALUES", genTypes[TypeIds.TupleType], false),
				("CONDITION_CODE_NAME_VALUES", genTypes[TypeIds.ConditionCode], false),
				("MEMORY_SIZE_OPTIONS_NAME_VALUES", genTypes[TypeIds.MemorySizeOptions], false),
				("NUMBER_BASE_NAME_VALUES", genTypes[TypeIds.NumberBase], false),
				("OPTIONS_PROPS_NAME_VALUES", genTypes[TypeIds.OptionsProps], false),
				("CC_B_NAME_VALUES", genTypes[TypeIds.CC_b], false),
				("CC_AE_NAME_VALUES", genTypes[TypeIds.CC_ae], false),
				("CC_E_NAME_VALUES", genTypes[TypeIds.CC_e], false),
				("CC_NE_NAME_VALUES", genTypes[TypeIds.CC_ne], false),
				("CC_BE_NAME_VALUES", genTypes[TypeIds.CC_be], false),
				("CC_A_NAME_VALUES", genTypes[TypeIds.CC_a], false),
				("CC_P_NAME_VALUES", genTypes[TypeIds.CC_p], false),
				("CC_NP_NAME_VALUES", genTypes[TypeIds.CC_np], false),
				("CC_L_NAME_VALUES", genTypes[TypeIds.CC_l], false),
				("CC_GE_NAME_VALUES", genTypes[TypeIds.CC_ge], false),
				("CC_LE_NAME_VALUES", genTypes[TypeIds.CC_le], false),
				("CC_G_NAME_VALUES", genTypes[TypeIds.CC_g], false),
				("MVEX_CONV_FN_NAME_VALUES", genTypes[TypeIds.MvexConvFn], false),
				("MVEX_TUPLE_TYPE_LUT_KIND_NAME_VALUES", genTypes[TypeIds.MvexTupleTypeLutKind], false),
			};
			var tables = infos.Select(a => new CppNameValueTable(a.name, a.enumType, GetNameValues(a.enumType, a.lowerCase))).ToArray();
			new CppNameValueTableWriter(generatorContext, idConverter).Write("from_str_conv_tables", tables);
		}

		static (string name, EnumValue value)[] GetNameValues(EnumType enumType, bool lowerCase) =>
			// Same values and keys as the Rust generator
			enumType.Values.
				Where(a => !a.DeprecatedInfo.IsDeprecatedAndRenamed).
				Where(a => !(a.DeprecatedInfo.IsDeprecated && a.DeprecatedInfo.IsError)).
				Select(a => (lowerCase ? a.RawName.ToLowerInvariant() : a.RawName, a)).
				ToArray();
	}
}
