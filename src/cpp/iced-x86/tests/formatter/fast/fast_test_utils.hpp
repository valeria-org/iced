// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Fast formatter test helpers (Rust: the *_fast() functions in formatter/tests/*.rs and formatter/fast/tests/{fmt_factory,not_fast_fmt}.rs)

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "formatter/formatter_test_utils.hpp"
#include "iced_x86/fast_formatter.hpp"
#include "test_framework.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/non_decoded_tests.hpp"

namespace iced_x86::tests {

// Since SpecializedFormatterTraitOptions::INTERNAL_IS_FAST_FORMATTER exists, we have code
// paths that aren't tested completely. We create a new FastFormatter with that const
// set to false so these new paths are tested.
struct NotFastFormatterTraitOptions : SpecializedFormatterTraitOptions {
	static constexpr bool INTERNAL_IS_FAST_FORMATTER = false;
	static constexpr bool ENABLE_SYMBOL_RESOLVER = DefaultFastFormatterTraitOptions::ENABLE_SYMBOL_RESOLVER;
	static constexpr bool ENABLE_DB_DW_DD_DQ = DefaultFastFormatterTraitOptions::ENABLE_DB_DW_DD_DQ;
	static constexpr bool verify_output_has_enough_bytes_left() noexcept { return DefaultFastFormatterTraitOptions::verify_output_has_enough_bytes_left(); }
	static constexpr bool space_after_operand_separator(const FastFormatterOptions& options) noexcept {
		return DefaultFastFormatterTraitOptions::space_after_operand_separator(options);
	}
	static constexpr bool rip_relative_addresses(const FastFormatterOptions& options) noexcept {
		return DefaultFastFormatterTraitOptions::rip_relative_addresses(options);
	}
	static constexpr bool use_pseudo_ops(const FastFormatterOptions& options) noexcept { return DefaultFastFormatterTraitOptions::use_pseudo_ops(options); }
	static constexpr bool show_symbol_address(const FastFormatterOptions& options) noexcept {
		return DefaultFastFormatterTraitOptions::show_symbol_address(options);
	}
	static constexpr bool always_show_segment_register(const FastFormatterOptions& options) noexcept {
		return DefaultFastFormatterTraitOptions::always_show_segment_register(options);
	}
	static constexpr bool always_show_memory_size(const FastFormatterOptions& options) noexcept {
		return DefaultFastFormatterTraitOptions::always_show_memory_size(options);
	}
	static constexpr bool uppercase_hex(const FastFormatterOptions& options) noexcept { return DefaultFastFormatterTraitOptions::uppercase_hex(options); }
	static constexpr bool use_hex_prefix(const FastFormatterOptions& options) noexcept {
		return DefaultFastFormatterTraitOptions::use_hex_prefix(options);
	}
};

template <typename TraitOptions>
using FastFormatterFactory = std::function<std::unique_ptr<SpecializedFormatter<TraitOptions>>()>;
template <typename TraitOptions>
using FastFormatterResolverFactory = std::function<std::unique_ptr<SpecializedFormatter<TraitOptions>>(std::unique_ptr<SymbolResolver> symbol_resolver)>;

// Rust: formatter/fast/tests/fmt_factory.rs

template <typename TraitOptions>
std::unique_ptr<SpecializedFormatter<TraitOptions>> fast_create_default() {
	return std::make_unique<SpecializedFormatter<TraitOptions>>();
}

template <typename TraitOptions>
std::unique_ptr<SpecializedFormatter<TraitOptions>> fast_create_inverted() {
	auto fmt = std::make_unique<SpecializedFormatter<TraitOptions>>();

	bool opt = fmt->options().space_after_operand_separator() ^ true;
	fmt->options_mut().set_space_after_operand_separator(opt);

	opt = fmt->options().rip_relative_addresses() ^ true;
	fmt->options_mut().set_rip_relative_addresses(opt);

	opt = fmt->options().use_pseudo_ops() ^ true;
	fmt->options_mut().set_use_pseudo_ops(opt);

	opt = fmt->options().show_symbol_address() ^ true;
	fmt->options_mut().set_show_symbol_address(opt);

	opt = fmt->options().always_show_segment_register() ^ true;
	fmt->options_mut().set_always_show_segment_register(opt);

	opt = fmt->options().always_show_memory_size() ^ true;
	fmt->options_mut().set_always_show_memory_size(opt);

	opt = fmt->options().uppercase_hex() ^ true;
	fmt->options_mut().set_uppercase_hex(opt);

	opt = fmt->options().use_hex_prefix() ^ true;
	fmt->options_mut().set_use_hex_prefix(opt);

	return fmt;
}

template <typename TraitOptions>
std::unique_ptr<SpecializedFormatter<TraitOptions>> fast_create_options() {
	auto fmt = std::make_unique<SpecializedFormatter<TraitOptions>>();
	fmt->options_mut().set_rip_relative_addresses(true);
	return fmt;
}

template <typename TraitOptions>
std::unique_ptr<SpecializedFormatter<TraitOptions>> fast_create_resolver(std::unique_ptr<SymbolResolver> symbol_resolver) {
	auto fmt = std::make_unique<SpecializedFormatter<TraitOptions>>(SpecializedFormatter<TraitOptions>::try_with_options(std::move(symbol_resolver)).value());
	fmt->options_mut().set_rip_relative_addresses(true);
	return fmt;
}

// Formats the instruction with the buffer API and returns the formatted string. It also verifies all output paths:
// - a `MAX_FMT_INSTR_LEN + 1` byte buffer (the fast path if there's no symbol resolver; all writes are verified since
//   verify_output_has_enough_bytes_left() returns true)
// - an exact fit buffer, truncated output (buffer is 1 byte too small), 1 byte buffer, 0 byte buffer (scratch buffer path)
// - the `std::string` convenience wrapper
template <typename TraitOptions>
std::string fast_format(SpecializedFormatter<TraitOptions>& formatter, const Instruction& instruction) {
	using Formatter = SpecializedFormatter<TraitOptions>;
	char buffer[Formatter::MAX_FMT_INSTR_LEN + 1];
	std::memset(buffer, 'X', sizeof(buffer));
	const std::size_t len = formatter.format(instruction, buffer);
	std::string result;
	if (len < sizeof(buffer)) {
		REQUIRE(buffer[len] == '\0');
		result.assign(buffer, len);
	}
	else {
		// Only possible with a symbol resolver
		REQUIRE(buffer[sizeof(buffer) - 1] == '\0');
		std::vector<char> big(len + 100, 'X');
		REQUIRE(formatter.format(instruction, big.data(), big.size()) == len);
		REQUIRE(big[len] == '\0');
		result.assign(big.data(), len);
		CHECK(std::memcmp(buffer, result.data(), sizeof(buffer) - 1) == 0);
	}
	REQUIRE(result.find('\0') == std::string::npos);

	std::vector<char> exact(len + 1, 'X');
	CHECK(formatter.format(instruction, exact.data(), exact.size()) == len);
	CHECK(exact[len] == '\0');
	CHECK(std::memcmp(exact.data(), result.data(), len) == 0);

	if (len > 0) {
		std::vector<char> small(len, 'X');
		CHECK(formatter.format(instruction, small.data(), small.size()) == len);
		CHECK(small[len - 1] == '\0');
		CHECK(std::memcmp(small.data(), result.data(), len - 1) == 0);
	}

	char one = 'X';
	CHECK(formatter.format(instruction, &one, 1) == len);
	CHECK(one == '\0');
	CHECK(formatter.format(instruction, nullptr, 0) == len);

	std::string str("abc");
	formatter.format(instruction, str);
	CHECK(str == "abc" + result);

	return result;
}

// Rust: formatter/tests/mod.rs

template <typename TraitOptions>
void format_test_instruction_fast_core(const Instruction& instruction, const std::string& formatted_string, SpecializedFormatter<TraitOptions>& formatter,
									   const std::string& context) {
	const std::string actual_formatted_string = fast_format(formatter, instruction);
	CHECK_MSG(actual_formatted_string == formatted_string,
			  "Formatted string: '" + actual_formatted_string + "' != expected: '" + formatted_string + "' " + context);
}

template <typename TraitOptions>
void formatter_test_fast(std::uint32_t bitness, std::string_view dir, std::string_view filename, bool is_misc,
						 const FastFormatterFactory<TraitOptions>& fmt_factory) {
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
		format_test_instruction_fast_core(instruction, lines[i], *formatter,
										  "(" + std::to_string(info.bitness) + "-bit, hex bytes: " + info.hex_bytes + ", Code: " + to_string(info.code) + ")");
	}
}

template <typename TraitOptions>
void formatter_test_nondec_fast(std::uint32_t bitness, std::string_view dir, std::string_view filename,
								const FastFormatterFactory<TraitOptions>& fmt_factory) {
	const auto& instrs = get_non_decoded_infos(bitness);
	const auto lines = get_formatted_lines(bitness, dir, filename);
	REQUIRE_MSG(instrs.size() == lines.size(), "Instrs len (" + std::to_string(instrs.size()) + ") != fmt len (" + std::to_string(lines.size()) +
												   "); dir=" + std::string(dir) + ", filename: " + std::string(filename));
	for (std::size_t i = 0; i < lines.size(); i++) {
		auto formatter = fmt_factory();
		format_test_instruction_fast_core(instrs[i].instruction, lines[i], *formatter,
										  "(" + std::to_string(bitness) + "-bit, hex bytes: " + instrs[i].hex_bytes + ")");
	}
}

template <typename TraitOptions>
void simple_format_test_fast(std::uint32_t bitness, const std::string& hex_bytes, std::uint64_t ip, Code code, std::uint32_t decoder_options,
							 std::uint32_t line_number, const std::string& formatted_string, SpecializedFormatter<TraitOptions>& formatter,
							 const InitDecoderFn& init_decoder) {
	const auto bytes = to_vec_u8(hex_bytes);
	const auto instruction = decode_test_instruction(bitness, bytes, ip, code, decoder_options, init_decoder);
	const std::string output = fast_format(formatter, instruction);
	CHECK_MSG(output == formatted_string, "Formatted string: '" + output + "' != expected: '" + formatted_string + "', line " + std::to_string(line_number));
}

// Rust: formatter/tests/options.rs

template <typename TraitOptions>
void test_format_fast(const std::vector<std::pair<const OptionsInstructionInfo*, std::string>>& infos,
					  const FastFormatterFactory<TraitOptions>& fmt_factory) {
	for (const auto& [tc, formatted_string] : infos) {
		auto formatter = fmt_factory();
		tc->initialize_options_fast(formatter->options_mut());
		simple_format_test_fast(tc->bitness, tc->hex_bytes, tc->ip, tc->code, tc->decoder_options, tc->line_number, formatted_string, *formatter,
								[tc = tc](Decoder& decoder) { tc->initialize_decoder(decoder); });
	}
}

template <typename TraitOptions>
void test_format_file_common_fast(std::string_view dir, std::string_view file_part, const FastFormatterFactory<TraitOptions>& fmt_factory) {
	test_format_fast(filter_options_infos(dir, file_part, get_common_options_infos()), fmt_factory);
}

template <typename TraitOptions>
void test_format_file_fast(std::string_view dir, std::string_view file_part, std::string_view options_file,
						   const FastFormatterFactory<TraitOptions>& fmt_factory) {
	const auto infos = read_options_test_file(get_formatter_unit_tests_dir() + "/" + std::string(dir) + "/" + std::string(options_file) + ".txt");
	test_format_fast(filter_options_infos(dir, file_part, infos), fmt_factory);
}

// Rust: formatter/tests/sym_res.rs

template <typename TraitOptions>
void symbol_resolver_test_fast(std::string_view dir, std::string_view filename, const FastFormatterResolverFactory<TraitOptions>& fmt_factory) {
	const auto [infos, formatted_lines] = get_symbol_resolver_infos_and_lines(dir, filename);
	for (std::size_t i = 0; i < formatted_lines.size(); i++) {
		const auto& info = infos->infos[i];
		auto formatter = fmt_factory(std::make_unique<TestSymbolResolver>(info));
		for (const auto& props : info.options)
			props.second.initialize_options_fast(formatter->options_mut(), props.first);
		simple_format_test_fast(info.bitness, info.hex_bytes, info.ip, info.code, info.decoder_options, info.line_number, formatted_lines[i], *formatter,
								[&info](Decoder& decoder) {
									for (const auto& props : info.options)
										props.second.initialize_decoder(decoder, props.first);
								});
	}
}

} // namespace iced_x86::tests
