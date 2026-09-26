// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/tests/misc.rs, misc2.rs, mod.rs (verify_sae_er)

#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "formatter/formatter_test_utils.hpp"
#include "iced_x86/fast_formatter.hpp"
#include "iced_x86/formatter.hpp"
#include "iced_x86/gas_formatter.hpp"
#include "iced_x86/intel_formatter.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/masm_formatter.hpp"
#include "iced_x86/nasm_formatter.hpp"
#include "iced_x86/rounding_control.hpp"
#include "test_framework.hpp"
#include "test_utils/decoder_test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/non_decoded_tests.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/misc/test_formatter_operand_options_methods") {
	FormatterOperandOptions options;

	options.set_memory_size_options(MemorySizeOptions::Always);
	CHECK_EQ(options.memory_size_options(), MemorySizeOptions::Always);

	options.set_memory_size_options(MemorySizeOptions::Minimal);
	CHECK_EQ(options.memory_size_options(), MemorySizeOptions::Minimal);

	options.set_memory_size_options(MemorySizeOptions::Never);
	CHECK_EQ(options.memory_size_options(), MemorySizeOptions::Never);

	options.set_memory_size_options(MemorySizeOptions::Default);
	CHECK_EQ(options.memory_size_options(), MemorySizeOptions::Default);

	options.set_branch_size(true);
	CHECK(options.branch_size());
	options.set_branch_size(false);
	CHECK(!options.branch_size());

	options.set_rip_relative_addresses(true);
	CHECK(options.rip_relative_addresses());
	options.set_rip_relative_addresses(false);
	CHECK(!options.rip_relative_addresses());
}

TEST_CASE("formatter/misc/verify_default_formatter_options") {
	const FormatterOptions options;
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
	CHECK(options.hex_suffix() == "");
	CHECK_EQ(options.hex_digit_group_size(), 4U);
	CHECK(options.decimal_prefix() == "");
	CHECK(options.decimal_suffix() == "");
	CHECK_EQ(options.decimal_digit_group_size(), 3U);
	CHECK(options.octal_prefix() == "");
	CHECK(options.octal_suffix() == "");
	CHECK_EQ(options.octal_digit_group_size(), 4U);
	CHECK(options.binary_prefix() == "");
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

TEST_CASE("formatter/misc/verify_formatter_options_new_is_same_as_default") {
	// Rust: FormatterOptions::default() == FormatterOptions::new()
	const FormatterOptions default_options{};
	const FormatterOptions new_options;
	CHECK(default_options == new_options);
	CHECK(!(default_options != new_options));
	FormatterOptions modified = new_options;
	modified.set_hex_prefix("0x");
	CHECK(modified != new_options);
}

// Rust: formatter/tests/misc2.rs
TEST_CASE("formatter/misc2/make_sure_all_code_values_are_formatted") {
	std::vector<std::uint8_t> tested(IcedConstants::CODE_ENUM_COUNT, 0);

	const std::pair<std::uint32_t, bool> all_args[6] = {
		{16, false}, {32, false}, {64, false}, {16, true}, {32, true}, {64, true},
	};
	for (const auto& [bitness, is_misc] : all_args) {
		for (const auto& info : get_formatter_instruction_infos(bitness, is_misc).infos)
			tested[static_cast<std::size_t>(info.code)] = 1;
	}
	for (const auto& info : get_non_decoded_tests())
		tested[static_cast<std::size_t>(info.instruction.code())] = 1;

	std::string sb;
	std::uint32_t missing = 0;
	const auto code_names_ = code_names();
	for (std::size_t i = 0; i < tested.size(); i++) {
		if (tested[i] != 1 && !is_ignored_code(code_names_[i])) {
			sb += code_names_[i];
			sb += ' ';
			missing++;
		}
	}
	CHECK_EQ("Fmt: " + std::to_string(missing) + " ins " + sb, std::string("Fmt: 0 ins "));
}

// Rust: formatter/tests/mod.rs
//
// Verifies that all formatters show the {sae} and {er} decorators when needed. Each formatter adds an entry to
// `get_sae_er_formatters()` (the gas/intel/masm/nasm formatters must enable `show_useless_prefixes`).
namespace {
constexpr std::uint16_t FL_SAE = 0x01;
constexpr std::uint16_t FL_RN = 0x02;
constexpr std::uint16_t FL_RD = 0x04;
constexpr std::uint16_t FL_RU = 0x08;
constexpr std::uint16_t FL_RZ = 0x10;
constexpr std::uint16_t FL_RN_SAE = 0x20;
constexpr std::uint16_t FL_RD_SAE = 0x40;
constexpr std::uint16_t FL_RU_SAE = 0x80;
constexpr std::uint16_t FL_RZ_SAE = 0x100;

struct SaeErFormatter {
	const char* name;
	std::function<void(const Instruction& instruction, std::string& output)> format;
	// {sae}, {rn}, {rd}, {ru}, {rz}, {rn-sae}, {rd-sae}, {ru-sae}, {rz-sae}
	std::array<const char*, 9> decorators;
};

std::vector<SaeErFormatter> get_sae_er_formatters() {
	std::vector<SaeErFormatter> formatters;
	{
		auto fast = std::make_shared<FastFormatter>();
		formatters.push_back(SaeErFormatter{" fast", [fast](const Instruction& instruction, std::string& output) { fast->format(instruction, output); },
											{"{sae}", "{rn}", "{rd}", "{ru}", "{rz}", "{rn-sae}", "{rd-sae}", "{ru-sae}", "{rz-sae}"}});
	}
	{
		auto gas = std::make_shared<GasFormatter>();
		gas->options_mut().set_show_useless_prefixes(true);
		formatters.push_back(SaeErFormatter{"  gas", [gas](const Instruction& instruction, std::string& output) { gas->format(instruction, output); },
											{"{sae}", "{rn}", "{rd}", "{ru}", "{rz}", "{rn-sae}", "{rd-sae}", "{ru-sae}", "{rz-sae}"}});
	}
	{
		auto intel = std::make_shared<IntelFormatter>();
		intel->options_mut().set_show_useless_prefixes(true);
		formatters.push_back(SaeErFormatter{"intel", [intel](const Instruction& instruction, std::string& output) { intel->format(instruction, output); },
											{"{sae}", "{rne}", "{rd}", "{ru}", "{rz}", "{rne-sae}", "{rd-sae}", "{ru-sae}", "{rz-sae}"}});
	}
	{
		auto masm = std::make_shared<MasmFormatter>();
		masm->options_mut().set_show_useless_prefixes(true);
		formatters.push_back(SaeErFormatter{" masm", [masm](const Instruction& instruction, std::string& output) { masm->format(instruction, output); },
											{"{sae}", "{rn}", "{rd}", "{ru}", "{rz}", "{rn-sae}", "{rd-sae}", "{ru-sae}", "{rz-sae}"}});
	}
	{
		auto nasm = std::make_shared<NasmFormatter>();
		nasm->options_mut().set_show_useless_prefixes(true);
		formatters.push_back(SaeErFormatter{" nasm", [nasm](const Instruction& instruction, std::string& output) { nasm->format(instruction, output); },
											{"{sae}", "{rn}", "{rd}", "{ru}", "{rz}", "{rn-sae}", "{rd-sae}", "{ru-sae}", "{rz-sae}"}});
	}
	return formatters;
}

std::uint16_t get_flags(const std::string& disasm, const std::array<const char*, 9>& decorators) {
	static constexpr std::uint16_t FLAGS[9] = {FL_SAE, FL_RN, FL_RD, FL_RU, FL_RZ, FL_RN_SAE, FL_RD_SAE, FL_RU_SAE, FL_RZ_SAE};
	std::uint16_t result = 0;
	for (std::size_t i = 0; i < decorators.size(); i++) {
		if (disasm.find(decorators[i]) != std::string::npos)
			result |= FLAGS[i];
	}
	return result;
}

std::string to_hex(std::uint32_t value) {
	char buf[16];
	std::snprintf(buf, sizeof(buf), "0x%X", value);
	return buf;
}
} // namespace

TEST_CASE("formatter/verify_sae_er") {
	auto formatters = get_sae_er_formatters();
	std::string output;
	for (const auto& tc : decoder_tests(true, false)) {
		const auto bytes = to_vec_u8(tc.hex_bytes());
		auto decoder = create_decoder(tc.bitness(), bytes, tc.ip(), tc.decoder_options()).decoder;
		Instruction instr;
		decoder.decode_out(instr);

		std::uint16_t expected_flags;
		if (IcedConstants::is_mvex(tc.code()) && !instr.suppress_all_exceptions()) {
			switch (instr.rounding_control()) {
			case RoundingControl::None:
				expected_flags = 0;
				break;
			case RoundingControl::RoundToNearest:
				expected_flags = FL_RN;
				break;
			case RoundingControl::RoundDown:
				expected_flags = FL_RD;
				break;
			case RoundingControl::RoundUp:
				expected_flags = FL_RU;
				break;
			case RoundingControl::RoundTowardZero:
			default:
				expected_flags = FL_RZ;
				break;
			}
		}
		else {
			switch (instr.rounding_control()) {
			case RoundingControl::None:
				expected_flags = instr.suppress_all_exceptions() ? FL_SAE : 0;
				break;
			case RoundingControl::RoundToNearest:
				expected_flags = FL_RN_SAE;
				break;
			case RoundingControl::RoundDown:
				expected_flags = FL_RD_SAE;
				break;
			case RoundingControl::RoundUp:
				expected_flags = FL_RU_SAE;
				break;
			case RoundingControl::RoundTowardZero:
			default:
				expected_flags = FL_RZ_SAE;
				break;
			}
		}

		bool ok = true;
		std::string all_output;
		for (auto& formatter : formatters) {
			output.clear();
			formatter.format(instr, output);
			const std::uint16_t flags = get_flags(output, formatter.decorators);
			if (flags != expected_flags)
				ok = false;
			all_output += std::string(formatter.name) + ": " + to_hex(flags) + " " + output + "\n";
		}
		CHECK_MSG(ok, "\nMissing/extra {sae} and/or {er}\nexpected: " + to_hex(expected_flags) + "\n" + all_output + std::to_string(tc.bitness()) +
						  "-bit, hex " + tc.hex_bytes() + " Code = " + to_string(tc.code()) + "\n");
	}
}
