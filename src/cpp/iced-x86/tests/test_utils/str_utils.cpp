// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_utils/str_utils.hpp"

#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace iced_x86::tests {

// Unicode White_Space chars (Rust's char::is_whitespace()):
//	U+0009-U+000D, U+0020, U+0085, U+00A0, U+1680, U+2000-U+200A, U+2028, U+2029, U+202F, U+205F, U+3000

static bool is_ws_2(unsigned char b0, unsigned char b1) noexcept {
	// U+0085, U+00A0
	return b0 == 0xC2 && (b1 == 0x85 || b1 == 0xA0);
}

static bool is_ws_3(unsigned char b0, unsigned char b1, unsigned char b2) noexcept {
	switch (b0) {
	case 0xE1:
		// U+1680
		return b1 == 0x9A && b2 == 0x80;
	case 0xE2:
		// U+2000-U+200A, U+2028, U+2029, U+202F
		if (b1 == 0x80)
			return (b2 >= 0x80 && b2 <= 0x8A) || b2 == 0xA8 || b2 == 0xA9 || b2 == 0xAF;
		// U+205F
		return b1 == 0x81 && b2 == 0x9F;
	case 0xE3:
		// U+3000
		return b1 == 0x80 && b2 == 0x80;
	default:
		return false;
	}
}

static bool is_ws_1(unsigned char b) noexcept { return b == ' ' || (b >= 0x09 && b <= 0x0D); }

// Returns the length of the whitespace char at s[pos] or 0 if it's not a whitespace char
static std::size_t ws_len_at(std::string_view s, std::size_t pos) noexcept {
	const auto remaining = s.size() - pos;
	const auto b0 = static_cast<unsigned char>(s[pos]);
	if (is_ws_1(b0))
		return 1;
	if (remaining >= 2 && is_ws_2(b0, static_cast<unsigned char>(s[pos + 1])))
		return 2;
	if (remaining >= 3 && is_ws_3(b0, static_cast<unsigned char>(s[pos + 1]), static_cast<unsigned char>(s[pos + 2])))
		return 3;
	return 0;
}

// Returns the length of the whitespace char that ends at s[end - 1] or 0 if it's not a whitespace char
static std::size_t ws_len_before(std::string_view s, std::size_t end) noexcept {
	if (end >= 1 && is_ws_1(static_cast<unsigned char>(s[end - 1])))
		return 1;
	if (end >= 2 && is_ws_2(static_cast<unsigned char>(s[end - 2]), static_cast<unsigned char>(s[end - 1])))
		return 2;
	if (end >= 3 && is_ws_3(static_cast<unsigned char>(s[end - 3]), static_cast<unsigned char>(s[end - 2]), static_cast<unsigned char>(s[end - 1])))
		return 3;
	return 0;
}

// Returns the length of the UTF-8 char at s[pos]
static std::size_t char_len_at(std::string_view s, std::size_t pos) noexcept {
	const auto b0 = static_cast<unsigned char>(s[pos]);
	std::size_t len;
	if (b0 < 0x80)
		len = 1;
	else if (b0 >= 0xC0 && b0 <= 0xDF)
		len = 2;
	else if (b0 >= 0xE0 && b0 <= 0xEF)
		len = 3;
	else if (b0 >= 0xF0 && b0 <= 0xF7)
		len = 4;
	else
		len = 1;
	const auto remaining = s.size() - pos;
	return len <= remaining ? len : remaining;
}

std::string_view trim_start(std::string_view s) noexcept {
	std::size_t pos = 0;
	while (pos < s.size()) {
		const auto len = ws_len_at(s, pos);
		if (len == 0)
			break;
		pos += len;
	}
	return s.substr(pos);
}

std::string_view trim_end(std::string_view s) noexcept {
	std::size_t end = s.size();
	while (end > 0) {
		const auto len = ws_len_before(s, end);
		if (len == 0)
			break;
		end -= len;
	}
	return s.substr(0, end);
}

std::string_view trim(std::string_view s) noexcept { return trim_end(trim_start(s)); }

std::vector<std::string_view> split(std::string_view s, char separator) {
	std::vector<std::string_view> result;
	std::size_t start = 0;
	for (;;) {
		const auto index = s.find(separator, start);
		if (index == std::string_view::npos) {
			result.push_back(s.substr(start));
			break;
		}
		result.push_back(s.substr(start, index - start));
		start = index + 1;
	}
	return result;
}

std::vector<std::string_view> splitn(std::string_view s, std::size_t n, char separator) {
	std::vector<std::string_view> result;
	if (n == 0)
		return result;
	std::size_t start = 0;
	for (;;) {
		const auto index = result.size() + 1 == n ? std::string_view::npos : s.find(separator, start);
		if (index == std::string_view::npos) {
			result.push_back(s.substr(start));
			break;
		}
		result.push_back(s.substr(start, index - start));
		start = index + 1;
	}
	return result;
}

std::vector<std::string_view> split(std::string_view s, std::string_view separator) {
	if (separator.empty())
		throw std::invalid_argument("split(): empty separator");
	std::vector<std::string_view> result;
	std::size_t start = 0;
	for (;;) {
		const auto index = s.find(separator, start);
		if (index == std::string_view::npos) {
			result.push_back(s.substr(start));
			break;
		}
		result.push_back(s.substr(start, index - start));
		start = index + separator.size();
	}
	return result;
}

std::vector<std::string_view> split_whitespace(std::string_view s) {
	std::vector<std::string_view> result;
	std::size_t pos = 0;
	std::size_t start = 0;
	while (pos < s.size()) {
		const auto len = ws_len_at(s, pos);
		if (len != 0) {
			if (start != pos)
				result.push_back(s.substr(start, pos - start));
			pos += len;
			start = pos;
		}
		else
			pos += char_len_at(s, pos);
	}
	if (start != s.size())
		result.push_back(s.substr(start));
	return result;
}

std::string to_ascii_lowercase(std::string_view s) {
	std::string result(s);
	for (auto& c : result) {
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	}
	return result;
}

std::string to_ascii_uppercase(std::string_view s) {
	std::string result(s);
	for (auto& c : result) {
		if (c >= 'a' && c <= 'z')
			c = static_cast<char>(c - 'a' + 'A');
	}
	return result;
}

std::vector<std::string> read_lines(const std::string& filename) {
	std::ifstream file(filename, std::ios::in | std::ios::binary);
	if (!file)
		throw std::runtime_error("Couldn't open file " + filename);
	std::vector<std::string> lines;
	std::string line;
	while (std::getline(file, line)) {
		// Only remove the '\r' if the line ends in "\r\n" (same as Rust)
		if (!file.eof() && !line.empty() && line.back() == '\r')
			line.pop_back();
		lines.push_back(std::move(line));
		line.clear();
	}
	if (file.bad())
		throw std::runtime_error("Couldn't read file " + filename);
	return lines;
}

} // namespace iced_x86::tests
