// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace iced_x86::tests {

/// Reads a file with sections (port of Rust's `test_utils/section_file_reader.rs`):
///
///	# comment
///	[section-name]
///	line
///	line
///
/// Empty lines and lines starting with `#` are ignored.
class SectionFileReader {
public:
	/// Called for each line in a section. `id` is the section's id. Throw a `std::runtime_error` if the line is invalid.
	using Handler = std::function<void(std::uint32_t id, std::string_view line)>;

	/// `infos` = (section name, id)
	explicit SectionFileReader(std::vector<std::pair<std::string, std::uint32_t>> infos);

	/// Reads all lines and calls `handler` for each non-empty, non-comment line.
	/// Throws `std::runtime_error("Error parsing file '<filename>', line <n>: <msg>")` if there's an error
	/// (eg. the handler threw a `std::exception`) or "Couldn't open file <filename>" if the file couldn't be opened.
	void read(const std::string& filename, const Handler& handler) const;

private:
	const std::pair<std::string, std::uint32_t>* get_section(std::string_view section_name) const noexcept;

	std::vector<std::pair<std::string, std::uint32_t>> infos_;
};

} // namespace iced_x86::tests
