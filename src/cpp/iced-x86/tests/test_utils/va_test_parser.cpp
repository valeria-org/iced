// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_utils/va_test_parser.hpp"
#include "iced_x86/decoder_options.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/str_utils.hpp"

#include <exception>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace iced_x86::tests {

static std::optional<VirtualAddressTestCase> read_next_test_case(std::string_view line) {
	const auto elems = split(line, ',');
	if (elems.size() != 9)
		throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));

	VirtualAddressTestCase tc{};
	tc.bitness = to_u32(elems[0]);
	if (is_ignored_code(trim(elems[1])))
		return std::nullopt;
	tc.hex_bytes = std::string(trim(elems[2]));
	(void)to_vec_u8(tc.hex_bytes);
	tc.operand = to_i32(elems[3]);
	tc.used_mem_index = to_i32(elems[4]);
	tc.element_index = static_cast<std::size_t>(to_u32(elems[5]));
	tc.expected_value = to_u64(elems[6]);
	const auto dec_opt_str = trim(elems[7]);
	tc.decoder_options = dec_opt_str.empty() ? DecoderOptions::NONE : to_decoder_options(dec_opt_str);

	for (const auto elem : split_whitespace(elems[8])) {
		if (elem.empty())
			continue;
		const auto kv = split(elem, '=');
		if (kv.size() != 2)
			throw std::runtime_error("Expected key=value: " + std::string(elem));
		const auto key = kv[0];
		const auto value_str = kv[1];

		VARegisterValue reg_value{};
		if (contains(key, ';')) {
			const auto parts = split(key, ';');
			if (parts.size() != 3)
				throw std::runtime_error("Invalid number of semicolons: " + std::to_string(parts.size() - 1));
			reg_value.register_ = to_register(parts[0]);
			reg_value.element_index = static_cast<std::size_t>(to_u32(parts[1]));
			reg_value.element_size = static_cast<std::size_t>(to_u32(parts[2]));
		}
		else {
			reg_value.register_ = to_register(key);
			reg_value.element_index = 0;
			reg_value.element_size = 0;
		}
		reg_value.value = to_u64(value_str);
		tc.register_values.push_back(reg_value);
	}

	return tc;
}

std::vector<VirtualAddressTestCase> read_va_test_cases(const std::string& filename) {
	const auto lines = read_lines(filename);
	std::vector<VirtualAddressTestCase> result;
	for (std::size_t i = 0; i < lines.size(); i++) {
		const std::string_view line = lines[i];
		if (line.empty() || starts_with(line, "#"))
			continue;
		std::optional<VirtualAddressTestCase> tc;
		std::string err;
		try {
			tc = read_next_test_case(line);
		}
		catch (const std::exception& ex) {
			err = ex.what();
		}
		if (!err.empty())
			throw std::runtime_error("Error parsing virtual address test case file '" + filename + "', line " + std::to_string(i + 1) + ": " + err);
		if (tc)
			result.push_back(std::move(*tc));
	}
	return result;
}

const std::vector<VirtualAddressTestCase>& get_va_test_cases() {
	static const std::vector<VirtualAddressTestCase> test_cases = read_va_test_cases(get_instruction_unit_tests_dir() + "/VirtualAddressTests.txt");
	return test_cases;
}

} // namespace iced_x86::tests
