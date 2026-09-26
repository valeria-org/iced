// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "info/info_test_cases.hpp"
#include "generated/memory_size_flags.hpp"
#include "generated/misc_instr_info_test_constants.hpp"
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

static MemorySizeInfoTestCase read_next_memory_size_info_test_case(const std::unordered_map<std::string_view, std::uint32_t>& to_flags, const std::string& line, std::uint32_t line_number) {
	static_assert(MiscInstrInfoTestConstants::MEMORY_SIZE_ELEMS_PER_LINE == 6, "");
	const std::vector<std::string_view> elems = splitn(line, MiscInstrInfoTestConstants::MEMORY_SIZE_ELEMS_PER_LINE, ',');
	if (elems.size() != MiscInstrInfoTestConstants::MEMORY_SIZE_ELEMS_PER_LINE)
		throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));

	MemorySizeInfoTestCase tc;
	tc.line_number = line_number;
	tc.memory_size = to_memory_size(elems[0]);
	tc.size = to_u32(elems[1]);
	tc.element_size = to_u32(elems[2]);
	tc.element_type = to_memory_size(elems[3]);
	tc.element_count = to_u32(elems[4]);
	for (std::string_view value : split_whitespace(elems[5])) {
		auto it = to_flags.find(value);
		if (it == to_flags.end())
			throw std::runtime_error("Invalid flags value: " + std::string(value));
		tc.flags |= it->second;
	}
	return tc;
}

std::vector<MemorySizeInfoTestCase> read_memory_size_info_test_cases(const std::string& filename) {
	const std::unordered_map<std::string_view, std::uint32_t> to_flags = create_dict(MEMORY_SIZE_FLAGS_DICT);
	std::vector<MemorySizeInfoTestCase> result;
	std::uint32_t line_number = 0;
	for (const std::string& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || line[0] == '#')
			continue;
		try {
			result.push_back(read_next_memory_size_info_test_case(to_flags, line, line_number));
		}
		catch (const std::exception& ex) {
			throw std::runtime_error("Error parsing memory size info test case file '" + filename + "', line " + std::to_string(line_number) + ": " +
									 ex.what());
		}
	}
	return result;
}

} // namespace iced_x86::tests::instr_info
