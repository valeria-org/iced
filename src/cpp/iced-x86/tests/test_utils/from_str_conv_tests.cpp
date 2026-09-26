// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_framework.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "generated/masm_symbol_test_flags.hpp"
#include "generated/memory_size_flags.hpp"
#include "generated/register_flags.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/format_mnemonic_options.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/mandatory_prefix.hpp"
#include "iced_x86/op_access.hpp"
#include "iced_x86/op_code_table_kind.hpp"
#include "iced_x86/symbol_flags.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iced_x86;
using namespace iced_x86::tests;

namespace {

// Returns the error message if `f` throws a `std::runtime_error`
template <typename F>
std::optional<std::string> get_error(F&& f) {
	try {
		(void)f();
	}
	catch (const std::runtime_error& ex) {
		return std::string(ex.what());
	}
	return std::nullopt;
}

template <typename E, typename F>
void check_round_trip(std::size_t count, F&& to_value, bool lower_case = false) {
	for (std::size_t i = 0; i < count; i++) {
		const auto value = static_cast<E>(i);
		std::string name = to_string(value);
		REQUIRE(!name.empty());
		if (lower_case)
			name = to_ascii_lowercase(name);
		CHECK_EQ(to_value(name), value);
		CHECK_EQ(to_value("  " + name + "\t"), value);
	}
}

} // namespace

TEST_CASE("test_utils/to_vec_u8") {
	CHECK(to_vec_u8("").empty());
	CHECK(to_vec_u8(" \t ").empty());
	CHECK_EQ(to_vec_u8("12"), (std::vector<std::uint8_t>{0x12}));
	CHECK_EQ(to_vec_u8("12 AB cd"), (std::vector<std::uint8_t>{0x12, 0xAB, 0xCD}));
	CHECK_EQ(to_vec_u8("1 2 3\t4 "), (std::vector<std::uint8_t>{0x12, 0x34}));
	CHECK_EQ(to_vec_u8("\t0F\r\n"), (std::vector<std::uint8_t>{0x0F}));
	CHECK_EQ(to_vec_u8("fFaA09"), (std::vector<std::uint8_t>{0xFF, 0xAA, 0x09}));
	// U+00A0 (NBSP) and U+3000 are whitespace
	CHECK_EQ(to_vec_u8("12\xC2\xA0" "34\xE3\x80\x80" "56"), (std::vector<std::uint8_t>{0x12, 0x34, 0x56}));

	CHECK_EQ(get_error([] { return to_vec_u8("1"); }), std::optional<std::string>("Missing hex digit in string: '1'"));
	CHECK_EQ(get_error([] { return to_vec_u8("123"); }), std::optional<std::string>("Missing hex digit in string: '123'"));
	CHECK_EQ(get_error([] { return to_vec_u8("1G"); }), std::optional<std::string>("Invalid hex string: '1G'"));
	CHECK_EQ(get_error([] { return to_vec_u8("G1"); }), std::optional<std::string>("Invalid hex string: 'G1'"));
	CHECK_EQ(get_error([] { return to_vec_u8("0x12"); }), std::optional<std::string>("Invalid hex string: '0x12'"));
	// Non-ASCII chars are one char (Rust iterates over chars, not bytes)
	CHECK_EQ(get_error([] { return to_vec_u8("1\xC3\xA9"); }), std::optional<std::string>("Invalid hex string: '1\xC3\xA9'"));
	CHECK_EQ(get_error([] { return to_vec_u8("\xC3\xA9"); }), std::optional<std::string>("Missing hex digit in string: '\xC3\xA9'"));
	// Missing digit is checked before invalid digit
	CHECK_EQ(get_error([] { return to_vec_u8("12G"); }), std::optional<std::string>("Missing hex digit in string: '12G'"));
}

TEST_CASE("test_utils/to_u64") {
	CHECK_EQ(to_u64("0"), 0U);
	CHECK_EQ(to_u64(" 123 "), 123U);
	CHECK_EQ(to_u64("+5"), 5U);
	CHECK_EQ(to_u64("0x1F"), 0x1FU);
	CHECK_EQ(to_u64("0x1f"), 0x1FU);
	CHECK_EQ(to_u64("0x+1f"), 0x1FU);
	CHECK_EQ(to_u64("0x0"), 0U);
	CHECK_EQ(to_u64("0xFFFFFFFFFFFFFFFF"), std::numeric_limits<std::uint64_t>::max());
	CHECK_EQ(to_u64("0x0000000000000000FFFFFFFFFFFFFFFF"), std::numeric_limits<std::uint64_t>::max());
	CHECK_EQ(to_u64("18446744073709551615"), std::numeric_limits<std::uint64_t>::max());
	CHECK_EQ(to_u64("\t0x7654321001242B38\r\n"), 0x7654321001242B38U);

	CHECK_EQ(get_error([] { return to_u64("18446744073709551616"); }), std::optional<std::string>("Invalid number: 18446744073709551616"));
	CHECK_EQ(get_error([] { return to_u64("0x10000000000000000"); }), std::optional<std::string>("Invalid number: 0x10000000000000000"));
	CHECK_EQ(get_error([] { return to_u64(" xyz "); }), std::optional<std::string>("Invalid number: xyz"));
	CHECK(get_error([] { return to_u64(""); }));
	CHECK(get_error([] { return to_u64("  "); }));
	CHECK(get_error([] { return to_u64("0x"); }));
	CHECK(get_error([] { return to_u64("0x 5"); }));
	CHECK(get_error([] { return to_u64("0X5"); }));
	CHECK(get_error([] { return to_u64("-1"); }));
	CHECK(get_error([] { return to_u64("+"); }));
	CHECK(get_error([] { return to_u64("++1"); }));
	CHECK(get_error([] { return to_u64("1_000"); }));
	CHECK(get_error([] { return to_u64("12a"); }));
	CHECK(get_error([] { return to_u64("1 2"); }));
	CHECK(get_error([] { return to_u64("0x1G"); }));
}

TEST_CASE("test_utils/to_i64") {
	CHECK_EQ(to_i64("0"), 0);
	CHECK_EQ(to_i64("-0"), 0);
	CHECK_EQ(to_i64("123"), 123);
	CHECK_EQ(to_i64(" -123 "), -123);
	CHECK_EQ(to_i64("-0x10"), -16);
	CHECK_EQ(to_i64("- 5"), -5);
	CHECK_EQ(to_i64("-+5"), -5);
	CHECK_EQ(to_i64("9223372036854775807"), std::numeric_limits<std::int64_t>::max());
	CHECK_EQ(to_i64("-9223372036854775808"), std::numeric_limits<std::int64_t>::min());
	CHECK_EQ(to_i64("-0x8000000000000000"), std::numeric_limits<std::int64_t>::min());
	// Wraps like Rust's `(value as i64).wrapping_mul(mult)`
	CHECK_EQ(to_i64("0xFFFFFFFFFFFFFFFF"), -1);
	CHECK_EQ(to_i64("-0xFFFFFFFFFFFFFFFF"), 1);
	CHECK_EQ(to_i64("9223372036854775808"), std::numeric_limits<std::int64_t>::min());

	// The error message uses the original (untrimmed) string
	CHECK_EQ(get_error([] { return to_i64(" x "); }), std::optional<std::string>("Invalid number:  x "));
	CHECK(get_error([] { return to_i64("--5"); }));
	CHECK(get_error([] { return to_i64("-"); }));
	CHECK(get_error([] { return to_i64(""); }));
	CHECK(get_error([] { return to_i64("- 0x5"); }));
	CHECK(get_error([] { return to_i64("18446744073709551616"); }));
	CHECK(get_error([] { return to_i64("-18446744073709551616"); }));
}

TEST_CASE("test_utils/to_u32_i32_u16_i16_u8_i8") {
	CHECK_EQ(to_u32("4294967295"), 0xFFFFFFFFU);
	CHECK_EQ(to_u32("0xFFFFFFFF"), 0xFFFFFFFFU);
	CHECK_EQ(to_u32(" 12 "), 12U);
	CHECK_EQ(get_error([] { return to_u32(" 4294967296 "); }), std::optional<std::string>("Invalid number: 4294967296"));
	CHECK(get_error([] { return to_u32("0x100000000"); }));
	CHECK(get_error([] { return to_u32("-1"); }));
	CHECK(get_error([] { return to_u32("abc"); }));

	CHECK_EQ(to_i32("2147483647"), std::numeric_limits<std::int32_t>::max());
	CHECK_EQ(to_i32("-2147483648"), std::numeric_limits<std::int32_t>::min());
	CHECK_EQ(to_i32("-0x80000000"), std::numeric_limits<std::int32_t>::min());
	CHECK_EQ(to_i32("-1"), -1);
	CHECK_EQ(get_error([] { return to_i32(" 2147483648 "); }), std::optional<std::string>("Invalid number: 2147483648"));
	CHECK(get_error([] { return to_i32("-2147483649"); }));
	CHECK(get_error([] { return to_i32("0x80000000"); }));
	CHECK(get_error([] { return to_i32("0xFFFFFFFF"); }));
	// Wraps to -1 as an i64 which is a valid i32
	CHECK_EQ(to_i32("0xFFFFFFFFFFFFFFFF"), -1);

	CHECK_EQ(to_u16("65535"), 0xFFFFU);
	CHECK_EQ(to_u16("0xFFFF"), 0xFFFFU);
	CHECK(get_error([] { return to_u16("65536"); }));
	CHECK(get_error([] { return to_u16("-1"); }));

	CHECK_EQ(to_i16("32767"), 32767);
	CHECK_EQ(to_i16("-32768"), -32768);
	CHECK(get_error([] { return to_i16("32768"); }));
	CHECK(get_error([] { return to_i16("-32769"); }));

	CHECK_EQ(to_u8("255"), 0xFFU);
	CHECK_EQ(to_u8("0xFF"), 0xFFU);
	CHECK(get_error([] { return to_u8("256"); }));
	CHECK(get_error([] { return to_u8("0x100"); }));

	CHECK_EQ(to_i8("127"), 127);
	CHECK_EQ(to_i8("-128"), -128);
	CHECK_EQ(to_i8("-0x80"), -128);
	CHECK(get_error([] { return to_i8("128"); }));
	CHECK(get_error([] { return to_i8("-129"); }));
}

TEST_CASE("test_utils/to_enum") {
	CHECK_EQ(to_code("Add_rm8_r8"), Code::Add_rm8_r8);
	CHECK_EQ(to_code(" INVALID "), Code::INVALID);
	CHECK_EQ(get_error([] { return to_code(" xyz "); }), std::optional<std::string>("Invalid Code value: xyz"));
	CHECK(get_error([] { return to_code("add_rm8_r8"); }));

	CHECK_EQ(to_mnemonic("Add"), Mnemonic::Add);
	CHECK_EQ(get_error([] { return to_mnemonic("xyz"); }), std::optional<std::string>("Invalid Mnemonic value: xyz"));

	CHECK_EQ(to_register("eax"), Register::EAX);
	CHECK_EQ(to_register(" xmm31 "), Register::XMM31);
	CHECK_EQ(to_register(""), Register::None);
	CHECK_EQ(to_register("  "), Register::None);
	CHECK_EQ(to_register("none"), Register::None);
	CHECK_EQ(get_error([] { return to_register("EAX"); }), std::optional<std::string>("Invalid Register value: EAX"));

	CHECK_EQ(to_memory_size("UInt8"), MemorySize::UInt8);
	CHECK_EQ(get_error([] { return to_memory_size("xyz"); }), std::optional<std::string>("Invalid MemorySize value: xyz"));

	CHECK_EQ(to_decoder_error("NoMoreBytes"), DecoderError::NoMoreBytes);
	CHECK_EQ(get_error([] { return to_decoder_error("xyz"); }), std::optional<std::string>("Invalid DecoderError value: xyz"));

	CHECK_EQ(to_decoder_options("None"), DecoderOptions::NONE);
	CHECK_EQ(to_decoder_options("AMD"), DecoderOptions::AMD);
	CHECK_EQ(to_decoder_options(" NoInvalidCheck "), DecoderOptions::NO_INVALID_CHECK);
	CHECK_EQ(get_error([] { return to_decoder_options("AMD|NoInvalidCheck"); }), std::optional<std::string>("Invalid DecoderOptions value: AMD|NoInvalidCheck"));

	CHECK_EQ(to_encoding_kind("EVEX"), EncodingKind::EVEX);
	CHECK_EQ(get_error([] { return to_encoding_kind("xyz"); }), std::optional<std::string>("Invalid EncodingKind value: xyz"));

	CHECK_EQ(to_tuple_type("N1"), TupleType::N1);
	CHECK_EQ(get_error([] { return to_tuple_type("xyz"); }), std::optional<std::string>("Invalid TupleType value: xyz"));

	CHECK_EQ(to_mvex_conv_fn("None"), MvexConvFn::None);
	CHECK_EQ(get_error([] { return to_mvex_conv_fn("xyz"); }), std::optional<std::string>("Invalid MvexConvFn value: xyz"));

	CHECK_EQ(to_mvex_tuple_type_lut_kind("Int32"), MvexTupleTypeLutKind::Int32);
	CHECK_EQ(get_error([] { return to_mvex_tuple_type_lut_kind("xyz"); }), std::optional<std::string>("Invalid MvexTupleTypeLutKind value: xyz"));

	CHECK_EQ(to_cpuid_features("AVX"), CpuidFeature::AVX);
	CHECK_EQ(get_error([] { return to_cpuid_features("xyz"); }), std::optional<std::string>("Invalid CpuidFeature value: xyz"));

	CHECK_EQ(to_flow_control("Call"), FlowControl::Call);
	CHECK_EQ(get_error([] { return to_flow_control("xyz"); }), std::optional<std::string>("Invalid FlowControl value: xyz"));

	CHECK_EQ(to_op_code_operand_kind("r8_or_mem"), OpCodeOperandKind::r8_or_mem);
	CHECK_EQ(get_error([] { return to_op_code_operand_kind("xyz"); }), std::optional<std::string>("Invalid OpCodeOperandKind value: xyz"));

	CHECK_EQ(to_condition_code("e"), ConditionCode::e);
	CHECK_EQ(get_error([] { return to_condition_code("xyz"); }), std::optional<std::string>("Invalid ConditionCode value: xyz"));

	CHECK_EQ(to_options_props("IP"), OptionsProps::IP);
	CHECK_EQ(to_options_props("CC_b"), OptionsProps::CC_b);
	CHECK_EQ(get_error([] { return to_options_props("xyz"); }), std::optional<std::string>("Invalid OptionsProps value: xyz"));

	CHECK_EQ(to_memory_size_options("Always"), MemorySizeOptions::Always);
	CHECK_EQ(get_error([] { return to_memory_size_options("xyz"); }), std::optional<std::string>("Invalid MemorySizeOptions value: xyz"));

	CHECK_EQ(to_number_base("Hexadecimal"), NumberBase::Hexadecimal);
	CHECK_EQ(get_error([] { return to_number_base("xyz"); }), std::optional<std::string>("Invalid NumberBase value: xyz"));
	CHECK_EQ(number_base_len(), IcedConstants::NUMBER_BASE_ENUM_COUNT);

	CHECK_EQ(to_boolean("true"), true);
	CHECK_EQ(to_boolean(" false "), false);
	CHECK_EQ(get_error([] { return to_boolean("True"); }), std::optional<std::string>("Invalid boolean value: True"));
	CHECK(get_error([] { return to_boolean("1"); }));

	CHECK_EQ(to_cc_b("b"), CC_b::b);
	CHECK_EQ(to_cc_b("c"), CC_b::c);
	CHECK_EQ(to_cc_b("nae"), CC_b::nae);
	CHECK_EQ(to_cc_ae("nc"), CC_ae::nc);
	CHECK_EQ(to_cc_e("z"), CC_e::z);
	CHECK_EQ(to_cc_ne("nz"), CC_ne::nz);
	CHECK_EQ(to_cc_be("na"), CC_be::na);
	CHECK_EQ(to_cc_a("nbe"), CC_a::nbe);
	CHECK_EQ(to_cc_p("pe"), CC_p::pe);
	CHECK_EQ(to_cc_np("po"), CC_np::po);
	CHECK_EQ(to_cc_l("nge"), CC_l::nge);
	CHECK_EQ(to_cc_ge("nl"), CC_ge::nl);
	CHECK_EQ(to_cc_le("ng"), CC_le::ng);
	CHECK_EQ(to_cc_g("nle"), CC_g::nle);
	CHECK_EQ(get_error([] { return to_cc_b("xyz"); }), std::optional<std::string>("Invalid CC_b value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_ae("xyz"); }), std::optional<std::string>("Invalid CC_ae value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_e("xyz"); }), std::optional<std::string>("Invalid CC_e value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_ne("xyz"); }), std::optional<std::string>("Invalid CC_ne value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_be("xyz"); }), std::optional<std::string>("Invalid CC_be value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_a("xyz"); }), std::optional<std::string>("Invalid CC_a value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_p("xyz"); }), std::optional<std::string>("Invalid CC_p value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_np("xyz"); }), std::optional<std::string>("Invalid CC_np value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_l("xyz"); }), std::optional<std::string>("Invalid CC_l value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_ge("xyz"); }), std::optional<std::string>("Invalid CC_ge value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_le("xyz"); }), std::optional<std::string>("Invalid CC_le value: xyz"));
	CHECK_EQ(get_error([] { return to_cc_g("xyz"); }), std::optional<std::string>("Invalid CC_g value: xyz"));
}

TEST_CASE("test_utils/to_enum_round_trip") {
	check_round_trip<Code>(IcedConstants::CODE_ENUM_COUNT, to_code);
	check_round_trip<Register>(IcedConstants::REGISTER_ENUM_COUNT, to_register, true);
	check_round_trip<Mnemonic>(IcedConstants::MNEMONIC_ENUM_COUNT, to_mnemonic);
	check_round_trip<MemorySize>(IcedConstants::MEMORY_SIZE_ENUM_COUNT, to_memory_size);
	check_round_trip<DecoderError>(IcedConstants::DECODER_ERROR_ENUM_COUNT, to_decoder_error);
	check_round_trip<EncodingKind>(IcedConstants::ENCODING_KIND_ENUM_COUNT, to_encoding_kind);
	check_round_trip<TupleType>(IcedConstants::TUPLE_TYPE_ENUM_COUNT, to_tuple_type);
	check_round_trip<MvexConvFn>(IcedConstants::MVEX_CONV_FN_ENUM_COUNT, to_mvex_conv_fn);
	check_round_trip<MvexTupleTypeLutKind>(IcedConstants::MVEX_TUPLE_TYPE_LUT_KIND_ENUM_COUNT, to_mvex_tuple_type_lut_kind);
	check_round_trip<CpuidFeature>(IcedConstants::CPUID_FEATURE_ENUM_COUNT, to_cpuid_features);
	check_round_trip<FlowControl>(IcedConstants::FLOW_CONTROL_ENUM_COUNT, to_flow_control);
	check_round_trip<OpCodeOperandKind>(IcedConstants::OP_CODE_OPERAND_KIND_ENUM_COUNT, to_op_code_operand_kind);
	check_round_trip<ConditionCode>(IcedConstants::CONDITION_CODE_ENUM_COUNT, to_condition_code);
	check_round_trip<OptionsProps>(OPTIONS_PROPS_NAME_VALUES.size(), to_options_props);
	check_round_trip<MemorySizeOptions>(IcedConstants::MEMORY_SIZE_OPTIONS_ENUM_COUNT, to_memory_size_options);
	check_round_trip<NumberBase>(IcedConstants::NUMBER_BASE_ENUM_COUNT, to_number_base);
	check_round_trip<CC_b>(IcedConstants::CC_B_ENUM_COUNT, to_cc_b);
	check_round_trip<CC_ae>(IcedConstants::CC_AE_ENUM_COUNT, to_cc_ae);
	check_round_trip<CC_e>(IcedConstants::CC_E_ENUM_COUNT, to_cc_e);
	check_round_trip<CC_ne>(IcedConstants::CC_NE_ENUM_COUNT, to_cc_ne);
	check_round_trip<CC_be>(IcedConstants::CC_BE_ENUM_COUNT, to_cc_be);
	check_round_trip<CC_a>(IcedConstants::CC_A_ENUM_COUNT, to_cc_a);
	check_round_trip<CC_p>(IcedConstants::CC_P_ENUM_COUNT, to_cc_p);
	check_round_trip<CC_np>(IcedConstants::CC_NP_ENUM_COUNT, to_cc_np);
	check_round_trip<CC_l>(IcedConstants::CC_L_ENUM_COUNT, to_cc_l);
	check_round_trip<CC_ge>(IcedConstants::CC_GE_ENUM_COUNT, to_cc_ge);
	check_round_trip<CC_le>(IcedConstants::CC_LE_ENUM_COUNT, to_cc_le);
	check_round_trip<CC_g>(IcedConstants::CC_G_ENUM_COUNT, to_cc_g);

	// Every table entry is a valid name (the tables can also contain aliases)
	CHECK_EQ(CODE_NAME_VALUES.size(), IcedConstants::CODE_ENUM_COUNT);
	CHECK_EQ(REGISTER_NAME_VALUES.size(), IcedConstants::REGISTER_ENUM_COUNT);
	CHECK_EQ(MNEMONIC_NAME_VALUES.size(), IcedConstants::MNEMONIC_ENUM_COUNT);
	CHECK_EQ(MEMORY_SIZE_NAME_VALUES.size(), IcedConstants::MEMORY_SIZE_ENUM_COUNT);
	CHECK_EQ(OPTIONS_PROPS_NAME_VALUES.size(), static_cast<std::size_t>(OptionsProps::ShowUselessPrefixes) + 1);
}

TEST_CASE("test_utils/code_names") {
	const auto names = code_names();
	REQUIRE_EQ(names.size(), IcedConstants::CODE_ENUM_COUNT);
	for (std::size_t i = 0; i < names.size(); i++)
		CHECK_EQ(std::string(names[i]), std::string(to_string(static_cast<Code>(i))));
	CHECK_EQ(std::string(names[0]), "INVALID");
}

TEST_CASE("test_utils/is_ignored_code") {
	CHECK(!is_ignored_code("Add_rm8_r8"));
	CHECK(!is_ignored_code("VEX_Vmaskmovdqu_rDI_xmm_xmm"));
	CHECK(!is_ignored_code("EVEX_Vaddps_xmm_k1z_xmm_xmmm128b32"));
	CHECK(!is_ignored_code("XOP_Vpcmov_xmm_xmm_xmmm128_xmm"));
	CHECK(!is_ignored_code("D3NOW_Pfadd_mm_mmm64"));
	CHECK(!is_ignored_code("MVEX_Vprefetchnta_m"));
	CHECK(!is_ignored_code("xyz"));
	for (const char* name : IGNORED_CODE_NAMES) {
		CHECK(is_ignored_code(name));
		CHECK(is_ignored_code(std::string(" ") + name + " "));
	}
}

TEST_CASE("test_utils/clone_register_hashmap") {
	const auto map = clone_register_hashmap();
	CHECK_EQ(map.size(), IcedConstants::REGISTER_ENUM_COUNT);
	const auto it = map.find("rax");
	REQUIRE(it != map.end());
	CHECK_EQ(it->second, Register::RAX);
	CHECK(map.find("RAX") == map.end());
}

TEST_CASE("test_utils/dicts") {
	const auto op_access = create_dict(OP_ACCESS_DICT);
	CHECK_EQ(op_access.size(), IcedConstants::OP_ACCESS_ENUM_COUNT);
	CHECK_EQ(op_access.at("n"), OpAccess::None);
	CHECK_EQ(op_access.at("r"), OpAccess::Read);
	CHECK_EQ(op_access.at("cr"), OpAccess::CondRead);
	CHECK_EQ(op_access.at("w"), OpAccess::Write);
	CHECK_EQ(op_access.at("cw"), OpAccess::CondWrite);
	CHECK_EQ(op_access.at("rw"), OpAccess::ReadWrite);
	CHECK_EQ(op_access.at("rcw"), OpAccess::ReadCondWrite);
	CHECK_EQ(op_access.at("nma"), OpAccess::NoMemAccess);

	const auto mem_flags = create_dict(MEMORY_SIZE_FLAGS_DICT);
	CHECK_EQ(mem_flags.size(), 3U);
	CHECK_EQ(mem_flags.at("signed"), MemorySizeFlags::SIGNED);
	CHECK_EQ(mem_flags.at("bcst"), MemorySizeFlags::BROADCAST);
	CHECK_EQ(mem_flags.at("packed"), MemorySizeFlags::PACKED);

	const auto reg_flags = create_dict(REGISTER_FLAGS_DICT);
	CHECK_EQ(reg_flags.size(), REGISTER_FLAGS_DICT.size());
	CHECK_EQ(reg_flags.at("gpr64"), RegisterFlags::GPR64);
	CHECK_EQ(reg_flags.at("xmm"), RegisterFlags::XMM);
	CHECK_EQ(reg_flags.at("tmm"), RegisterFlags::TMM);

	const auto encoding = create_dict(ENCODING_KIND_DICT);
	CHECK_EQ(encoding.size(), IcedConstants::ENCODING_KIND_ENUM_COUNT);
	CHECK_EQ(encoding.at("legacy"), EncodingKind::Legacy);
	CHECK_EQ(encoding.at("3DNow!"), EncodingKind::D3NOW);
	CHECK_EQ(encoding.at("MVEX"), EncodingKind::MVEX);

	const auto mandatory_prefix = create_dict(MANDATORY_PREFIX_DICT);
	CHECK_EQ(mandatory_prefix.size(), IcedConstants::MANDATORY_PREFIX_ENUM_COUNT);
	CHECK_EQ(mandatory_prefix.at("NP"), MandatoryPrefix::PNP);
	CHECK_EQ(mandatory_prefix.at("66"), MandatoryPrefix::P66);

	const auto table_kind = create_dict(OP_CODE_TABLE_KIND_DICT);
	CHECK_EQ(table_kind.size(), IcedConstants::OP_CODE_TABLE_KIND_ENUM_COUNT);
	CHECK_EQ(table_kind.at("0F38"), OpCodeTableKind::T0F38);
	CHECK_EQ(table_kind.at("XA"), OpCodeTableKind::MAP10);

	const auto masm_flags = create_dict(MASM_SYMBOL_TEST_FLAGS_DICT);
	CHECK_EQ(masm_flags.size(), 7U);
	CHECK_EQ(masm_flags.at("sym"), SymbolTestFlags::SYMBOL);
	CHECK_EQ(masm_flags.at("nods32"), SymbolTestFlags::NO_ADD_DS_PREFIX32);

	const auto fmt_mnemonic = create_dict(FORMAT_MNEMONIC_OPTIONS_DICT);
	CHECK_EQ(fmt_mnemonic.size(), 2U);
	CHECK_EQ(fmt_mnemonic.at("noprefixes"), FormatMnemonicOptions::NO_PREFIXES);
	CHECK_EQ(fmt_mnemonic.at("nomnemonic"), FormatMnemonicOptions::NO_MNEMONIC);

	const auto symbol_flags = create_dict(SYMBOL_FLAGS_DICT);
	CHECK_EQ(symbol_flags.size(), 2U);
	CHECK_EQ(symbol_flags.at("rel"), SymbolFlags::RELATIVE);
	CHECK_EQ(symbol_flags.at("signed"), SymbolFlags::SIGNED);

	const auto ignored = create_set(IGNORED_CODE_NAMES);
	CHECK_EQ(ignored.size(), IGNORED_CODE_NAMES.size());
}

TEST_CASE("test_utils/get_default_ip") {
	CHECK_EQ(get_default_ip(16), 0x7FF0U);
	CHECK_EQ(get_default_ip(32), 0x7FFF'FFF0U);
	CHECK_EQ(get_default_ip(64), 0x7FFF'FFFF'FFFF'FFF0ULL);
	CHECK(get_error([] { return get_default_ip(8); }));
}
