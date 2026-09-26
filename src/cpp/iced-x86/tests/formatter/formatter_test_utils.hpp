// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Shared formatter test helpers (Rust: formatter/tests/*.rs). They're used by the tests of all formatters
// (tests/formatter/<syntax>/*.cpp). The fast formatter versions are in tests/formatter/fast/fast_test_utils.hpp.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "generated/options_props.hpp"
#include "iced_x86/cc_a.hpp"
#include "iced_x86/cc_ae.hpp"
#include "iced_x86/cc_b.hpp"
#include "iced_x86/cc_be.hpp"
#include "iced_x86/cc_e.hpp"
#include "iced_x86/cc_g.hpp"
#include "iced_x86/cc_ge.hpp"
#include "iced_x86/cc_l.hpp"
#include "iced_x86/cc_le.hpp"
#include "iced_x86/cc_ne.hpp"
#include "iced_x86/cc_np.hpp"
#include "iced_x86/cc_p.hpp"
#include "iced_x86/code.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/fast_formatter_options.hpp"
#include "iced_x86/formatter.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/memory_size.hpp"
#include "iced_x86/memory_size_options.hpp"
#include "iced_x86/number_base.hpp"
#include "iced_x86/symbol_resolver.hpp"

// TEMPORARY until the encoder lands on `cpp` (non_decoded_tests.hpp + Instruction::with_declare_*())
#if __has_include("test_utils/non_decoded_tests.hpp")
#define ICED_X86_TESTS_HAS_ENCODER 1
#else
#define ICED_X86_TESTS_HAS_ENCODER 0
#endif

namespace iced_x86::tests {

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/mod.rs, instr_infos.rs

/// A test case in `Formatter/InstructionInfos{16,32,64}[_Misc].txt`
struct FormatterInstructionInfo {
	std::uint32_t bitness;
	std::string hex_bytes;
	std::uint64_t ip;
	Code code;
	std::uint32_t options; // DecoderOptions
};

/// All test cases in a `Formatter/InstructionInfos*.txt` file and the (0-based) indexes of the ignored test cases
struct FormatterInstructionInfos {
	std::vector<FormatterInstructionInfo> infos;
	std::unordered_set<std::uint32_t> ignored;
};

/// Gets all instructions in `Formatter/InstructionInfos{bitness}[_Misc].txt` (the file is only read once)
const FormatterInstructionInfos& get_formatter_instruction_infos(std::uint32_t bitness, bool is_misc);

/// Reads all lines in a file, ignoring empty lines and comments (lines that start with `#`)
std::vector<std::string> get_lines_ignore_comments(const std::string& filename);

/// Reads `Formatter/{dir}/Test{bitness}_{file_part}.txt`
std::vector<std::string> get_formatted_lines(std::uint32_t bitness, std::string_view dir, std::string_view file_part);

/// Removes the lines whose (0-based) index is in `ignored`
std::vector<std::string> filter_removed_code_tests(std::vector<std::string> strings, const std::unordered_set<std::uint32_t>& ignored);

/// Creates a formatter
using FormatterFactory = std::function<std::unique_ptr<Formatter>()>;
/// Creates a formatter that uses a symbol resolver
using FormatterResolverFactory = std::function<std::unique_ptr<Formatter>(std::unique_ptr<SymbolResolver> symbol_resolver)>;
/// Initializes a decoder before it decodes the instruction
using InitDecoderFn = std::function<void(Decoder& decoder)>;

/// Decodes an instruction and verifies its code and IPs (`hex_bytes` is used to create the decoder so it must outlive the result)
Instruction decode_test_instruction(std::uint32_t bitness, const std::vector<std::uint8_t>& bytes, std::uint64_t ip, Code code,
									std::uint32_t decoder_options, const InitDecoderFn& init_decoder = nullptr);

/// Formats all instructions in `Formatter/InstructionInfos{bitness}[_Misc].txt` and compares them with
/// `Formatter/{dir}/Test{bitness}_{filename}.txt` (Rust: `formatter_test()`)
void formatter_test(std::uint32_t bitness, std::string_view dir, std::string_view filename, bool is_misc, const FormatterFactory& fmt_factory);

/// Formats all non-decodable instructions (see `test_utils/non_decoded_tests.hpp`) and compares them with
/// `Formatter/{dir}/Test{bitness}_{filename}.txt` (Rust: `formatter_test_nondec()`)
void formatter_test_nondec(std::uint32_t bitness, std::string_view dir, std::string_view filename, const FormatterFactory& fmt_factory);

/// Formats the instruction (whole instruction, mnemonic + each operand, mnemonic + all operands) and compares it with `formatted_string`
void format_test_instruction_core(const Instruction& instruction, const std::string& formatted_string, Formatter& formatter,
								  const std::string& context = std::string());

/// Decodes the instruction, formats it and compares it with `formatted_string` (Rust: `simple_format_test()`)
void simple_format_test(std::uint32_t bitness, const std::string& hex_bytes, std::uint64_t ip, Code code, std::uint32_t decoder_options,
						std::uint32_t line_number, const std::string& formatted_string, Formatter& formatter, const InitDecoderFn& init_decoder);

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/opt_value.rs, options_parser.rs, opts_info.rs, options_test_case_parser.rs, opts_infos.rs

/// Rust: `OptionValue::DecoderOptions(u32)`
struct DecoderOptionsValue {
	std::uint32_t value;
};

/// A parsed option value (Rust: `OptionValue`). `std::monostate` = `OptionValue::Ignore`
class OptionValue {
public:
	using Value = std::variant<std::monostate, bool, std::int32_t, std::uint64_t, std::string, MemorySizeOptions, NumberBase, CC_b, CC_ae, CC_e, CC_ne,
							   CC_be, CC_a, CC_p, CC_np, CC_l, CC_ge, CC_le, CC_g, DecoderOptionsValue>;

	explicit OptionValue(Value value) : value_(std::move(value)) {}

	bool to_bool() const { return std::get<bool>(value_); }
	std::uint32_t to_i32_as_u32() const {
		const std::int32_t value = std::get<std::int32_t>(value_);
		return value <= 0 ? 0 : static_cast<std::uint32_t>(value);
	}
	std::uint64_t to_u64() const { return std::get<std::uint64_t>(value_); }
	const std::string& to_str() const { return std::get<std::string>(value_); }
	MemorySizeOptions to_memory_size_options() const { return std::get<MemorySizeOptions>(value_); }
	NumberBase to_number_base() const { return std::get<NumberBase>(value_); }
	template <typename T>
	T get() const {
		return std::get<T>(value_);
	}
	const Value& value() const noexcept { return value_; }

	/// Gets all decoder options
	static std::uint32_t get_decoder_options(const std::vector<std::pair<OptionsProps, OptionValue>>& props);

	/// Initializes a formatter option
	void initialize_options(FormatterOptions& options, OptionsProps property) const;
	/// Initializes a fast formatter option
	void initialize_options_fast(FastFormatterOptions& options, OptionsProps property) const;
	/// Initializes the decoder (eg. the IP)
	void initialize_decoder(Decoder& decoder, OptionsProps property) const;

private:
	Value value_;
};

/// Parses `key=value`. Throws if it's invalid (Rust: `parse_option()`)
std::pair<OptionsProps, OptionValue> parse_option(std::string_view key_value);

/// A test case in `Formatter/Options*.txt` (Rust: `OptionsInstructionInfo`)
struct OptionsInstructionInfo {
	std::uint32_t bitness;
	std::string hex_bytes;
	std::uint64_t ip;
	std::uint32_t decoder_options;
	std::uint32_t line_number;
	Code code;
	std::vector<std::pair<OptionsProps, OptionValue>> vec;

	void initialize_options(FormatterOptions& options) const;
	void initialize_options_fast(FastFormatterOptions& options) const;
	void initialize_decoder(Decoder& decoder) const;
};

/// All test cases in an options file and the (0-based) indexes of the ignored test cases
struct OptionsInstructionInfos {
	std::vector<OptionsInstructionInfo> infos;
	std::unordered_set<std::uint32_t> ignored;
};

/// Parses an options file (Rust: `OptionsTestParser`)
OptionsInstructionInfos read_options_test_file(const std::string& filename);

/// `Formatter/Options.Common.txt` (the file is only read once)
const OptionsInstructionInfos& get_common_options_infos();
/// `Formatter/Options.txt` (the file is only read once)
const OptionsInstructionInfos& get_all_options_infos();

/// Returns (options test case, expected formatted string) pairs (Rust: `filter_infos()`)
std::vector<std::pair<const OptionsInstructionInfo*, std::string>> filter_options_infos(std::string_view dir, std::string_view file_part,
																						   const OptionsInstructionInfos& infos);

/// Rust: `formatter::tests::options::test_format_file_common()`
void test_format_file_common(std::string_view dir, std::string_view file_part, const FormatterFactory& fmt_factory);
/// Rust: `formatter::tests::options::test_format_file_all()`
void test_format_file_all(std::string_view dir, std::string_view file_part, const FormatterFactory& fmt_factory);
/// Rust: `formatter::tests::options::test_format_file()`
void test_format_file(std::string_view dir, std::string_view file_part, std::string_view options_file, const FormatterFactory& fmt_factory);

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/sym_res.rs, sym_res_test_case.rs, sym_res_test_parser.rs

struct SymbolResultTestCase {
	std::uint64_t address;
	std::uint64_t symbol_address;
	std::uint32_t address_size;
	std::uint32_t flags; // SymbolFlags
	std::optional<MemorySize> memory_size;
	std::vector<std::string> symbol_parts;
};

struct SymbolResolverTestCase {
	std::uint32_t bitness;
	std::string hex_bytes;
	std::uint64_t ip;
	std::uint32_t decoder_options;
	std::uint32_t line_number;
	Code code;
	std::vector<std::pair<OptionsProps, OptionValue>> options;
	std::vector<SymbolResultTestCase> symbol_results;
};

struct SymbolResolverTestCases {
	std::vector<SymbolResolverTestCase> infos;
	std::unordered_set<std::uint32_t> ignored;
};

/// `Formatter/SymbolResolverTests.txt` (the file is only read once)
const SymbolResolverTestCases& get_symbol_resolver_test_cases();

/// A symbol resolver that returns the symbols of a `SymbolResolverTestCase`
class TestSymbolResolver final : public SymbolResolver {
public:
	explicit TestSymbolResolver(const SymbolResolverTestCase& info) : info_(&info) {}
	std::optional<SymbolResult> symbol(const Instruction& instruction, std::uint32_t operand, std::optional<std::uint32_t> instruction_operand,
									   std::uint64_t address, std::uint32_t address_size) override;

private:
	const SymbolResolverTestCase* info_;
	std::vector<SymResTextPart> vec_;
};

/// Gets the test cases and the expected formatted strings (`Formatter/{dir}/{filename}.txt`)
std::pair<const SymbolResolverTestCases*, std::vector<std::string>> get_symbol_resolver_infos_and_lines(std::string_view dir, std::string_view filename);

/// Rust: `symbol_resolver_test()`
void symbol_resolver_test(std::string_view dir, std::string_view filename, const FormatterResolverFactory& fmt_factory);

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/mnemonic_opts_parser.rs

struct MnemonicOptionsTestCase {
	std::string hex_bytes;
	Code code;
	std::uint32_t bitness;
	std::uint64_t ip;
	std::string formatted_string;
	std::uint32_t flags; // FormatMnemonicOptions
};

/// Parses a `MnemonicOptions.txt` file (Rust: `MnemonicOptionsTestParser`)
std::vector<MnemonicOptionsTestCase> read_mnemonic_options_test_cases(const std::string& filename);

/// Tests `format_mnemonic_options()` with all test cases in `Formatter/{dir}/MnemonicOptions.txt`
/// (Rust: each formatter's `misc::format_mnemonic_options()` test)
void format_mnemonic_options_test(std::string_view dir, const FormatterFactory& fmt_factory);

// ---------------------------------------------------------------------------------------------------------------------
// Rust: formatter/tests/number.rs, registers.rs, misc.rs

/// Rust: `number_tests()`
void number_tests(const FormatterFactory& fmt_factory);

/// Rust: `register_tests()`
void register_tests(std::string_view dir, std::string_view file_part, const FormatterFactory& fmt_factory);

/// Rust: `misc::methods_panic_if_invalid_operand_or_instruction_operand()`
void methods_panic_if_invalid_operand_or_instruction_operand(const FormatterFactory& fmt_factory);

/// Rust: `misc::test_op_index()`
void test_op_index(const FormatterFactory& fmt_factory);

} // namespace iced_x86::tests
