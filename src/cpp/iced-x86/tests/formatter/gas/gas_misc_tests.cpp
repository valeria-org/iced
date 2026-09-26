// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/tests/misc.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/gas/gas_fmt_factory.hpp"
#include "iced_x86/gas_formatter.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/gas/misc/methods_panic_if_invalid_operand_or_instruction_operand") {
	methods_panic_if_invalid_operand_or_instruction_operand(gas::create);
}

TEST_CASE("formatter/gas/misc/test_op_index") { test_op_index(gas::create); }

TEST_CASE("formatter/gas/misc/verify_default_formatter_options") {
	const FormatterOptions options = FormatterOptions::with_gas();
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
	CHECK(options.hex_prefix() == "0x");
	CHECK(options.hex_suffix() == "");
	CHECK_EQ(options.hex_digit_group_size(), 4U);
	CHECK(options.decimal_prefix() == "");
	CHECK(options.decimal_suffix() == "");
	CHECK_EQ(options.decimal_digit_group_size(), 3U);
	CHECK(options.octal_prefix() == "0");
	CHECK(options.octal_suffix() == "");
	CHECK_EQ(options.octal_digit_group_size(), 4U);
	CHECK(options.binary_prefix() == "0b");
	CHECK(options.binary_suffix() == "");
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

TEST_CASE("formatter/gas/misc/verify_formatter_options") { CHECK(GasFormatter().options() == FormatterOptions::with_gas()); }

TEST_CASE("formatter/gas/misc/format_mnemonic_options") { format_mnemonic_options_test("Gas", gas::create); }
