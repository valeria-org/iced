// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/masm/tests/symres.rs, sym_opts.rs, sym_opts_parser.rs

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "formatter/formatter_test_utils.hpp"
#include "formatter/masm/masm_fmt_factory.hpp"
#include "generated/masm_symbol_test_flags.hpp"
#include "generated/test_dicts.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/formatter_text_kind.hpp"
#include "iced_x86/symbol_flags.hpp"
#include "iced_x86/symbol_resolver.hpp"
#include "test_framework.hpp"
#include "test_utils.hpp"
#include "test_utils/decoder_test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/str_utils.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/masm/symres/symres") { symbol_resolver_test("Masm", "SymbolResolverTests", masm::create_resolver); }

namespace {

struct SymbolOptionsTestCase {
	std::string hex_bytes;
	std::uint32_t bitness;
	std::uint64_t ip;
	std::string formatted_string;
	std::uint32_t flags; // SymbolTestFlags
};

// Rust: SymbolOptionsTestParser
std::optional<SymbolOptionsTestCase> read_next_test_case(const std::unordered_map<std::string_view, std::uint32_t>& to_flags, const std::string& line) {
	const auto elems = split(line, ',');
	if (elems.size() != 5)
		throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));

	SymbolOptionsTestCase tc;
	tc.hex_bytes = std::string(trim(elems[0]));
	static_cast<void>(to_vec_u8(tc.hex_bytes));
	if (is_ignored_code(elems[1]))
		return std::nullopt;
	tc.bitness = to_u32(elems[2]);
	tc.ip = get_default_ip(tc.bitness);
	tc.formatted_string = std::string(trim(elems[3]));
	for (auto& c : tc.formatted_string) {
		if (c == '|')
			c = ',';
	}
	tc.flags = SymbolTestFlags::NONE;
	for (const auto value : split_whitespace(elems[4])) {
		if (value.empty())
			continue;
		const auto it = to_flags.find(value);
		if (it == to_flags.end())
			throw std::runtime_error("Invalid flags value: " + std::string(value));
		tc.flags |= it->second;
	}
	return tc;
}

std::vector<SymbolOptionsTestCase> read_symbol_options_test_cases(const std::string& filename) {
	const auto to_flags = create_dict(MASM_SYMBOL_TEST_FLAGS_DICT);
	std::vector<SymbolOptionsTestCase> result;
	std::uint32_t line_number = 0;
	for (const auto& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || line[0] == '#')
			continue;
		try {
			auto tc = read_next_test_case(to_flags, line);
			if (tc)
				result.push_back(std::move(*tc));
		} catch (const std::exception& ex) {
			throw std::runtime_error("Error parsing symbol options test case file '" + filename + "', line " + std::to_string(line_number) + ": " +
									 ex.what());
		}
	}
	return result;
}

class SymbolResolverImpl final : public SymbolResolver {
public:
	explicit SymbolResolverImpl(std::uint32_t flags) : flags_(flags) {}

	std::optional<SymbolResult> symbol(const Instruction& instruction, std::uint32_t operand, std::optional<std::uint32_t> instruction_operand,
									   std::uint64_t address, std::uint32_t address_size) override {
		static_cast<void>(instruction);
		static_cast<void>(operand);
		static_cast<void>(address_size);
		if (instruction_operand == 1U && (flags_ & SymbolTestFlags::SYMBOL) != 0) {
			return SymbolResult::with_str_kind_flags(address, "symbol", FormatterTextKind::Data,
													 (flags_ & SymbolTestFlags::SIGNED) != 0 ? SymbolFlags::SIGNED : SymbolFlags::NONE);
		}
		return std::nullopt;
	}

private:
	std::uint32_t flags_;
};

} // namespace

TEST_CASE("formatter/masm/symres/symbol_options") {
	const auto test_cases = read_symbol_options_test_cases(get_formatter_unit_tests_dir() + "/Masm/SymbolOptions.txt");
	CHECK(!test_cases.empty());
	for (const auto& tc : test_cases) {
		const auto bytes = to_vec_u8(tc.hex_bytes);
		auto decoder = create_decoder(tc.bitness, bytes, tc.ip, DecoderOptions::NONE).decoder;
		const auto instruction = decoder.decode();

		auto formatter = masm::create_resolver(std::make_unique<SymbolResolverImpl>(tc.flags));
		formatter->options_mut().set_masm_symbol_displ_in_brackets((tc.flags & SymbolTestFlags::SYMBOL_DISPL_IN_BRACKETS) != 0);
		formatter->options_mut().set_masm_displ_in_brackets((tc.flags & SymbolTestFlags::DISPL_IN_BRACKETS) != 0);
		formatter->options_mut().set_rip_relative_addresses((tc.flags & SymbolTestFlags::RIP) != 0);
		formatter->options_mut().set_show_zero_displacements((tc.flags & SymbolTestFlags::SHOW_ZERO_DISPLACEMENTS) != 0);
		formatter->options_mut().set_masm_add_ds_prefix32((tc.flags & SymbolTestFlags::NO_ADD_DS_PREFIX32) == 0);

		std::string output;
		formatter->format(instruction, output);
		CHECK_MSG(output == tc.formatted_string, "hex bytes: " + tc.hex_bytes + ", expected: " + tc.formatted_string + ", actual: " + output);
	}
}
