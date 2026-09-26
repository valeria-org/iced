// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "internal/formatter/num_fmt.hpp"

#include <cstddef>

namespace iced_x86::internal {

static constexpr std::uint64_t SMALL_POSITIVE_NUMBER = 9;

static constexpr std::string_view SMALL_DECIMAL_VALUES[SMALL_POSITIVE_NUMBER + 1] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

// clang-format off
static constexpr std::uint64_t DIVS[20] = {
	1ULL,
	10ULL,
	100ULL,
	1000ULL,
	10000ULL,
	100000ULL,
	1000000ULL,
	10000000ULL,
	100000000ULL,
	1000000000ULL,
	10000000000ULL,
	100000000000ULL,
	1000000000000ULL,
	10000000000000ULL,
	100000000000000ULL,
	1000000000000000ULL,
	10000000000000000ULL,
	100000000000000000ULL,
	1000000000000000000ULL,
	10000000000000000000ULL,
};
// clang-format on

static inline void write_hexadecimal(std::string& sb, std::uint64_t value, std::uint32_t digit_group_size, std::string_view digit_separator,
									 std::uint32_t digits, bool upper, bool leading_zero) {
	if (digits == 0) {
		digits = 1;
		std::uint64_t tmp = value;
		for (;;) {
			tmp >>= 4;
			if (tmp == 0)
				break;
			digits++;
		}
	}

	const std::uint32_t hex_high = upper ? static_cast<std::uint32_t>('A') - 10 : static_cast<std::uint32_t>('a') - 10;
	if (leading_zero && digits < 17 && ((value >> ((digits - 1) << 2)) & 0xF) > 9)
		digits++; // Another 0
	const bool use_digit_sep = digit_group_size > 0 && !digit_separator.empty();
	for (std::uint32_t i = 0; i < digits; i++) {
		const std::uint32_t index = digits - i - 1;
		const auto digit = index >= 16 ? 0U : static_cast<std::uint32_t>((value >> (index << 2)) & 0xF);
		if (digit > 9)
			sb.push_back(static_cast<char>(digit + hex_high));
		else
			sb.push_back(static_cast<char>(digit + '0'));
		if (use_digit_sep && index > 0 && (index % digit_group_size) == 0)
			sb.append(digit_separator.data(), digit_separator.size());
	}
}

static inline void write_decimal(std::string& sb, std::uint64_t value, std::uint32_t digit_group_size, std::string_view digit_separator,
								 std::uint32_t digits) {
	if (digits == 0) {
		digits = 1;
		std::uint64_t tmp = value;
		for (;;) {
			tmp /= 10;
			if (tmp == 0)
				break;
			digits++;
		}
	}

	const bool use_digit_sep = digit_group_size > 0 && !digit_separator.empty();
	for (std::uint32_t i = 0; i < digits; i++) {
		const std::uint32_t index = digits - i - 1;
		if (index < sizeof(DIVS) / sizeof(DIVS[0])) {
			const auto digit = static_cast<std::uint32_t>(value / DIVS[index] % 10);
			sb.push_back(static_cast<char>(digit + '0'));
		}
		else
			sb.push_back('0');
		if (use_digit_sep && index > 0 && (index % digit_group_size) == 0)
			sb.append(digit_separator.data(), digit_separator.size());
	}
}

static inline void write_octal(std::string& sb, std::uint64_t value, std::uint32_t digit_group_size, std::string_view digit_separator,
							   std::uint32_t digits, std::string_view prefix) {
	if (digits == 0) {
		digits = 1;
		std::uint64_t tmp = value;
		for (;;) {
			tmp >>= 3;
			if (tmp == 0)
				break;
			digits++;
		}
	}

	if (!prefix.empty()) {
		// The prefix is part of the number so that a digit separator can be placed
		// between the "prefix" and the rest of the number, eg. "0" + "1234" with
		// digit separator "`" and group size = 2 is "0`12`34" and not "012`34".
		// Other prefixes, eg. "0o" prefix: 0o12`34 and never 0o`12`34.
		if (prefix == "0") {
			if (digits < 23 && ((value >> ((digits - 1) * 3)) & 7) != 0)
				digits++; // Another 0
		}
		else
			sb.append(prefix.data(), prefix.size());
	}

	const bool use_digit_sep = digit_group_size > 0 && !digit_separator.empty();
	for (std::uint32_t i = 0; i < digits; i++) {
		const std::uint32_t index = digits - i - 1;
		const auto digit = index >= 22 ? 0U : static_cast<std::uint32_t>((value >> (index * 3)) & 7);
		sb.push_back(static_cast<char>(digit + '0'));
		if (use_digit_sep && index > 0 && (index % digit_group_size) == 0)
			sb.append(digit_separator.data(), digit_separator.size());
	}
}

static inline void write_binary(std::string& sb, std::uint64_t value, std::uint32_t digit_group_size, std::string_view digit_separator,
								std::uint32_t digits) {
	if (digits == 0) {
		digits = 1;
		std::uint64_t tmp = value;
		for (;;) {
			tmp >>= 1;
			if (tmp == 0)
				break;
			digits++;
		}
	}

	const bool use_digit_sep = digit_group_size > 0 && !digit_separator.empty();
	for (std::uint32_t i = 0; i < digits; i++) {
		const std::uint32_t index = digits - i - 1;
		const auto digit = index >= 64 ? 0U : static_cast<std::uint32_t>((value >> index) & 1);
		sb.push_back(static_cast<char>(digit + '0'));
		if (use_digit_sep && index > 0 && (index % digit_group_size) == 0)
			sb.append(digit_separator.data(), digit_separator.size());
	}
}

std::string_view NumberFormatter::format_unsigned_integer(const FormatterOptions& formatter_options, const NumberFormattingOptions& options,
														  std::uint64_t value, std::uint32_t value_size, std::uint32_t flags) {
	sb_.clear();
	if ((flags & Flags::ADD_MINUS_SIGN) != 0)
		sb_.push_back('-');
	std::string_view suffix;
	switch (options.number_base) {
	case NumberBase::Hexadecimal:
		if ((flags & Flags::SMALL_HEX_NUMBERS_IN_DECIMAL) != 0 && value <= SMALL_POSITIVE_NUMBER) {
			const auto prefix = formatter_options.decimal_prefix();
			sb_.append(prefix.data(), prefix.size());
			const auto s = SMALL_DECIMAL_VALUES[value];
			sb_.append(s.data(), s.size());
			suffix = formatter_options.decimal_suffix();
		}
		else {
			sb_.append(options.prefix.data(), options.prefix.size());
			write_hexadecimal(sb_, value, options.digit_group_size, options.digit_separator,
							  (flags & Flags::LEADING_ZEROS) != 0 ? (value_size + 3) >> 2 : 0, options.uppercase_hex,
							  options.add_leading_zero_to_hex_numbers && options.prefix.empty());
			suffix = options.suffix;
		}
		break;

	case NumberBase::Decimal:
		sb_.append(options.prefix.data(), options.prefix.size());
		write_decimal(sb_, value, options.digit_group_size, options.digit_separator, 0);
		suffix = options.suffix;
		break;

	case NumberBase::Octal:
		write_octal(sb_, value, options.digit_group_size, options.digit_separator, (flags & Flags::LEADING_ZEROS) != 0 ? (value_size + 2) / 3 : 0,
					options.prefix);
		suffix = options.suffix;
		break;

	case NumberBase::Binary:
	default:
		sb_.append(options.prefix.data(), options.prefix.size());
		write_binary(sb_, value, options.digit_group_size, options.digit_separator, (flags & Flags::LEADING_ZEROS) != 0 ? value_size : 0);
		suffix = options.suffix;
		break;
	}

	sb_.append(suffix.data(), suffix.size());
	return sb_;
}

} // namespace iced_x86::internal
