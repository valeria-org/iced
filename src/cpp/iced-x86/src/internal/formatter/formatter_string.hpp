// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace iced_x86::internal {

/// A lowercase string and its uppercase version (Rust: `formatter::FormatterString`)
class FormatterString {
public:
	FormatterString() = default;

	/// Creates a new instance. `lower` must be a lowercase ASCII string
	explicit FormatterString(std::string lower) : lower_(std::move(lower)), upper_(to_upper(lower_)) {}
	/// Creates a new instance. `lower` must be a lowercase ASCII string
	explicit FormatterString(std::string_view lower) : FormatterString(std::string(lower)) {}
	/// Creates a new instance. `lower` must be a lowercase ASCII string
	explicit FormatterString(const char* lower) : FormatterString(std::string(lower)) {}

	/// Creates a `FormatterString` of each string
	static std::vector<FormatterString> with_strings(std::vector<std::string> strings) {
		std::vector<FormatterString> result;
		result.reserve(strings.size());
		for (auto& s : strings)
			result.emplace_back(std::move(s));
		return result;
	}

	/// Length of the string
	std::size_t len() const noexcept { return lower_.size(); }

	/// `true` if it's the empty string
	bool is_default() const noexcept { return lower_.empty(); }

	/// Gets the lowercase (`upper == false`) or the uppercase (`upper == true`) string
	std::string_view get(bool upper) const noexcept { return upper ? std::string_view(upper_) : std::string_view(lower_); }

	/// Gets the lowercase string
	const std::string& lower() const noexcept { return lower_; }

	/// Gets the uppercase string
	const std::string& upper() const noexcept { return upper_; }

private:
	static std::string to_upper(const std::string& s) {
		std::string result(s);
		for (auto& c : result) {
			if (c >= 'a' && c <= 'z')
				c = static_cast<char>(c - 'a' + 'A');
		}
		return result;
	}

	std::string lower_;
	std::string upper_;
};

/// A read-only slice of `const FormatterString*` (Rust: `&'static [&'static FormatterString]`)
class FormatterStringSlice {
public:
	constexpr FormatterStringSlice() noexcept : data_(nullptr), size_(0) {}
	constexpr FormatterStringSlice(const FormatterString* const* data, std::size_t size) noexcept : data_(data), size_(size) {}
	template <std::size_t N>
	constexpr FormatterStringSlice(const std::array<const FormatterString*, N>& array) noexcept : data_(array.data()), size_(N) {}
	FormatterStringSlice(const std::vector<const FormatterString*>& vec) noexcept : data_(vec.data()), size_(vec.size()) {}

	constexpr const FormatterString* const* begin() const noexcept { return data_; }
	constexpr const FormatterString* const* end() const noexcept { return data_ + size_; }
	constexpr std::size_t size() const noexcept { return size_; }
	constexpr bool empty() const noexcept { return size_ == 0; }
	constexpr const FormatterString* operator[](std::size_t index) const noexcept { return data_[index]; }

private:
	const FormatterString* const* data_;
	std::size_t size_;
};

} // namespace iced_x86::internal
