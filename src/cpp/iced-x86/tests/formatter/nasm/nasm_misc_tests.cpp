// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/nasm/tests/misc.rs, number.rs, options.rs, registers.rs, symres.rs

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "formatter/formatter_test_utils.hpp"
#include "formatter/nasm/nasm_fmt_factory.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/formatter_options.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/symbol_resolver.hpp"
#include "iced_x86/nasm_formatter.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/nasm/misc/methods_panic_if_invalid_operand_or_instruction_operand") {
	methods_panic_if_invalid_operand_or_instruction_operand(nasm::create);
}

TEST_CASE("formatter/nasm/misc/test_op_index") { test_op_index(nasm::create); }

TEST_CASE("formatter/nasm/misc/verify_default_formatter_options") {
	const auto options = FormatterOptions::with_nasm();
	CHECK(!options.uppercase_prefixes());
	CHECK(!options.uppercase_mnemonics());
	CHECK(!options.uppercase_registers());
	CHECK(!options.uppercase_keywords());
	CHECK(!options.uppercase_decorators());
	CHECK(!options.uppercase_all());
	CHECK_EQ(options.first_operand_char_index(), 0U);
	CHECK_EQ(options.tab_size(), 0U);
	CHECK(!options.space_after_operand_separator());
	CHECK(!options.space_after_memory_bracket());
	CHECK(!options.space_between_memory_add_operators());
	CHECK(!options.space_between_memory_mul_operators());
	CHECK(!options.scale_before_index());
	CHECK(!options.always_show_scale());
	CHECK(!options.always_show_segment_register());
	CHECK(!options.show_zero_displacements());
	CHECK(options.hex_prefix() == "");
	CHECK(options.hex_suffix() == "h");
	CHECK_EQ(options.hex_digit_group_size(), 4U);
	CHECK(options.decimal_prefix() == "");
	CHECK(options.decimal_suffix() == "");
	CHECK_EQ(options.decimal_digit_group_size(), 3U);
	CHECK(options.octal_prefix() == "");
	CHECK(options.octal_suffix() == "o");
	CHECK_EQ(options.octal_digit_group_size(), 4U);
	CHECK(options.binary_prefix() == "");
	CHECK(options.binary_suffix() == "b");
	CHECK_EQ(options.binary_digit_group_size(), 4U);
	CHECK(options.digit_separator() == "");
	CHECK(!options.leading_zeros());
	CHECK(!options.leading_zeroes());
	CHECK(options.uppercase_hex());
	CHECK(options.small_hex_numbers_in_decimal());
	CHECK(options.add_leading_zero_to_hex_numbers());
	CHECK_EQ(options.number_base(), NumberBase::Hexadecimal);
	CHECK(options.branch_leading_zeros());
	CHECK(options.branch_leading_zeroes());
	CHECK(!options.signed_immediate_operands());
	CHECK(options.signed_memory_displacements());
	CHECK(!options.displacement_leading_zeros());
	CHECK(!options.displacement_leading_zeroes());
	CHECK_EQ(options.memory_size_options(), MemorySizeOptions::Default);
	CHECK(!options.rip_relative_addresses());
	CHECK(options.show_branch_size());
	CHECK(options.use_pseudo_ops());
	CHECK(!options.show_symbol_address());
	CHECK(!options.prefer_st0());
	CHECK_EQ(options.cc_b(), CC_b::b);
	CHECK_EQ(options.cc_ae(), CC_ae::ae);
	CHECK_EQ(options.cc_e(), CC_e::e);
	CHECK_EQ(options.cc_ne(), CC_ne::ne);
	CHECK_EQ(options.cc_be(), CC_be::be);
	CHECK_EQ(options.cc_a(), CC_a::a);
	CHECK_EQ(options.cc_p(), CC_p::p);
	CHECK_EQ(options.cc_np(), CC_np::np);
	CHECK_EQ(options.cc_l(), CC_l::l);
	CHECK_EQ(options.cc_ge(), CC_ge::ge);
	CHECK_EQ(options.cc_le(), CC_le::le);
	CHECK_EQ(options.cc_g(), CC_g::g);
	CHECK(!options.show_useless_prefixes());
	CHECK(!options.gas_naked_registers());
	CHECK(!options.gas_show_mnemonic_size_suffix());
	CHECK(!options.gas_space_after_memory_operand_comma());
	CHECK(options.masm_add_ds_prefix32());
	CHECK(options.masm_symbol_displ_in_brackets());
	CHECK(options.masm_displ_in_brackets());
	CHECK(!options.nasm_show_sign_extended_immediate_size());
}

TEST_CASE("formatter/nasm/misc/verify_formatter_options") { CHECK(NasmFormatter().options() == FormatterOptions::with_nasm()); }

TEST_CASE("formatter/nasm/misc/format_mnemonic_options") { format_mnemonic_options_test("Nasm", nasm::create); }

TEST_CASE("formatter/nasm/misc/move_formatter") {
	// Not in Rust: the C++ formatter is move-only
	NasmFormatter formatter;
	formatter.options_mut().set_uppercase_all(true);
	NasmFormatter formatter2(std::move(formatter));
	const std::uint8_t bytes[] = {0x00, 0xCE};
	Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
	const auto instr = decoder.decode();
	std::string output;
	formatter2.format(instr, output);
	CHECK(output == "ADD DH,CL");
	formatter = std::move(formatter2);
	output.clear();
	formatter.format(instr, output);
	CHECK(output == "ADD DH,CL");
}

TEST_CASE("formatter/nasm/number/test_numbers") { number_tests(nasm::create_numbers); }

TEST_CASE("formatter/nasm/options/test_options_common") { test_format_file_common("Nasm", "OptionsResult.Common", nasm::create_options); }

TEST_CASE("formatter/nasm/options/test_options_all") { test_format_file_all("Nasm", "OptionsResult", nasm::create_options); }

TEST_CASE("formatter/nasm/options/test_options2") { test_format_file("Nasm", "OptionsResult2", "Options2", nasm::create_options); }

TEST_CASE("formatter/nasm/registers/test_regs") { register_tests("Nasm", "RegisterTests", nasm::create_registers); }

TEST_CASE("formatter/nasm/symres/symres") { symbol_resolver_test("Nasm", "SymbolResolverTests", nasm::create_resolver); }

// Rust: the doc examples in formatter/nasm.rs
TEST_CASE("formatter/nasm/doc_examples") {
	{
		const std::uint8_t bytes[] = {0x62, 0xF2, 0x4F, 0xDD, 0x72, 0x50, 0x01};
		Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
		const auto instr = decoder.decode();
		std::string output;
		NasmFormatter formatter;
		formatter.options_mut().set_uppercase_mnemonics(true);
		formatter.format(instr, output);
		CHECK(output == "VCVTNE2PS2BF16 zmm2{k5}{z},zmm6,[rax+4]{1to16}");
	}
	{
		class MySymbolResolver final : public SymbolResolver {
		public:
			std::optional<SymbolResult> symbol(const Instruction&, std::uint32_t, std::optional<std::uint32_t>, std::uint64_t address,
											   std::uint32_t) override {
				if (address == 0x5AA55AA5)
					return SymbolResult::with_str(address, "my_data");
				return std::nullopt;
			}
		};
		const std::uint8_t bytes[] = {0x48, 0x8B, 0x8A, 0xA5, 0x5A, 0xA5, 0x5A};
		Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
		const auto instr = decoder.decode();
		std::string output;
		NasmFormatter formatter(std::make_unique<MySymbolResolver>(), nullptr);
		formatter.format(instr, output);
		CHECK(output == "mov rcx,[rdx+my_data]");
	}
}
