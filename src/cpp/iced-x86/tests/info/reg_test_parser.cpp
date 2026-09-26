// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "info/info_test_cases.hpp"
#include "generated/misc_instr_info_test_constants.hpp"
#include "generated/register_flags.hpp"
#include "generated/test_dicts.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/str_utils.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace iced_x86::tests::instr_info {

static RegisterInfoTestCase read_next_register_info_test_case(const std::unordered_map<std::string_view, std::uint32_t>& to_flags, const std::string& line, std::uint32_t line_number) {
	static_assert(MiscInstrInfoTestConstants::REGISTER_ELEMS_PER_LINE == 7, "");
	const std::vector<std::string_view> elems = splitn(line, MiscInstrInfoTestConstants::REGISTER_ELEMS_PER_LINE, ',');
	if (elems.size() != MiscInstrInfoTestConstants::REGISTER_ELEMS_PER_LINE)
		throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));

	RegisterInfoTestCase tc;
	tc.line_number = line_number;
	tc.register_ = to_register(elems[0]);
	tc.number = to_u32(elems[1]);
	tc.base = to_register(elems[2]);
	tc.full_register = to_register(elems[3]);
	tc.full_register32 = to_register(elems[4]);
	tc.size = to_u32(elems[5]);
	for (std::string_view value : split_whitespace(elems[6])) {
		auto it = to_flags.find(value);
		if (it == to_flags.end())
			throw std::runtime_error("Invalid flags value: " + std::string(value));
		tc.flags |= it->second;
	}
	return tc;
}

std::vector<RegisterInfoTestCase> read_register_info_test_cases(const std::string& filename) {
	const std::unordered_map<std::string_view, std::uint32_t> to_flags = create_dict(REGISTER_FLAGS_DICT);
	std::vector<RegisterInfoTestCase> result;
	std::uint32_t line_number = 0;
	for (const std::string& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || line[0] == '#')
			continue;
		try {
			result.push_back(read_next_register_info_test_case(to_flags, line, line_number));
		}
		catch (const std::exception& ex) {
			throw std::runtime_error("Error parsing register info test case file '" + filename + "', line " + std::to_string(line_number) + ": " +
									 ex.what());
		}
	}
	return result;
}

} // namespace iced_x86::tests::instr_info
