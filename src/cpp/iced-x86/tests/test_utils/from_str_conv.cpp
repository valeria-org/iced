// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_utils/from_str_conv.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace iced_x86::tests {

template <typename T>
static T get_value(const std::unordered_map<std::string_view, T>& dict, std::string_view value, const char* type_name) {
	value = trim(value);
	const auto it = dict.find(value);
	if (it == dict.end())
		throw std::runtime_error(std::string("Invalid ") + type_name + " value: " + std::string(value));
	return it->second;
}

static std::int32_t try_parse_hex_char(char c) noexcept {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

// Returns the length of the UTF-8 char at s[pos]
static std::size_t utf8_char_len(std::string_view s, std::size_t pos) noexcept {
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

std::vector<std::uint8_t> to_vec_u8(std::string_view hex_data) {
	std::vector<std::uint8_t> bytes;
	bytes.reserve(hex_data.size() / 2);
	// Iterate over all non-whitespace chars (UTF-8), same as Rust's `hex_data.chars().filter(|c| !c.is_whitespace())`
	std::size_t pos = 0;
	auto next_char = [&](std::int32_t& digit) -> bool {
		while (pos < hex_data.size()) {
			const auto len = utf8_char_len(hex_data, pos);
			const auto c = hex_data.substr(pos, len);
			pos += len;
			if (trim(c).empty())
				continue;
			digit = len == 1 ? try_parse_hex_char(c[0]) : -1;
			return true;
		}
		return false;
	};
	for (;;) {
		std::int32_t hi, lo;
		if (!next_char(hi))
			break;
		if (!next_char(lo))
			throw std::runtime_error("Missing hex digit in string: '" + std::string(hex_data) + "'");
		if (hi < 0 || lo < 0)
			throw std::runtime_error("Invalid hex string: '" + std::string(hex_data) + "'");
		bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
	}
	return bytes;
}

// Same as Rust's `u64::from_str_radix(s, radix)` (radix is 10 or 16)
static bool try_parse_u64_radix(std::string_view s, std::uint32_t radix, std::uint64_t& result) noexcept {
	if (s.empty())
		return false;
	if (s[0] == '+') {
		s.remove_prefix(1);
		if (s.empty())
			return false;
	}
	std::uint64_t value = 0;
	for (char c : s) {
		std::uint32_t digit;
		if (c >= '0' && c <= '9')
			digit = static_cast<std::uint32_t>(c - '0');
		else if (radix == 16 && c >= 'a' && c <= 'f')
			digit = static_cast<std::uint32_t>(c - 'a' + 10);
		else if (radix == 16 && c >= 'A' && c <= 'F')
			digit = static_cast<std::uint32_t>(c - 'A' + 10);
		else
			return false;
		if (digit >= radix)
			return false;
		if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / radix)
			return false;
		value = value * radix + digit;
	}
	result = value;
	return true;
}

static bool try_to_u64(std::string_view value, std::uint64_t& result) noexcept {
	value = trim(value);
	if (starts_with(value, "0x"))
		return try_parse_u64_radix(value.substr(2), 16, result);
	return try_parse_u64_radix(trim(value), 10, result);
}

static bool try_to_i64(std::string_view value, std::int64_t& result) noexcept {
	auto unsigned_value = trim(value);
	bool negate = false;
	if (starts_with(unsigned_value, "-")) {
		unsigned_value.remove_prefix(1);
		negate = true;
	}
	std::uint64_t u;
	bool ok;
	if (starts_with(unsigned_value, "0x"))
		ok = try_parse_u64_radix(unsigned_value.substr(2), 16, u);
	else
		ok = try_parse_u64_radix(trim(unsigned_value), 10, u);
	if (!ok)
		return false;
	// Rust: `(value as i64).wrapping_mul(mult)`
	result = static_cast<std::int64_t>(negate ? 0 - u : u);
	return true;
}

static std::runtime_error invalid_number(std::string_view value) { return std::runtime_error("Invalid number: " + std::string(value)); }

std::uint64_t to_u64(std::string_view value) {
	std::uint64_t result;
	if (!try_to_u64(value, result))
		throw invalid_number(trim(value));
	return result;
}

std::int64_t to_i64(std::string_view value) {
	std::int64_t result;
	if (!try_to_i64(value, result))
		throw invalid_number(value);
	return result;
}

template <typename T>
static T to_unsigned(std::string_view value) {
	value = trim(value);
	std::uint64_t v64;
	if (try_to_u64(value, v64) && v64 <= static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
		return static_cast<T>(v64);
	throw invalid_number(value);
}

template <typename T>
static T to_signed(std::string_view value) {
	value = trim(value);
	std::int64_t v64;
	if (try_to_i64(value, v64) && static_cast<std::int64_t>(std::numeric_limits<T>::min()) <= v64 &&
		v64 <= static_cast<std::int64_t>(std::numeric_limits<T>::max()))
		return static_cast<T>(v64);
	throw invalid_number(value);
}

std::uint32_t to_u32(std::string_view value) { return to_unsigned<std::uint32_t>(value); }
std::int32_t to_i32(std::string_view value) { return to_signed<std::int32_t>(value); }
std::uint16_t to_u16(std::string_view value) { return to_unsigned<std::uint16_t>(value); }
std::int16_t to_i16(std::string_view value) { return to_signed<std::int16_t>(value); }
std::uint8_t to_u8(std::string_view value) { return to_unsigned<std::uint8_t>(value); }
std::int8_t to_i8(std::string_view value) { return to_signed<std::int8_t>(value); }

static const std::unordered_map<std::string_view, Code>& get_code_dict() {
	static const auto dict = create_dict(CODE_NAME_VALUES);
	return dict;
}

static const std::unordered_map<std::string_view, Register>& get_register_dict() {
	static const auto dict = create_dict(REGISTER_NAME_VALUES);
	return dict;
}

static const std::unordered_map<std::string_view, NumberBase>& get_number_base_dict() {
	static const auto dict = create_dict(NUMBER_BASE_NAME_VALUES);
	return dict;
}

Code to_code(std::string_view value) { return get_value(get_code_dict(), value, "Code"); }

bool is_ignored_code(std::string_view value) {
	// All features are enabled in the C++ code (VEX, EVEX, XOP, 3DNow!, MVEX) so only check the removed Code values
	static const auto ignored = create_set(IGNORED_CODE_NAMES);
	return ignored.find(trim(value)) != ignored.end();
}

std::vector<std::string_view> code_names() {
	const auto& dict = get_code_dict();
	std::vector<std::pair<std::string_view, Code>> values(dict.begin(), dict.end());
	std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) { return static_cast<std::uint32_t>(a.second) < static_cast<std::uint32_t>(b.second); });
	std::vector<std::string_view> names;
	names.reserve(values.size());
	for (const auto& kv : values)
		names.push_back(kv.first);
	return names;
}

Mnemonic to_mnemonic(std::string_view value) {
	static const auto dict = create_dict(MNEMONIC_NAME_VALUES);
	return get_value(dict, value, "Mnemonic");
}

Register to_register(std::string_view value) {
	if (trim(value).empty())
		return Register::None;
	return get_value(get_register_dict(), value, "Register");
}

std::unordered_map<std::string, Register> clone_register_hashmap() {
	const auto& dict = get_register_dict();
	std::unordered_map<std::string, Register> result;
	result.reserve(dict.size());
	for (const auto& kv : dict)
		result.emplace(std::string(kv.first), kv.second);
	return result;
}

MemorySize to_memory_size(std::string_view value) {
	static const auto dict = create_dict(MEMORY_SIZE_NAME_VALUES);
	return get_value(dict, value, "MemorySize");
}

DecoderError to_decoder_error(std::string_view value) {
	static const auto dict = create_dict(DECODER_ERROR_NAME_VALUES);
	return get_value(dict, value, "DecoderError");
}

std::uint32_t to_decoder_options(std::string_view value) {
	static const auto dict = create_dict(DECODER_OPTIONS_NAME_VALUES);
	return get_value(dict, value, "DecoderOptions");
}

EncodingKind to_encoding_kind(std::string_view value) {
	static const auto dict = create_dict(ENCODING_KIND_NAME_VALUES);
	return get_value(dict, value, "EncodingKind");
}

TupleType to_tuple_type(std::string_view value) {
	static const auto dict = create_dict(TUPLE_TYPE_NAME_VALUES);
	return get_value(dict, value, "TupleType");
}

MvexConvFn to_mvex_conv_fn(std::string_view value) {
	static const auto dict = create_dict(MVEX_CONV_FN_NAME_VALUES);
	return get_value(dict, value, "MvexConvFn");
}

MvexTupleTypeLutKind to_mvex_tuple_type_lut_kind(std::string_view value) {
	static const auto dict = create_dict(MVEX_TUPLE_TYPE_LUT_KIND_NAME_VALUES);
	return get_value(dict, value, "MvexTupleTypeLutKind");
}

CpuidFeature to_cpuid_features(std::string_view value) {
	static const auto dict = create_dict(CPUID_FEATURE_NAME_VALUES);
	return get_value(dict, value, "CpuidFeature");
}

FlowControl to_flow_control(std::string_view value) {
	static const auto dict = create_dict(FLOW_CONTROL_NAME_VALUES);
	return get_value(dict, value, "FlowControl");
}

OpCodeOperandKind to_op_code_operand_kind(std::string_view value) {
	static const auto dict = create_dict(OP_CODE_OPERAND_KIND_NAME_VALUES);
	return get_value(dict, value, "OpCodeOperandKind");
}

ConditionCode to_condition_code(std::string_view value) {
	static const auto dict = create_dict(CONDITION_CODE_NAME_VALUES);
	return get_value(dict, value, "ConditionCode");
}

OptionsProps to_options_props(std::string_view value) {
	static const auto dict = create_dict(OPTIONS_PROPS_NAME_VALUES);
	return get_value(dict, value, "OptionsProps");
}

MemorySizeOptions to_memory_size_options(std::string_view value) {
	static const auto dict = create_dict(MEMORY_SIZE_OPTIONS_NAME_VALUES);
	return get_value(dict, value, "MemorySizeOptions");
}

NumberBase to_number_base(std::string_view value) { return get_value(get_number_base_dict(), value, "NumberBase"); }

std::size_t number_base_len() { return get_number_base_dict().size(); }

bool to_boolean(std::string_view value) {
	value = trim(value);
	if (value == "false")
		return false;
	if (value == "true")
		return true;
	throw std::runtime_error("Invalid boolean value: " + std::string(value));
}

CC_b to_cc_b(std::string_view value) {
	static const auto dict = create_dict(CC_B_NAME_VALUES);
	return get_value(dict, value, "CC_b");
}

CC_ae to_cc_ae(std::string_view value) {
	static const auto dict = create_dict(CC_AE_NAME_VALUES);
	return get_value(dict, value, "CC_ae");
}

CC_e to_cc_e(std::string_view value) {
	static const auto dict = create_dict(CC_E_NAME_VALUES);
	return get_value(dict, value, "CC_e");
}

CC_ne to_cc_ne(std::string_view value) {
	static const auto dict = create_dict(CC_NE_NAME_VALUES);
	return get_value(dict, value, "CC_ne");
}

CC_be to_cc_be(std::string_view value) {
	static const auto dict = create_dict(CC_BE_NAME_VALUES);
	return get_value(dict, value, "CC_be");
}

CC_a to_cc_a(std::string_view value) {
	static const auto dict = create_dict(CC_A_NAME_VALUES);
	return get_value(dict, value, "CC_a");
}

CC_p to_cc_p(std::string_view value) {
	static const auto dict = create_dict(CC_P_NAME_VALUES);
	return get_value(dict, value, "CC_p");
}

CC_np to_cc_np(std::string_view value) {
	static const auto dict = create_dict(CC_NP_NAME_VALUES);
	return get_value(dict, value, "CC_np");
}

CC_l to_cc_l(std::string_view value) {
	static const auto dict = create_dict(CC_L_NAME_VALUES);
	return get_value(dict, value, "CC_l");
}

CC_ge to_cc_ge(std::string_view value) {
	static const auto dict = create_dict(CC_GE_NAME_VALUES);
	return get_value(dict, value, "CC_ge");
}

CC_le to_cc_le(std::string_view value) {
	static const auto dict = create_dict(CC_LE_NAME_VALUES);
	return get_value(dict, value, "CC_le");
}

CC_g to_cc_g(std::string_view value) {
	static const auto dict = create_dict(CC_G_NAME_VALUES);
	return get_value(dict, value, "CC_g");
}

} // namespace iced_x86::tests
