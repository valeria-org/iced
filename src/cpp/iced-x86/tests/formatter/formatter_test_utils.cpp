// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "formatter/formatter_test_utils.hpp"

#include <array>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "iced_x86/decoder_options.hpp"
#include "iced_x86/format_mnemonic_options.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/symbol_flags.hpp"
#include "test_framework.hpp"
#include "test_utils.hpp"
#include "test_utils/decoder_test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#if ICED_X86_TESTS_HAS_ENCODER
#include "test_utils/non_decoded_tests.hpp"
#endif
#include "test_utils/str_utils.hpp"

namespace iced_x86::tests {

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/mod.rs, instr_infos.rs

static std::string join_path(std::string_view dir, std::string_view file) {
	std::string result = get_formatter_unit_tests_dir();
	if (!dir.empty()) {
		result += '/';
		result += dir;
	}
	result += '/';
	result += file;
	return result;
}

static std::string to_hex_string(std::uint64_t value) {
	std::ostringstream os;
	os << "0x" << std::hex << std::uppercase << value;
	return os.str();
}

static std::optional<FormatterInstructionInfo> read_next_info(std::uint32_t bitness, const std::string& line) {
	const auto parts = split(line, ',');
	std::uint32_t options;
	switch (parts.size()) {
	case 2:
		options = 0;
		break;
	case 3:
		options = to_decoder_options(parts[2]);
		break;
	default:
		throw std::runtime_error("Invalid number of commas");
	}
	const auto hex_bytes = trim(parts[0]);
	if (is_ignored_code(trim(parts[1])))
		return std::nullopt;
	const Code code = to_code(trim(parts[1]));
	const std::uint64_t ip = get_default_ip(bitness);
	return FormatterInstructionInfo{bitness, std::string(hex_bytes), ip, code, options};
}

static FormatterInstructionInfos read_infos(std::uint32_t bitness, bool is_misc) {
	const std::string filename =
		join_path("", is_misc ? "InstructionInfos" + std::to_string(bitness) + "_Misc.txt" : "InstructionInfos" + std::to_string(bitness) + ".txt");
	FormatterInstructionInfos result;
	std::uint32_t line_number = 0;
	std::uint32_t test_case_number = 0;
	for (const auto& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || starts_with(line, "#"))
			continue;
		test_case_number++;
		std::optional<FormatterInstructionInfo> tc;
		try {
			tc = read_next_info(bitness, line);
		}
		catch (const std::exception& ex) {
			throw std::runtime_error("Error parsing formatter test case file '" + filename + "', line " + std::to_string(line_number) + ": " + ex.what());
		}
		if (tc)
			result.infos.push_back(std::move(*tc));
		else
			result.ignored.insert(test_case_number - 1);
	}
	return result;
}

const FormatterInstructionInfos& get_formatter_instruction_infos(std::uint32_t bitness, bool is_misc) {
	static const FormatterInstructionInfos infos16 = read_infos(16, false);
	static const FormatterInstructionInfos infos32 = read_infos(32, false);
	static const FormatterInstructionInfos infos64 = read_infos(64, false);
	static const FormatterInstructionInfos infos_misc16 = read_infos(16, true);
	static const FormatterInstructionInfos infos_misc32 = read_infos(32, true);
	static const FormatterInstructionInfos infos_misc64 = read_infos(64, true);
	switch (bitness) {
	case 16:
		return is_misc ? infos_misc16 : infos16;
	case 32:
		return is_misc ? infos_misc32 : infos32;
	case 64:
		return is_misc ? infos_misc64 : infos64;
	default:
		throw std::runtime_error("Invalid bitness");
	}
}

std::vector<std::string> get_lines_ignore_comments(const std::string& filename) {
	std::vector<std::string> result;
	for (auto& line : read_lines(filename)) {
		if (!line.empty() && !starts_with(line, "#"))
			result.push_back(std::move(line));
	}
	return result;
}

std::vector<std::string> get_formatted_lines(std::uint32_t bitness, std::string_view dir, std::string_view file_part) {
	return get_lines_ignore_comments(join_path(dir, "Test" + std::to_string(bitness) + "_" + std::string(file_part) + ".txt"));
}

std::vector<std::string> filter_removed_code_tests(std::vector<std::string> strings, const std::unordered_set<std::uint32_t>& ignored) {
	if (ignored.empty())
		return strings;
	std::vector<std::string> result;
	result.reserve(strings.size());
	for (std::size_t i = 0; i < strings.size(); i++) {
		if (ignored.find(static_cast<std::uint32_t>(i)) == ignored.end())
			result.push_back(std::move(strings[i]));
	}
	return result;
}

Instruction decode_test_instruction(std::uint32_t bitness, const std::vector<std::uint8_t>& bytes, std::uint64_t ip, Code code,
									std::uint32_t decoder_options, const InitDecoderFn& init_decoder) {
	auto decoder = create_decoder(bitness, bytes, ip, decoder_options).decoder;
	if (init_decoder)
		init_decoder(decoder);
	std::uint64_t next_rip = decoder.ip();
	const Instruction instruction = decoder.decode();
	CHECK_EQ(instruction.code(), code);
	CHECK_EQ(instruction.ip16(), static_cast<std::uint16_t>(next_rip));
	CHECK_EQ(instruction.ip32(), static_cast<std::uint32_t>(next_rip));
	CHECK_EQ(instruction.ip(), next_rip);
	next_rip += instruction.len();
	CHECK_EQ(decoder.ip(), next_rip);
	CHECK_EQ(instruction.next_ip16(), static_cast<std::uint16_t>(next_rip));
	CHECK_EQ(instruction.next_ip32(), static_cast<std::uint32_t>(next_rip));
	CHECK_EQ(instruction.next_ip(), next_rip);
	return instruction;
}

static void check_formatted(const std::string& actual, const std::string& expected, const std::string& context) {
	if (actual != expected)
		report_failure(__FILE__, __LINE__, "Formatted string: '" + actual + "' != expected: '" + expected + "' " + context);
}

void formatter_test(std::uint32_t bitness, std::string_view dir, std::string_view filename, bool is_misc, const FormatterFactory& fmt_factory) {
	const auto& infos = get_formatter_instruction_infos(bitness, is_misc);
	const auto lines = filter_removed_code_tests(get_formatted_lines(bitness, dir, filename), infos.ignored);
	REQUIRE_MSG(infos.infos.size() == lines.size(), "Infos len (" + std::to_string(infos.infos.size()) + ") != fmt len (" +
														 std::to_string(lines.size()) + "); dir=" + std::string(dir) +
														 ", filename: " + std::string(filename) + ", is_misc: " + (is_misc ? "true" : "false"));
	for (std::size_t i = 0; i < lines.size(); i++) {
		const auto& info = infos.infos[i];
		const auto bytes = to_vec_u8(info.hex_bytes);
		const auto instruction = decode_test_instruction(info.bitness, bytes, info.ip, info.code, info.options);
		auto formatter = fmt_factory();
		format_test_instruction_core(instruction, lines[i], *formatter,
									 "(" + std::to_string(info.bitness) + "-bit, hex bytes: " + info.hex_bytes + ", Code: " + to_string(info.code) + ")");
	}
}

#if ICED_X86_TESTS_HAS_ENCODER
void formatter_test_nondec(std::uint32_t bitness, std::string_view dir, std::string_view filename, const FormatterFactory& fmt_factory) {
	const auto& instrs = get_non_decoded_infos(bitness);
	const auto lines = get_formatted_lines(bitness, dir, filename);
	REQUIRE_MSG(instrs.size() == lines.size(), "Instrs len (" + std::to_string(instrs.size()) + ") != fmt len (" + std::to_string(lines.size()) +
												   "); dir=" + std::string(dir) + ", filename: " + std::string(filename));
	for (std::size_t i = 0; i < lines.size(); i++) {
		auto formatter = fmt_factory();
		format_test_instruction_core(instrs[i].instruction, lines[i], *formatter,
									 "(" + std::to_string(bitness) + "-bit, hex bytes: " + instrs[i].hex_bytes + ")");
	}
}
#endif

void format_test_instruction_core(const Instruction& instruction, const std::string& formatted_string, Formatter& formatter,
								  const std::string& context) {
	{
		std::string actual_formatted_string;
		formatter.format(instruction, actual_formatted_string);
		check_formatted(actual_formatted_string, formatted_string, context);
	}

	std::string mnemonic;
	formatter.format_mnemonic(instruction, mnemonic);
	const std::uint32_t op_count = formatter.operand_count(instruction);
	std::vector<std::string> operands;
	operands.reserve(op_count);
	for (std::uint32_t i = 0; i < op_count; i++) {
		std::string output;
		CHECK(formatter.format_operand(instruction, output, i).is_ok());
		operands.push_back(std::move(output));
	}
	{
		std::string output = mnemonic;
		if (!operands.empty()) {
			output.push_back(' ');
			for (std::size_t i = 0; i < operands.size(); i++) {
				if (i > 0)
					formatter.format_operand_separator(instruction, output);
				output += operands[i];
			}
		}
		check_formatted(output, formatted_string, context);
	}

	{
		std::string all_operands;
		formatter.format_all_operands(instruction, all_operands);
		const std::string actual_formatted_string = all_operands.empty() ? mnemonic : mnemonic + " " + all_operands;
		check_formatted(actual_formatted_string, formatted_string, context);
	}
}

void simple_format_test(std::uint32_t bitness, const std::string& hex_bytes, std::uint64_t ip, Code code, std::uint32_t decoder_options,
						std::uint32_t line_number, const std::string& formatted_string, Formatter& formatter, const InitDecoderFn& init_decoder) {
	const auto bytes = to_vec_u8(hex_bytes);
	const auto instruction = decode_test_instruction(bitness, bytes, ip, code, decoder_options, init_decoder);
	std::string output;
	formatter.format(instruction, output);
	check_formatted(output, formatted_string, "line " + std::to_string(line_number));
}

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/opt_value.rs, options_parser.rs, opts_info.rs, options_test_case_parser.rs, opts_infos.rs

std::uint32_t OptionValue::get_decoder_options(const std::vector<std::pair<OptionsProps, OptionValue>>& props) {
	std::uint32_t decoder_options = DecoderOptions::NONE;
	for (const auto& prop : props) {
		if (const auto* value = std::get_if<DecoderOptionsValue>(&prop.second.value_))
			decoder_options |= value->value;
	}
	return decoder_options;
}

void OptionValue::initialize_options(FormatterOptions& options, OptionsProps property) const {
	switch (property) {
	case OptionsProps::AddLeadingZeroToHexNumbers:
		options.set_add_leading_zero_to_hex_numbers(to_bool());
		break;
	case OptionsProps::AlwaysShowScale:
		options.set_always_show_scale(to_bool());
		break;
	case OptionsProps::AlwaysShowSegmentRegister:
		options.set_always_show_segment_register(to_bool());
		break;
	case OptionsProps::BinaryDigitGroupSize:
		options.set_binary_digit_group_size(to_i32_as_u32());
		break;
	case OptionsProps::BinaryPrefix:
		options.set_binary_prefix_string(to_str());
		break;
	case OptionsProps::BinarySuffix:
		options.set_binary_suffix_string(to_str());
		break;
	case OptionsProps::BranchLeadingZeros:
		options.set_branch_leading_zeros(to_bool());
		break;
	case OptionsProps::DecimalDigitGroupSize:
		options.set_decimal_digit_group_size(to_i32_as_u32());
		break;
	case OptionsProps::DecimalPrefix:
		options.set_decimal_prefix_string(to_str());
		break;
	case OptionsProps::DecimalSuffix:
		options.set_decimal_suffix_string(to_str());
		break;
	case OptionsProps::DigitSeparator:
		options.set_digit_separator_string(to_str());
		break;
	case OptionsProps::DisplacementLeadingZeros:
		options.set_displacement_leading_zeros(to_bool());
		break;
	case OptionsProps::FirstOperandCharIndex:
		options.set_first_operand_char_index(to_i32_as_u32());
		break;
	case OptionsProps::GasNakedRegisters:
		options.set_gas_naked_registers(to_bool());
		break;
	case OptionsProps::GasShowMnemonicSizeSuffix:
		options.set_gas_show_mnemonic_size_suffix(to_bool());
		break;
	case OptionsProps::GasSpaceAfterMemoryOperandComma:
		options.set_gas_space_after_memory_operand_comma(to_bool());
		break;
	case OptionsProps::HexDigitGroupSize:
		options.set_hex_digit_group_size(to_i32_as_u32());
		break;
	case OptionsProps::HexPrefix:
		options.set_hex_prefix_string(to_str());
		break;
	case OptionsProps::HexSuffix:
		options.set_hex_suffix_string(to_str());
		break;
	case OptionsProps::LeadingZeros:
		options.set_leading_zeros(to_bool());
		break;
	case OptionsProps::MasmAddDsPrefix32:
		options.set_masm_add_ds_prefix32(to_bool());
		break;
	case OptionsProps::MemorySizeOptions:
		options.set_memory_size_options(to_memory_size_options());
		break;
	case OptionsProps::NasmShowSignExtendedImmediateSize:
		options.set_nasm_show_sign_extended_immediate_size(to_bool());
		break;
	case OptionsProps::NumberBase:
		options.set_number_base(to_number_base());
		break;
	case OptionsProps::OctalDigitGroupSize:
		options.set_octal_digit_group_size(to_i32_as_u32());
		break;
	case OptionsProps::OctalPrefix:
		options.set_octal_prefix_string(to_str());
		break;
	case OptionsProps::OctalSuffix:
		options.set_octal_suffix_string(to_str());
		break;
	case OptionsProps::PreferST0:
		options.set_prefer_st0(to_bool());
		break;
	case OptionsProps::RipRelativeAddresses:
		options.set_rip_relative_addresses(to_bool());
		break;
	case OptionsProps::ScaleBeforeIndex:
		options.set_scale_before_index(to_bool());
		break;
	case OptionsProps::ShowBranchSize:
		options.set_show_branch_size(to_bool());
		break;
	case OptionsProps::ShowSymbolAddress:
		options.set_show_symbol_address(to_bool());
		break;
	case OptionsProps::ShowZeroDisplacements:
		options.set_show_zero_displacements(to_bool());
		break;
	case OptionsProps::SignedImmediateOperands:
		options.set_signed_immediate_operands(to_bool());
		break;
	case OptionsProps::SignedMemoryDisplacements:
		options.set_signed_memory_displacements(to_bool());
		break;
	case OptionsProps::SmallHexNumbersInDecimal:
		options.set_small_hex_numbers_in_decimal(to_bool());
		break;
	case OptionsProps::SpaceAfterMemoryBracket:
		options.set_space_after_memory_bracket(to_bool());
		break;
	case OptionsProps::SpaceAfterOperandSeparator:
		options.set_space_after_operand_separator(to_bool());
		break;
	case OptionsProps::SpaceBetweenMemoryAddOperators:
		options.set_space_between_memory_add_operators(to_bool());
		break;
	case OptionsProps::SpaceBetweenMemoryMulOperators:
		options.set_space_between_memory_mul_operators(to_bool());
		break;
	case OptionsProps::TabSize:
		options.set_tab_size(to_i32_as_u32());
		break;
	case OptionsProps::UppercaseAll:
		options.set_uppercase_all(to_bool());
		break;
	case OptionsProps::UppercaseDecorators:
		options.set_uppercase_decorators(to_bool());
		break;
	case OptionsProps::UppercaseHex:
		options.set_uppercase_hex(to_bool());
		break;
	case OptionsProps::UppercaseKeywords:
		options.set_uppercase_keywords(to_bool());
		break;
	case OptionsProps::UppercaseMnemonics:
		options.set_uppercase_mnemonics(to_bool());
		break;
	case OptionsProps::UppercasePrefixes:
		options.set_uppercase_prefixes(to_bool());
		break;
	case OptionsProps::UppercaseRegisters:
		options.set_uppercase_registers(to_bool());
		break;
	case OptionsProps::UsePseudoOps:
		options.set_use_pseudo_ops(to_bool());
		break;
	case OptionsProps::CC_b:
		options.set_cc_b(get<CC_b>());
		break;
	case OptionsProps::CC_ae:
		options.set_cc_ae(get<CC_ae>());
		break;
	case OptionsProps::CC_e:
		options.set_cc_e(get<CC_e>());
		break;
	case OptionsProps::CC_ne:
		options.set_cc_ne(get<CC_ne>());
		break;
	case OptionsProps::CC_be:
		options.set_cc_be(get<CC_be>());
		break;
	case OptionsProps::CC_a:
		options.set_cc_a(get<CC_a>());
		break;
	case OptionsProps::CC_p:
		options.set_cc_p(get<CC_p>());
		break;
	case OptionsProps::CC_np:
		options.set_cc_np(get<CC_np>());
		break;
	case OptionsProps::CC_l:
		options.set_cc_l(get<CC_l>());
		break;
	case OptionsProps::CC_ge:
		options.set_cc_ge(get<CC_ge>());
		break;
	case OptionsProps::CC_le:
		options.set_cc_le(get<CC_le>());
		break;
	case OptionsProps::CC_g:
		options.set_cc_g(get<CC_g>());
		break;
	case OptionsProps::ShowUselessPrefixes:
		options.set_show_useless_prefixes(to_bool());
		break;
	case OptionsProps::IP:
	case OptionsProps::DecoderOptions:
		break;
	}
}

void OptionValue::initialize_options_fast(FastFormatterOptions& options, OptionsProps property) const {
	switch (property) {
	case OptionsProps::AddLeadingZeroToHexNumbers:
	case OptionsProps::AlwaysShowScale:
	case OptionsProps::BinaryDigitGroupSize:
	case OptionsProps::BinaryPrefix:
	case OptionsProps::BinarySuffix:
	case OptionsProps::BranchLeadingZeros:
	case OptionsProps::DecimalDigitGroupSize:
	case OptionsProps::DecimalPrefix:
	case OptionsProps::DecimalSuffix:
	case OptionsProps::DigitSeparator:
	case OptionsProps::DisplacementLeadingZeros:
	case OptionsProps::FirstOperandCharIndex:
	case OptionsProps::GasNakedRegisters:
	case OptionsProps::GasShowMnemonicSizeSuffix:
	case OptionsProps::GasSpaceAfterMemoryOperandComma:
	case OptionsProps::HexDigitGroupSize:
	case OptionsProps::LeadingZeros:
	case OptionsProps::MasmAddDsPrefix32:
	case OptionsProps::NasmShowSignExtendedImmediateSize:
	case OptionsProps::NumberBase:
	case OptionsProps::OctalDigitGroupSize:
	case OptionsProps::OctalPrefix:
	case OptionsProps::OctalSuffix:
	case OptionsProps::PreferST0:
	case OptionsProps::ScaleBeforeIndex:
	case OptionsProps::ShowBranchSize:
	case OptionsProps::ShowZeroDisplacements:
	case OptionsProps::SignedImmediateOperands:
	case OptionsProps::SignedMemoryDisplacements:
	case OptionsProps::SmallHexNumbersInDecimal:
	case OptionsProps::SpaceAfterMemoryBracket:
	case OptionsProps::SpaceBetweenMemoryAddOperators:
	case OptionsProps::SpaceBetweenMemoryMulOperators:
	case OptionsProps::TabSize:
	case OptionsProps::UppercaseAll:
	case OptionsProps::UppercaseDecorators:
	case OptionsProps::UppercaseKeywords:
	case OptionsProps::UppercaseMnemonics:
	case OptionsProps::UppercasePrefixes:
	case OptionsProps::UppercaseRegisters:
	case OptionsProps::CC_b:
	case OptionsProps::CC_ae:
	case OptionsProps::CC_e:
	case OptionsProps::CC_ne:
	case OptionsProps::CC_be:
	case OptionsProps::CC_a:
	case OptionsProps::CC_p:
	case OptionsProps::CC_np:
	case OptionsProps::CC_l:
	case OptionsProps::CC_ge:
	case OptionsProps::CC_le:
	case OptionsProps::CC_g:
	case OptionsProps::ShowUselessPrefixes:
		break;
	case OptionsProps::AlwaysShowSegmentRegister:
		options.set_always_show_segment_register(to_bool());
		break;
	case OptionsProps::RipRelativeAddresses:
		options.set_rip_relative_addresses(to_bool());
		break;
	case OptionsProps::ShowSymbolAddress:
		options.set_show_symbol_address(to_bool());
		break;
	case OptionsProps::SpaceAfterOperandSeparator:
		options.set_space_after_operand_separator(to_bool());
		break;
	case OptionsProps::UppercaseHex:
		options.set_uppercase_hex(to_bool());
		break;
	case OptionsProps::UsePseudoOps:
		options.set_use_pseudo_ops(to_bool());
		break;
	case OptionsProps::MemorySizeOptions:
		options.set_always_show_memory_size(to_memory_size_options() == MemorySizeOptions::Always);
		break;
	case OptionsProps::HexPrefix:
		if (to_str() == "0x")
			options.set_use_hex_prefix(true);
		break;
	case OptionsProps::HexSuffix:
		if (to_str() == "h")
			options.set_use_hex_prefix(false);
		break;
	case OptionsProps::IP:
	case OptionsProps::DecoderOptions:
		break;
	}
}

void OptionValue::initialize_decoder(Decoder& decoder, OptionsProps property) const {
	if (property == OptionsProps::IP)
		decoder.set_ip(to_u64());
}

std::pair<OptionsProps, OptionValue> parse_option(std::string_view key_value) {
	const auto trimmed = trim(key_value);
	const auto eq_index = trimmed.find('=');
	if (eq_index == std::string_view::npos)
		throw std::runtime_error("Expected key=value: '" + std::string(key_value) + "'");
	const auto value_str = trim(trimmed.substr(eq_index + 1));
	const OptionsProps prop = to_options_props(trimmed.substr(0, eq_index));
	switch (prop) {
	case OptionsProps::AddLeadingZeroToHexNumbers:
	case OptionsProps::AlwaysShowScale:
	case OptionsProps::AlwaysShowSegmentRegister:
	case OptionsProps::BranchLeadingZeros:
	case OptionsProps::DisplacementLeadingZeros:
	case OptionsProps::GasNakedRegisters:
	case OptionsProps::GasShowMnemonicSizeSuffix:
	case OptionsProps::GasSpaceAfterMemoryOperandComma:
	case OptionsProps::LeadingZeros:
	case OptionsProps::MasmAddDsPrefix32:
	case OptionsProps::NasmShowSignExtendedImmediateSize:
	case OptionsProps::PreferST0:
	case OptionsProps::RipRelativeAddresses:
	case OptionsProps::ScaleBeforeIndex:
	case OptionsProps::ShowBranchSize:
	case OptionsProps::ShowSymbolAddress:
	case OptionsProps::ShowZeroDisplacements:
	case OptionsProps::SignedImmediateOperands:
	case OptionsProps::SignedMemoryDisplacements:
	case OptionsProps::SmallHexNumbersInDecimal:
	case OptionsProps::SpaceAfterMemoryBracket:
	case OptionsProps::SpaceAfterOperandSeparator:
	case OptionsProps::SpaceBetweenMemoryAddOperators:
	case OptionsProps::SpaceBetweenMemoryMulOperators:
	case OptionsProps::UppercaseAll:
	case OptionsProps::UppercaseDecorators:
	case OptionsProps::UppercaseHex:
	case OptionsProps::UppercaseKeywords:
	case OptionsProps::UppercaseMnemonics:
	case OptionsProps::UppercasePrefixes:
	case OptionsProps::UppercaseRegisters:
	case OptionsProps::UsePseudoOps:
	case OptionsProps::ShowUselessPrefixes:
		return {prop, OptionValue(to_boolean(value_str))};

	case OptionsProps::BinaryDigitGroupSize:
	case OptionsProps::DecimalDigitGroupSize:
	case OptionsProps::FirstOperandCharIndex:
	case OptionsProps::HexDigitGroupSize:
	case OptionsProps::OctalDigitGroupSize:
	case OptionsProps::TabSize:
		return {prop, OptionValue(to_i32(value_str))};

	case OptionsProps::IP:
		return {prop, OptionValue(to_u64(value_str))};

	case OptionsProps::BinaryPrefix:
	case OptionsProps::BinarySuffix:
	case OptionsProps::DecimalPrefix:
	case OptionsProps::DecimalSuffix:
	case OptionsProps::DigitSeparator:
	case OptionsProps::HexPrefix:
	case OptionsProps::HexSuffix:
	case OptionsProps::OctalPrefix:
	case OptionsProps::OctalSuffix:
		return {prop, OptionValue(std::string(value_str == "<null>" ? std::string_view() : value_str))};

	case OptionsProps::MemorySizeOptions:
		return {prop, OptionValue(to_memory_size_options(value_str))};
	case OptionsProps::DecoderOptions:
		return {prop, OptionValue(DecoderOptionsValue{to_decoder_options(value_str)})};
	case OptionsProps::NumberBase:
		return {prop, OptionValue(to_number_base(value_str))};
	case OptionsProps::CC_b:
		return {prop, OptionValue(to_cc_b(value_str))};
	case OptionsProps::CC_ae:
		return {prop, OptionValue(to_cc_ae(value_str))};
	case OptionsProps::CC_e:
		return {prop, OptionValue(to_cc_e(value_str))};
	case OptionsProps::CC_ne:
		return {prop, OptionValue(to_cc_ne(value_str))};
	case OptionsProps::CC_be:
		return {prop, OptionValue(to_cc_be(value_str))};
	case OptionsProps::CC_a:
		return {prop, OptionValue(to_cc_a(value_str))};
	case OptionsProps::CC_p:
		return {prop, OptionValue(to_cc_p(value_str))};
	case OptionsProps::CC_np:
		return {prop, OptionValue(to_cc_np(value_str))};
	case OptionsProps::CC_l:
		return {prop, OptionValue(to_cc_l(value_str))};
	case OptionsProps::CC_ge:
		return {prop, OptionValue(to_cc_ge(value_str))};
	case OptionsProps::CC_le:
		return {prop, OptionValue(to_cc_le(value_str))};
	case OptionsProps::CC_g:
		return {prop, OptionValue(to_cc_g(value_str))};
	}
	throw std::runtime_error("Invalid option: '" + std::string(key_value) + "'");
}

void OptionsInstructionInfo::initialize_options(FormatterOptions& options) const {
	for (const auto& info : vec)
		info.second.initialize_options(options, info.first);
}

void OptionsInstructionInfo::initialize_options_fast(FastFormatterOptions& options) const {
	for (const auto& info : vec)
		info.second.initialize_options_fast(options, info.first);
}

void OptionsInstructionInfo::initialize_decoder(Decoder& decoder) const {
	for (const auto& info : vec)
		info.second.initialize_decoder(decoder, info.first);
}

static std::optional<OptionsInstructionInfo> read_next_options_test_case(const std::string& line, std::uint32_t line_number) {
	const auto elems = split(line, ',');
	if (elems.size() != 4)
		throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));

	const std::uint32_t bitness = to_u32(elems[0]);
	const std::uint64_t ip = get_default_ip(bitness);
	const auto hex_bytes = elems[1];
	static_cast<void>(to_vec_u8(hex_bytes));
	if (is_ignored_code(elems[2]))
		return std::nullopt;
	const Code code = to_code(elems[2]);
	std::vector<std::pair<OptionsProps, OptionValue>> properties;
	for (const auto part : split_whitespace(elems[3])) {
		if (trim(part).empty())
			continue;
		properties.push_back(parse_option(part));
	}

	const std::uint32_t decoder_options = OptionValue::get_decoder_options(properties);
	return OptionsInstructionInfo{bitness, std::string(hex_bytes), ip, decoder_options, line_number, code, std::move(properties)};
}

OptionsInstructionInfos read_options_test_file(const std::string& filename) {
	OptionsInstructionInfos result;
	std::uint32_t line_number = 0;
	std::uint32_t test_case_number = 0;
	for (const auto& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || starts_with(line, "#"))
			continue;
		test_case_number++;
		std::optional<OptionsInstructionInfo> tc;
		try {
			tc = read_next_options_test_case(line, line_number);
		}
		catch (const std::exception& ex) {
			throw std::runtime_error("Error parsing options test case file '" + filename + "', line " + std::to_string(line_number) + ": " + ex.what());
		}
		if (tc)
			result.infos.push_back(std::move(*tc));
		else
			result.ignored.insert(test_case_number - 1);
	}
	return result;
}

const OptionsInstructionInfos& get_common_options_infos() {
	static const OptionsInstructionInfos infos = read_options_test_file(join_path("", "Options.Common.txt"));
	return infos;
}

const OptionsInstructionInfos& get_all_options_infos() {
	static const OptionsInstructionInfos infos = read_options_test_file(join_path("", "Options.txt"));
	return infos;
}

std::vector<std::pair<const OptionsInstructionInfo*, std::string>> filter_options_infos(std::string_view dir, std::string_view file_part,
																						   const OptionsInstructionInfos& infos) {
	const std::string filename = join_path(dir, std::string(file_part) + ".txt");
	auto lines = filter_removed_code_tests(get_lines_ignore_comments(filename), infos.ignored);
	if (lines.size() != infos.infos.size())
		throw std::runtime_error("lines.len() (" + std::to_string(lines.size()) + ") != all_infos.len() (" + std::to_string(infos.infos.size()) +
								 "), file: " + filename);
	std::vector<std::pair<const OptionsInstructionInfo*, std::string>> result;
	result.reserve(lines.size());
	for (std::size_t i = 0; i < lines.size(); i++)
		result.emplace_back(&infos.infos[i], std::move(lines[i]));
	return result;
}

static void test_format(const std::vector<std::pair<const OptionsInstructionInfo*, std::string>>& infos, const FormatterFactory& fmt_factory) {
	for (const auto& [tc, formatted_string] : infos) {
		auto formatter = fmt_factory();
		tc->initialize_options(formatter->options_mut());
		simple_format_test(tc->bitness, tc->hex_bytes, tc->ip, tc->code, tc->decoder_options, tc->line_number, formatted_string, *formatter,
						   [tc = tc](Decoder& decoder) { tc->initialize_decoder(decoder); });
	}
}

void test_format_file_common(std::string_view dir, std::string_view file_part, const FormatterFactory& fmt_factory) {
	test_format(filter_options_infos(dir, file_part, get_common_options_infos()), fmt_factory);
}

void test_format_file_all(std::string_view dir, std::string_view file_part, const FormatterFactory& fmt_factory) {
	test_format(filter_options_infos(dir, file_part, get_all_options_infos()), fmt_factory);
}

void test_format_file(std::string_view dir, std::string_view file_part, std::string_view options_file, const FormatterFactory& fmt_factory) {
	const auto infos = read_options_test_file(join_path(dir, std::string(options_file) + ".txt"));
	test_format(filter_options_infos(dir, file_part, infos), fmt_factory);
}

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/sym_res.rs, sym_res_test_case.rs, sym_res_test_parser.rs

static std::optional<SymbolResolverTestCase> read_next_symbol_resolver_test_case(const std::unordered_map<std::string_view, std::uint32_t>& to_flags,
																				 const std::string& line, std::uint32_t line_number) {
	const auto elems = split(line, ',');
	constexpr std::size_t SYM_RES_INDEX = 4;
	if (elems.size() < SYM_RES_INDEX)
		throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));

	const std::uint32_t bitness = to_u32(elems[0]);
	const std::uint64_t ip = get_default_ip(bitness);
	const std::string hex_bytes(trim(elems[1]));
	static_cast<void>(to_vec_u8(hex_bytes));
	if (is_ignored_code(elems[2]))
		return std::nullopt;
	const Code code = to_code(elems[2]);

	std::vector<std::pair<OptionsProps, OptionValue>> options;
	for (const auto value : split_whitespace(elems[3])) {
		if (value.empty())
			continue;
		options.push_back(parse_option(value));
	}

	std::vector<SymbolResultTestCase> symbol_results;
	symbol_results.reserve(elems.size() - SYM_RES_INDEX);
	for (std::size_t i = SYM_RES_INDEX; i < elems.size(); i++) {
		const auto sym_parts = split(elems[i], ';');
		if (sym_parts.size() != 5)
			throw std::runtime_error("Invalid number of semicolons: " + std::to_string(sym_parts.size() - 1));

		const std::uint64_t address = to_u64(sym_parts[0]);
		const std::uint64_t symbol_address = to_u64(sym_parts[1]);
		const std::uint32_t address_size = to_u32(sym_parts[2]);
		std::vector<std::string> symbol_parts;
		for (const auto part : split(sym_parts[3], '|'))
			symbol_parts.emplace_back(part);

		std::optional<MemorySize> memory_size;
		std::uint32_t flags = SymbolFlags::NONE;
		for (const auto value : split_whitespace(sym_parts[4])) {
			if (value.empty())
				continue;
			const auto it = to_flags.find(value);
			if (it != to_flags.end())
				flags |= it->second;
			else
				memory_size = to_memory_size(value);
		}
		symbol_results.push_back(SymbolResultTestCase{address, symbol_address, address_size, flags, memory_size, std::move(symbol_parts)});
	}

	const std::uint32_t decoder_options = OptionValue::get_decoder_options(options);
	return SymbolResolverTestCase{bitness, hex_bytes, ip, decoder_options, line_number, code, std::move(options), std::move(symbol_results)};
}

static SymbolResolverTestCases read_symbol_resolver_test_cases(const std::string& filename) {
	const auto to_flags = create_dict(SYMBOL_FLAGS_DICT);
	SymbolResolverTestCases result;
	std::uint32_t line_number = 0;
	std::uint32_t test_case_number = 0;
	for (const auto& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || starts_with(line, "#"))
			continue;
		test_case_number++;
		std::optional<SymbolResolverTestCase> tc;
		try {
			tc = read_next_symbol_resolver_test_case(to_flags, line, line_number);
		}
		catch (const std::exception& ex) {
			throw std::runtime_error("Error parsing symbol resolver test case file '" + filename + "', line " + std::to_string(line_number) + ": " +
									 ex.what());
		}
		if (tc)
			result.infos.push_back(std::move(*tc));
		else
			result.ignored.insert(test_case_number - 1);
	}
	return result;
}

const SymbolResolverTestCases& get_symbol_resolver_test_cases() {
	static const SymbolResolverTestCases infos = read_symbol_resolver_test_cases(join_path("", "SymbolResolverTests.txt"));
	return infos;
}

std::optional<SymbolResult> TestSymbolResolver::symbol(const Instruction& instruction, std::uint32_t operand,
														std::optional<std::uint32_t> instruction_operand, std::uint64_t address,
														std::uint32_t address_size) {
	static_cast<void>(instruction);
	static_cast<void>(operand);
	static_cast<void>(instruction_operand);
	for (const auto& tc : info_->symbol_results) {
		if (tc.address != address || tc.address_size != address_size)
			continue;
		vec_.clear();
		for (const auto& part : tc.symbol_parts)
			vec_.emplace_back(std::string_view(part), FormatterTextKind::Text);
		const auto text = SymResTextInfo::with_vec(vec_);
		if (tc.memory_size)
			return SymbolResult::with_text_flags_size(tc.symbol_address, text, tc.flags, *tc.memory_size);
		return SymbolResult::with_text_flags(tc.symbol_address, text, tc.flags);
	}
	return std::nullopt;
}

std::pair<const SymbolResolverTestCases*, std::vector<std::string>> get_symbol_resolver_infos_and_lines(std::string_view dir,
																										  std::string_view filename) {
	const auto& infos = get_symbol_resolver_test_cases();
	auto formatted_lines = filter_removed_code_tests(get_lines_ignore_comments(join_path(dir, std::string(filename) + ".txt")), infos.ignored);
	if (infos.infos.size() != formatted_lines.size())
		throw std::runtime_error("infos.len() (" + std::to_string(infos.infos.size()) + ") != formatted_lines.len() (" +
								 std::to_string(formatted_lines.size()) + ")");
	return {&infos, std::move(formatted_lines)};
}

void symbol_resolver_test(std::string_view dir, std::string_view filename, const FormatterResolverFactory& fmt_factory) {
	const auto [infos, formatted_lines] = get_symbol_resolver_infos_and_lines(dir, filename);
	for (std::size_t i = 0; i < formatted_lines.size(); i++) {
		const auto& info = infos->infos[i];
		auto formatter = fmt_factory(std::make_unique<TestSymbolResolver>(info));
		for (const auto& props : info.options)
			props.second.initialize_options(formatter->options_mut(), props.first);
		simple_format_test(info.bitness, info.hex_bytes, info.ip, info.code, info.decoder_options, info.line_number, formatted_lines[i], *formatter,
						   [&info](Decoder& decoder) {
							   for (const auto& props : info.options)
								   props.second.initialize_decoder(decoder, props.first);
						   });
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/mnemonic_opts_parser.rs

static std::optional<MnemonicOptionsTestCase> read_next_mnemonic_options_test_case(const std::unordered_map<std::string_view, std::uint32_t>& to_flags,
																				   const std::string& line) {
	const auto elems = split(line, ',');
	if (elems.size() != 5)
		throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));

	std::string hex_bytes(trim(elems[0]));
	static_cast<void>(to_vec_u8(hex_bytes));
	if (is_ignored_code(elems[1]))
		return std::nullopt;
	const Code code = to_code(elems[1]);
	const std::uint32_t bitness = to_u32(elems[2]);
	const std::uint64_t ip = get_default_ip(bitness);
	std::string formatted_string(trim(elems[3]));
	for (auto& c : formatted_string) {
		if (c == '|')
			c = ',';
	}
	std::uint32_t flags = FormatMnemonicOptions::NONE;
	for (const auto value : split_whitespace(elems[4])) {
		if (value.empty())
			continue;
		const auto it = to_flags.find(value);
		if (it == to_flags.end())
			throw std::runtime_error("Invalid flags value: " + std::string(value));
		flags |= it->second;
	}

	return MnemonicOptionsTestCase{std::move(hex_bytes), code, bitness, ip, std::move(formatted_string), flags};
}

std::vector<MnemonicOptionsTestCase> read_mnemonic_options_test_cases(const std::string& filename) {
	const auto to_flags = create_dict(FORMAT_MNEMONIC_OPTIONS_DICT);
	std::vector<MnemonicOptionsTestCase> result;
	std::uint32_t line_number = 0;
	for (const auto& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || starts_with(line, "#"))
			continue;
		std::optional<MnemonicOptionsTestCase> tc;
		try {
			tc = read_next_mnemonic_options_test_case(to_flags, line);
		}
		catch (const std::exception& ex) {
			throw std::runtime_error("Error parsing mnemonic options test case file '" + filename + "', line " + std::to_string(line_number) + ": " +
									 ex.what());
		}
		if (tc)
			result.push_back(std::move(*tc));
	}
	return result;
}

void format_mnemonic_options_test(std::string_view dir, const FormatterFactory& fmt_factory) {
	for (const auto& tc : read_mnemonic_options_test_cases(join_path(dir, "MnemonicOptions.txt"))) {
		const auto bytes = to_vec_u8(tc.hex_bytes);
		auto decoder = create_decoder(tc.bitness, bytes, tc.ip, DecoderOptions::NONE).decoder;
		const auto instruction = decoder.decode();
		CHECK_EQ(instruction.code(), tc.code);
		auto formatter = fmt_factory();
		std::string output;
		formatter->format_mnemonic_options(instruction, output, tc.flags);
		check_formatted(output, tc.formatted_string, "(hex bytes: " + tc.hex_bytes + ")");
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/number.rs

using Number = std::variant<std::int8_t, std::uint8_t, std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t>;

static Number read_number_test_case(const std::string& line) {
	const auto elems = split(line, ',');
	if (elems.size() != 2)
		throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));
	const auto type = trim(elems[0]);
	if (type == "i8")
		return to_i8(elems[1]);
	if (type == "u8")
		return to_u8(elems[1]);
	if (type == "i16")
		return to_i16(elems[1]);
	if (type == "u16")
		return to_u16(elems[1]);
	if (type == "i32")
		return to_i32(elems[1]);
	if (type == "u32")
		return to_u32(elems[1]);
	if (type == "i64")
		return to_i64(elems[1]);
	if (type == "u64")
		return to_u64(elems[1]);
	throw std::runtime_error("Invalid type: " + std::string(elems[0]));
}

static constexpr std::array<NumberBase, 4> NUMBER_BASES = {
	NumberBase::Hexadecimal,
	NumberBase::Decimal,
	NumberBase::Octal,
	NumberBase::Binary,
};

void number_tests(const FormatterFactory& fmt_factory) {
	REQUIRE_EQ(NUMBER_BASES.size(), number_base_len());
	std::vector<Number> numbers;
	{
		const std::string filename = join_path("", "Number.txt");
		std::uint32_t line_number = 0;
		for (const auto& line : read_lines(filename)) {
			line_number++;
			if (line.empty() || starts_with(line, "#"))
				continue;
			try {
				numbers.push_back(read_number_test_case(line));
			}
			catch (const std::exception& ex) {
				throw std::runtime_error("Error parsing number test case file '" + filename + "', line " + std::to_string(line_number) + ": " +
										 ex.what());
			}
		}
	}
	std::vector<std::vector<std::string>> formatted_numbers;
	{
		const std::string filename = join_path("", "NumberTests.txt");
		std::uint32_t line_number = 0;
		for (const auto& line : read_lines(filename)) {
			line_number++;
			if (line.empty() || starts_with(line, "#"))
				continue;
			const auto elems = split(line, ',');
			if (elems.size() != NUMBER_BASES.size())
				throw std::runtime_error("Error parsing number strings test case file '" + filename + "', line " + std::to_string(line_number) +
										 ": Invalid number of commas: " + std::to_string(elems.size() - 1));
			std::vector<std::string> strings;
			for (const auto s : elems)
				strings.emplace_back(trim(s));
			formatted_numbers.push_back(std::move(strings));
		}
	}

	REQUIRE_MSG(numbers.size() == formatted_numbers.size(), "Files don't have the same amount of lines: " + std::to_string(numbers.size()) +
																" != " + std::to_string(formatted_numbers.size()));

	for (std::size_t i = 0; i < numbers.size(); i++) {
		const auto& number = numbers[i];
		const auto& formatted_strings = formatted_numbers[i];
		REQUIRE_EQ(formatted_strings.size(), NUMBER_BASES.size());
		for (std::size_t j = 0; j < NUMBER_BASES.size(); j++) {
			const auto& formatted_string = formatted_strings[j];
			auto formatter = fmt_factory();
			formatter->options_mut().set_number_base(NUMBER_BASES[j]);
			const FormatterOptions cloned_options = formatter->options();
			const auto number_options = NumberFormattingOptions::with_immediate(cloned_options);
			std::string s1, s2;
			std::visit(
				[&](auto value) {
					using T = decltype(value);
					if constexpr (std::is_same_v<T, std::int8_t>) {
						s1 = std::string(formatter->format_i8(value));
						s2 = std::string(formatter->format_i8_options(value, number_options));
					}
					else if constexpr (std::is_same_v<T, std::uint8_t>) {
						s1 = std::string(formatter->format_u8(value));
						s2 = std::string(formatter->format_u8_options(value, number_options));
					}
					else if constexpr (std::is_same_v<T, std::int16_t>) {
						s1 = std::string(formatter->format_i16(value));
						s2 = std::string(formatter->format_i16_options(value, number_options));
					}
					else if constexpr (std::is_same_v<T, std::uint16_t>) {
						s1 = std::string(formatter->format_u16(value));
						s2 = std::string(formatter->format_u16_options(value, number_options));
					}
					else if constexpr (std::is_same_v<T, std::int32_t>) {
						s1 = std::string(formatter->format_i32(value));
						s2 = std::string(formatter->format_i32_options(value, number_options));
					}
					else if constexpr (std::is_same_v<T, std::uint32_t>) {
						s1 = std::string(formatter->format_u32(value));
						s2 = std::string(formatter->format_u32_options(value, number_options));
					}
					else if constexpr (std::is_same_v<T, std::int64_t>) {
						s1 = std::string(formatter->format_i64(value));
						s2 = std::string(formatter->format_i64_options(value, number_options));
					}
					else {
						static_assert(std::is_same_v<T, std::uint64_t>, "");
						s1 = std::string(formatter->format_u64(value));
						s2 = std::string(formatter->format_u64_options(value, number_options));
					}
				},
				number);
			CHECK_EQ(s1, formatted_string);
			CHECK_EQ(s2, formatted_string);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/registers.rs

void register_tests(std::string_view dir, std::string_view file_part, const FormatterFactory& fmt_factory) {
	const auto lines = get_lines_ignore_comments(join_path(dir, std::string(file_part) + ".txt"));
	REQUIRE_EQ(lines.size(), IcedConstants::REGISTER_ENUM_COUNT);
	for (std::size_t i = 0; i < lines.size(); i++) {
		const auto& expected_register_string = lines[i];
		const auto register_ = static_cast<Register>(i);
		{
			auto formatter = fmt_factory();
			const std::string actual_register_string(formatter->format_register(register_));
			CHECK_EQ(actual_register_string, expected_register_string);
		}
		{
			auto formatter = fmt_factory();
			formatter->options_mut().set_uppercase_registers(false);
			const std::string actual_register_string(formatter->format_register(register_));
			CHECK_EQ(actual_register_string, to_ascii_lowercase(expected_register_string));
		}
		{
			auto formatter = fmt_factory();
			formatter->options_mut().set_uppercase_registers(true);
			const std::string actual_register_string(formatter->format_register(register_));
			CHECK_EQ(actual_register_string, to_ascii_uppercase(expected_register_string));
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/misc.rs

#if ICED_X86_TESTS_HAS_ENCODER
static void check_declare_data(const FormatterFactory& fmt_factory, const Instruction& db) {
	for (std::uint32_t i = 0; i < db.declare_data_len(); i++) {
		static_cast<void>(fmt_factory()->op_access(db, i));
		static_cast<void>(fmt_factory()->get_instruction_operand(db, i));
		std::string output;
		CHECK(fmt_factory()->format_operand(db, output, i).is_ok());
	}
	for (std::uint32_t i = static_cast<std::uint32_t>(db.declare_data_len()); i < 17; i++) {
		CHECK(fmt_factory()->op_access(db, i).is_err());
		CHECK(fmt_factory()->get_instruction_operand(db, i).is_err());
		std::string output;
		CHECK(fmt_factory()->format_operand(db, output, i).is_err());
	}
	CHECK(fmt_factory()->get_formatter_operand(db, 0).is_err());
}
#endif

void methods_panic_if_invalid_operand_or_instruction_operand(const FormatterFactory& fmt_factory) {
	{
		Instruction instruction;
		instruction.set_code(Code::Mov_rm64_r64);
		instruction.set_op0_register(Register::RAX);
		instruction.set_op1_register(Register::RCX);
		const std::uint32_t num_ops = 2;
		const std::uint32_t num_instr_ops = 2;
		CHECK_EQ(fmt_factory()->operand_count(instruction), num_ops);
		CHECK_EQ(instruction.op_count(), num_instr_ops);
		CHECK(fmt_factory()->op_access(instruction, num_ops).is_err());
		CHECK(fmt_factory()->get_instruction_operand(instruction, num_ops).is_err());
		CHECK(fmt_factory()->get_formatter_operand(instruction, num_instr_ops).is_err());
		std::string output;
		CHECK(fmt_factory()->format_operand(instruction, output, num_ops).is_err());
	}

	{
		const Instruction invalid;
		CHECK(fmt_factory()->op_access(invalid, 0).is_err());
		CHECK(fmt_factory()->get_instruction_operand(invalid, 0).is_err());
		CHECK(fmt_factory()->get_formatter_operand(invalid, 0).is_err());
		std::string output;
		CHECK(fmt_factory()->format_operand(invalid, output, 0).is_err());
	}

#if ICED_X86_TESTS_HAS_ENCODER
	{
		const std::uint8_t data[8] = {};
		const auto db = Instruction::with_declare_byte(data, sizeof(data)).value();
		CHECK_EQ(db.declare_data_len(), static_cast<std::size_t>(8));
		check_declare_data(fmt_factory, db);
	}

	{
		const std::uint16_t data[4] = {};
		const auto dw = Instruction::with_declare_word(data, 4).value();
		CHECK_EQ(dw.declare_data_len(), static_cast<std::size_t>(4));
		check_declare_data(fmt_factory, dw);
	}

	{
		const std::uint32_t data[2] = {8, 8};
		const auto dd = Instruction::with_declare_dword(data, 2).value();
		CHECK_EQ(dd.declare_data_len(), static_cast<std::size_t>(2));
		check_declare_data(fmt_factory, dd);
	}

	{
		const std::uint64_t data[1] = {};
		const auto dq = Instruction::with_declare_qword(data, 1).value();
		CHECK_EQ(dq.declare_data_len(), static_cast<std::size_t>(1));
		check_declare_data(fmt_factory, dq);
	}
#endif
}

void test_op_index(const FormatterFactory& fmt_factory) {
	auto formatter = fmt_factory();
	std::array<std::optional<std::uint32_t>, IcedConstants::MAX_OP_COUNT> instr_to_formatter;
	for (const auto& info : decoder_tests(true, false)) {
		const auto bytes = to_vec_u8(info.hex_bytes());
		auto decoder = create_decoder(info.bitness(), bytes, info.ip(), info.decoder_options()).decoder;
		const auto instruction = decoder.decode();
		CHECK_EQ(instruction.code(), info.code());

		instr_to_formatter.fill(std::nullopt);

		const std::uint32_t formatter_op_count = formatter->operand_count(instruction);
		const std::uint32_t instruction_op_count = instruction.op_count();

		std::uint32_t instr_op_used = 0;
		REQUIRE(instruction_op_count <= 32); // uint is 32 bits
		for (std::uint32_t formatter_op_index = 0; formatter_op_index < formatter_op_count; formatter_op_index++) {
			const auto instr_op_index_result = formatter->get_instruction_operand(instruction, formatter_op_index);
			REQUIRE(instr_op_index_result.is_ok());
			const auto instr_op_index = instr_op_index_result.value();
			if (instr_op_index) {
				REQUIRE(*instr_op_index < instruction_op_count);
				instr_to_formatter[*instr_op_index] = formatter_op_index;

				const auto op_access = formatter->op_access(instruction, formatter_op_index);
				REQUIRE(op_access.is_ok());
				CHECK(!op_access.value().has_value());

				const std::uint32_t instr_op_bit = 1U << *instr_op_index;
				CHECK_MSG((instr_op_used & instr_op_bit) == 0, "More than one formatter operand index maps to the same instruction op index");
				instr_op_used |= instr_op_bit;

				const auto formatter_operand = formatter->get_formatter_operand(instruction, *instr_op_index);
				REQUIRE(formatter_operand.is_ok());
				CHECK(formatter_operand.value() == std::optional<std::uint32_t>(formatter_op_index));
			}
			else {
				const auto op_access = formatter->op_access(instruction, formatter_op_index);
				REQUIRE(op_access.is_ok());
				CHECK(op_access.value().has_value());
			}
		}

		for (std::uint32_t instr_op_index = 0; instr_op_index < instruction_op_count; instr_op_index++) {
			const auto formatter_op_index = formatter->get_formatter_operand(instruction, instr_op_index);
			REQUIRE(formatter_op_index.is_ok());
			CHECK(formatter_op_index.value() == instr_to_formatter[instr_op_index]);
		}

		for (std::uint32_t instr_op_index = instruction_op_count; instr_op_index < IcedConstants::MAX_OP_COUNT; instr_op_index++)
			CHECK(fmt_factory()->get_formatter_operand(instruction, instr_op_index).is_err());
	}
}

} // namespace iced_x86::tests
