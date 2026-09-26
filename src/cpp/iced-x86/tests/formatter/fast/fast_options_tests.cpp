// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/fast/tests/options.rs

#include <cstdint>
#include <string>
#include <vector>

#include "formatter/fast/fast_test_utils.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/fast_formatter.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/fast/options/test_fmt_factory/test_options_common") {
	test_format_file_common_fast<DefaultFastFormatterTraitOptions>("Fast", "OptionsResult.Common", fast_create_options<DefaultFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/options/test_fmt_factory/test_options2") {
	test_format_file_fast<DefaultFastFormatterTraitOptions>("Fast", "OptionsResult2", "Options2", fast_create_options<DefaultFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/options/test_not_fmt_factory/test_options_common") {
	test_format_file_common_fast<NotFastFormatterTraitOptions>("Fast", "OptionsResult.Common", fast_create_options<NotFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/options/test_not_fmt_factory/test_options2") {
	test_format_file_fast<NotFastFormatterTraitOptions>("Fast", "OptionsResult2", "Options2", fast_create_options<NotFastFormatterTraitOptions>);
}

static void invert_options(FastFormatterOptions& options) {
	options.set_space_after_operand_separator(options.space_after_operand_separator() ^ true);
	options.set_rip_relative_addresses(options.rip_relative_addresses() ^ true);
	options.set_use_pseudo_ops(options.use_pseudo_ops() ^ true);
	options.set_show_symbol_address(options.show_symbol_address() ^ true);
	options.set_always_show_segment_register(options.always_show_segment_register() ^ true);
	options.set_always_show_memory_size(options.always_show_memory_size() ^ true);
	options.set_uppercase_hex(options.uppercase_hex() ^ true);
	options.set_use_hex_prefix(options.use_hex_prefix() ^ true);
}

namespace {
struct MyTraitOptions : SpecializedFormatterTraitOptions {};
} // namespace

template <typename TraitOptions>
static void check_default_specialized_trait_options() {
	using MyFormatter = SpecializedFormatter<TraitOptions>;

	MyFormatter formatter;
	auto& options = formatter.options_mut();

	for (int i = 0; i < 2; i++) {
		CHECK_EQ(TraitOptions::INTERNAL_IS_FAST_FORMATTER, false);
		CHECK_EQ(TraitOptions::ENABLE_SYMBOL_RESOLVER, false);
		CHECK_EQ(TraitOptions::ENABLE_DB_DW_DD_DQ, false);
		CHECK_EQ(TraitOptions::verify_output_has_enough_bytes_left(), true);
		CHECK_EQ(TraitOptions::space_after_operand_separator(options), false);
		CHECK_EQ(TraitOptions::rip_relative_addresses(options), true);
		CHECK_EQ(TraitOptions::use_pseudo_ops(options), false);
		CHECK_EQ(TraitOptions::show_symbol_address(options), false);
		CHECK_EQ(TraitOptions::always_show_segment_register(options), false);
		CHECK_EQ(TraitOptions::always_show_memory_size(options), false);
		CHECK_EQ(TraitOptions::uppercase_hex(options), true);
		CHECK_EQ(TraitOptions::use_hex_prefix(options), true);

		invert_options(options);
	}
}

TEST_CASE("formatter/fast/options/test_specialized_formatter_trait_options") { check_default_specialized_trait_options<MyTraitOptions>(); }

TEST_CASE("formatter/fast/options/test_default_specialized_formatter_trait_options") {
	check_default_specialized_trait_options<DefaultSpecializedFormatterTraitOptions>();
}

TEST_CASE("formatter/fast/options/test_default_fast_formatter_trait_options") {
	using MyFormatter = SpecializedFormatter<DefaultFastFormatterTraitOptions>;
	using TraitOptions = DefaultFastFormatterTraitOptions;

	MyFormatter formatter;
	auto& options = formatter.options_mut();

	CHECK_EQ(TraitOptions::INTERNAL_IS_FAST_FORMATTER, true);
	CHECK_EQ(TraitOptions::ENABLE_SYMBOL_RESOLVER, true);
	CHECK_EQ(TraitOptions::ENABLE_DB_DW_DD_DQ, true);
	CHECK_EQ(TraitOptions::verify_output_has_enough_bytes_left(), true);
	CHECK_EQ(TraitOptions::space_after_operand_separator(options), false);
	CHECK_EQ(TraitOptions::rip_relative_addresses(options), false);
	CHECK_EQ(TraitOptions::use_pseudo_ops(options), true);
	CHECK_EQ(TraitOptions::show_symbol_address(options), false);
	CHECK_EQ(TraitOptions::always_show_segment_register(options), false);
	CHECK_EQ(TraitOptions::always_show_memory_size(options), false);
	CHECK_EQ(TraitOptions::uppercase_hex(options), true);
	CHECK_EQ(TraitOptions::use_hex_prefix(options), false);

	invert_options(options);

	CHECK_EQ(TraitOptions::ENABLE_SYMBOL_RESOLVER, true);
	CHECK_EQ(TraitOptions::ENABLE_DB_DW_DD_DQ, true);
	CHECK_EQ(TraitOptions::verify_output_has_enough_bytes_left(), true);
	CHECK_EQ(TraitOptions::space_after_operand_separator(options), true);
	CHECK_EQ(TraitOptions::rip_relative_addresses(options), true);
	CHECK_EQ(TraitOptions::use_pseudo_ops(options), false);
	CHECK_EQ(TraitOptions::show_symbol_address(options), true);
	CHECK_EQ(TraitOptions::always_show_segment_register(options), true);
	CHECK_EQ(TraitOptions::always_show_memory_size(options), true);
	CHECK_EQ(TraitOptions::uppercase_hex(options), false);
	CHECK_EQ(TraitOptions::use_hex_prefix(options), true);
}

namespace {
template <bool SPACE_AFTER_OPERAND_SEPARATOR, bool RIP_RELATIVE_ADDRESSES, bool USE_PSEUDO_OPS, bool SHOW_SYMBOL_ADDRESS,
		  bool ALWAYS_SHOW_SEGMENT_REGISTER, bool ALWAYS_SHOW_MEMORY_SIZE, bool UPPERCASE_HEX, bool USE_HEX_PREFIX>
struct HardCodedTraitOptions : SpecializedFormatterTraitOptions {
	static constexpr bool space_after_operand_separator(const FastFormatterOptions&) noexcept { return SPACE_AFTER_OPERAND_SEPARATOR; }
	static constexpr bool rip_relative_addresses(const FastFormatterOptions&) noexcept { return RIP_RELATIVE_ADDRESSES; }
	static constexpr bool use_pseudo_ops(const FastFormatterOptions&) noexcept { return USE_PSEUDO_OPS; }
	static constexpr bool show_symbol_address(const FastFormatterOptions&) noexcept { return SHOW_SYMBOL_ADDRESS; }
	static constexpr bool always_show_segment_register(const FastFormatterOptions&) noexcept { return ALWAYS_SHOW_SEGMENT_REGISTER; }
	static constexpr bool always_show_memory_size(const FastFormatterOptions&) noexcept { return ALWAYS_SHOW_MEMORY_SIZE; }
	static constexpr bool uppercase_hex(const FastFormatterOptions&) noexcept { return UPPERCASE_HEX; }
	static constexpr bool use_hex_prefix(const FastFormatterOptions&) noexcept { return USE_HEX_PREFIX; }
};

template <bool SPACE_AFTER_OPERAND_SEPARATOR, bool RIP_RELATIVE_ADDRESSES, bool USE_PSEUDO_OPS, bool SHOW_SYMBOL_ADDRESS,
		  bool ALWAYS_SHOW_SEGMENT_REGISTER, bool ALWAYS_SHOW_MEMORY_SIZE, bool UPPERCASE_HEX, bool USE_HEX_PREFIX>
void test_option(const std::vector<std::uint8_t>& bytes, const char* disasm) {
	using MyTraitOptions2 = HardCodedTraitOptions<SPACE_AFTER_OPERAND_SEPARATOR, RIP_RELATIVE_ADDRESSES, USE_PSEUDO_OPS, SHOW_SYMBOL_ADDRESS,
												  ALWAYS_SHOW_SEGMENT_REGISTER, ALWAYS_SHOW_MEMORY_SIZE, UPPERCASE_HEX, USE_HEX_PREFIX>;
	using MyFormatter = SpecializedFormatter<MyTraitOptions2>;

	const auto instr = Decoder::with_ip(64, bytes.data(), bytes.size(), 0x1234'5678'9ABC'DEF1ULL, DecoderOptions::NONE).decode();

	MyFormatter formatter;
	std::string output;

	for (int i = 0; i < 2; i++) {
		output.clear();
		formatter.format(instr, output);
		CHECK_EQ(output, std::string(disasm));
		invert_options(formatter.options_mut());
	}
}
} // namespace

TEST_CASE("formatter/fast/options/test_specialized_fmt_does_not_call_options_methods") {
	const std::vector<std::uint8_t> add_rax = {0x48, 0x05, 0xA5, 0x5A, 0x34, 0x82};
	const std::vector<std::uint8_t> mov_rcx_rip = {0x48, 0x8B, 0x0D, 0x78, 0x56, 0x34, 0x12};
	const std::vector<std::uint8_t> mov_rcx_eip = {0x67, 0x48, 0x8B, 0x0D, 0x78, 0x56, 0x34, 0x12};
	const std::vector<std::uint8_t> cmpps = {0x0F, 0xC2, 0xCD, 0x01};

	// opt=false, other=false
	// opt=false, other=true
	// opt=true , other=false
	// opt=true , other=true

	// space_after_operand_separator
	test_option<false, false, false, false, false, false, false, false>(add_rax, "add rax,0ffffffff82345aa5h");
	test_option<false, true, true, true, true, true, true, true>(add_rax, "add rax,0xFFFFFFFF82345AA5");
	test_option<true, false, false, false, false, false, false, false>(add_rax, "add rax, 0ffffffff82345aa5h");
	test_option<true, true, true, true, true, true, true, true>(add_rax, "add rax, 0xFFFFFFFF82345AA5");
	// uppercase_hex
	test_option<false, false, false, false, false, false, false, false>(add_rax, "add rax,0ffffffff82345aa5h");
	test_option<true, true, true, true, true, true, false, true>(add_rax, "add rax, 0xffffffff82345aa5");
	test_option<false, false, false, false, false, false, true, false>(add_rax, "add rax,0FFFFFFFF82345AA5h");
	test_option<true, true, true, true, true, true, true, true>(add_rax, "add rax, 0xFFFFFFFF82345AA5");
	// use_hex_prefix
	test_option<false, false, false, false, false, false, false, false>(add_rax, "add rax,0ffffffff82345aa5h");
	test_option<true, true, true, true, true, true, true, false>(add_rax, "add rax, 0FFFFFFFF82345AA5h");
	test_option<false, false, false, false, false, false, false, true>(add_rax, "add rax,0xffffffff82345aa5");
	test_option<true, true, true, true, true, true, true, true>(add_rax, "add rax, 0xFFFFFFFF82345AA5");
	// rip_relative_addresses (64-bit)
	test_option<false, false, false, false, false, false, false, false>(mov_rcx_rip, "mov rcx,[12345678acf13570h]");
	test_option<true, false, true, true, true, true, true, true>(mov_rcx_rip, "mov rcx, qword ptr ds:[0x12345678ACF13570]");
	test_option<false, true, false, false, false, false, false, false>(mov_rcx_rip, "mov rcx,[rip+12345678h]");
	test_option<true, true, true, true, true, true, true, true>(mov_rcx_rip, "mov rcx, qword ptr ds:[rip+0x12345678]");
	// rip_relative_addresses (32-bit)
	test_option<false, false, false, false, false, false, false, false>(mov_rcx_eip, "mov rcx,[0acf13571h]");
	test_option<true, false, true, true, true, true, true, true>(mov_rcx_eip, "mov rcx, qword ptr ds:[0xACF13571]");
	test_option<false, true, false, false, false, false, false, false>(mov_rcx_eip, "mov rcx,[eip+12345678h]");
	test_option<true, true, true, true, true, true, true, true>(mov_rcx_eip, "mov rcx, qword ptr ds:[eip+0x12345678]");
	// always_show_memory_size
	test_option<false, false, false, false, false, false, false, false>(mov_rcx_rip, "mov rcx,[12345678acf13570h]");
	test_option<true, true, true, true, true, false, true, true>(mov_rcx_rip, "mov rcx, ds:[rip+0x12345678]");
	test_option<false, false, false, false, false, true, false, false>(mov_rcx_rip, "mov rcx,qword ptr [12345678acf13570h]");
	test_option<true, true, true, true, true, true, true, true>(mov_rcx_rip, "mov rcx, qword ptr ds:[rip+0x12345678]");
	// always_show_segment_register
	test_option<false, false, false, false, false, false, false, false>(mov_rcx_rip, "mov rcx,[12345678acf13570h]");
	test_option<true, true, true, true, false, true, true, true>(mov_rcx_rip, "mov rcx, qword ptr [rip+0x12345678]");
	test_option<false, false, false, false, true, false, false, false>(mov_rcx_rip, "mov rcx,ds:[12345678acf13570h]");
	test_option<true, true, true, true, true, true, true, true>(mov_rcx_rip, "mov rcx, qword ptr ds:[rip+0x12345678]");
	// use_pseudo_ops
	test_option<false, false, false, false, false, false, false, false>(cmpps, "cmpps xmm1,xmm5,1h");
	test_option<true, true, false, true, true, true, true, true>(cmpps, "cmpps xmm1, xmm5, 0x1");
	test_option<false, false, true, false, false, false, false, false>(cmpps, "cmpltps xmm1,xmm5");
	test_option<true, true, true, true, true, true, true, true>(cmpps, "cmpltps xmm1, xmm5");
	// Not tested: show_symbol_address
}
