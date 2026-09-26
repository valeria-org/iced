// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// String helpers with the same semantics as the Rust `str` methods used by the Rust tests.
// Whitespace = Unicode `White_Space` (same as Rust's `char::is_whitespace()`), strings are UTF-8.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace iced_x86::tests {

/// Same as Rust's `str::trim()`
std::string_view trim(std::string_view s) noexcept;
/// Same as Rust's `str::trim_start()`
std::string_view trim_start(std::string_view s) noexcept;
/// Same as Rust's `str::trim_end()`
std::string_view trim_end(std::string_view s) noexcept;

/// Same as Rust's `str::split(char)`: always returns at least one element (empty elements are kept)
std::vector<std::string_view> split(std::string_view s, char separator);
/// Same as Rust's `str::split(&str)`: always returns at least one element (empty elements are kept). `separator` must not be empty.
std::vector<std::string_view> split(std::string_view s, std::string_view separator);
/// Same as Rust's `str::splitn()`: returns at most `n` elements, the last one contains the rest of the string
std::vector<std::string_view> splitn(std::string_view s, std::size_t n, char separator);
/// Same as Rust's `str::split_whitespace()`: never returns empty elements
std::vector<std::string_view> split_whitespace(std::string_view s);

/// Same as Rust's `str::starts_with()`
constexpr bool starts_with(std::string_view s, std::string_view prefix) noexcept {
	return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}
/// Same as Rust's `str::ends_with()`
constexpr bool ends_with(std::string_view s, std::string_view suffix) noexcept {
	return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}
/// Same as Rust's `str::contains()`
constexpr bool contains(std::string_view s, std::string_view value) noexcept { return s.find(value) != std::string_view::npos; }
/// Same as Rust's `str::contains(char)`
constexpr bool contains(std::string_view s, char value) noexcept { return s.find(value) != std::string_view::npos; }

/// Same as Rust's `str::to_ascii_lowercase()`
std::string to_ascii_lowercase(std::string_view s);
/// Same as Rust's `str::to_ascii_uppercase()`
std::string to_ascii_uppercase(std::string_view s);

/// Reads all lines of a text file, same as Rust's `BufReader::new(File::open(filename)?).lines()`:
/// lines are split at `\n` and a `\r` before the `\n` is removed. Throws `std::runtime_error("Couldn't open file <filename>")`
/// if the file can't be opened.
std::vector<std::string> read_lines(const std::string& filename);

} // namespace iced_x86::tests
