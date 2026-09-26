// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_utils/section_file_reader.hpp"
#include "test_utils/str_utils.hpp"

#include <cstddef>
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>

namespace iced_x86::tests {

SectionFileReader::SectionFileReader(std::vector<std::pair<std::string, std::uint32_t>> infos) : infos_(std::move(infos)) {}

void SectionFileReader::read(const std::string& filename, const Handler& handler) const {
	const auto lines = read_lines(filename);
	std::string_view current_section_name;
	std::uint32_t current_section_id = std::numeric_limits<std::uint32_t>::max();
	for (std::size_t i = 0; i < lines.size(); i++) {
		const std::string_view line = lines[i];
		const auto line_number = i + 1;
		std::optional<std::string> err_str;
		if (line.empty() || starts_with(line, "#")) {
		}
		else if (starts_with(line, "[")) {
			if (!ends_with(line, "]"))
				err_str = "Missing ']'";
			else {
				const auto section_name = line.substr(1, line.size() - 2);
				if (const auto* new_info = get_section(section_name)) {
					current_section_name = new_info->first;
					current_section_id = new_info->second;
				}
				else
					err_str = "Unknown section name: " + std::string(section_name);
			}
		}
		else {
			if (current_section_name.empty())
				err_str = "Missing section";
			else {
				try {
					handler(current_section_id, line);
				}
				catch (const std::exception& ex) {
					err_str = ex.what();
				}
			}
		}
		if (err_str)
			throw std::runtime_error("Error parsing file '" + filename + "', line " + std::to_string(line_number) + ": " + *err_str);
	}
}

const std::pair<std::string, std::uint32_t>* SectionFileReader::get_section(std::string_view section_name) const noexcept {
	for (const auto& info : infos_) {
		if (info.first == section_name)
			return &info;
	}
	return nullptr;
}

} // namespace iced_x86::tests
