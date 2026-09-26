// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// String -> value conversion functions used by the tests (port of Rust's `test_utils/from_str_conv`).
// All functions trim the input (like Rust) and throw `std::runtime_error` (same message as the Rust `Err(String)`)
// if the input is invalid.

#pragma once

#include "generated/from_str_conv_tables.hpp"
#include "generated/options_props.hpp"
#include "generated/test_dicts.hpp"
#include "iced_x86/cc_a.hpp"
#include "iced_x86/cc_ae.hpp"
#include "iced_x86/cc_b.hpp"
#include "iced_x86/cc_be.hpp"
#include "iced_x86/cc_e.hpp"
#include "iced_x86/cc_g.hpp"
#include "iced_x86/cc_ge.hpp"
#include "iced_x86/cc_l.hpp"
#include "iced_x86/cc_le.hpp"
#include "iced_x86/cc_ne.hpp"
#include "iced_x86/cc_np.hpp"
#include "iced_x86/cc_p.hpp"
#include "iced_x86/code.hpp"
#include "iced_x86/condition_code.hpp"
#include "iced_x86/cpuid_feature.hpp"
#include "iced_x86/decoder_error.hpp"
#include "iced_x86/encoding_kind.hpp"
#include "iced_x86/flow_control.hpp"
#include "iced_x86/memory_size.hpp"
#include "iced_x86/memory_size_options.hpp"
#include "iced_x86/mnemonic.hpp"
#include "iced_x86/mvex_conv_fn.hpp"
#include "iced_x86/mvex_tuple_type_lut_kind.hpp"
#include "iced_x86/number_base.hpp"
#include "iced_x86/op_code_operand_kind.hpp"
#include "iced_x86/register.hpp"
#include "iced_x86/tuple_type.hpp"
#include "test_utils/name_value.hpp"
#include "test_utils/str_utils.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace iced_x86::tests {

/// Creates a dictionary from a generated `NameValue` table (eg. `OP_ACCESS_DICT`). Keys point to the (static) table
/// strings. If a key is duplicated, the last value is used (same as Rust's `HashMap::insert()`).
template <typename T, std::size_t N>
std::unordered_map<std::string_view, T> create_dict(const std::array<NameValue<T>, N>& values) {
	std::unordered_map<std::string_view, T> dict;
	dict.reserve(N);
	for (const auto& nv : values)
		dict.insert_or_assign(std::string_view(nv.name), nv.value);
	return dict;
}

/// Creates a set from a generated string table (eg. `IGNORED_CODE_NAMES`)
template <std::size_t N>
std::unordered_set<std::string_view> create_set(const std::array<const char*, N>& values) {
	std::unordered_set<std::string_view> set;
	set.reserve(N);
	for (const char* s : values)
		set.insert(std::string_view(s));
	return set;
}

/// Converts a hex string (whitespace is ignored) to bytes, eg. `"12 AB cd"`.
/// Throws "Missing hex digit in string: '...'" or "Invalid hex string: '...'"
std::vector<std::uint8_t> to_vec_u8(std::string_view hex_data);

/// Parses a `u64`: `0x` prefix = hex, else decimal. Throws "Invalid number: ..."
std::uint64_t to_u64(std::string_view value);
/// Parses an `i64`: optional `-`, then `0x` prefix = hex, else decimal (the `u64` value is negated, wrapping). Throws "Invalid number: ..."
std::int64_t to_i64(std::string_view value);
std::uint32_t to_u32(std::string_view value);
std::int32_t to_i32(std::string_view value);
std::uint16_t to_u16(std::string_view value);
std::int16_t to_i16(std::string_view value);
std::uint8_t to_u8(std::string_view value);
std::int8_t to_i8(std::string_view value);

/// Throws "Invalid Code value: ..."
Code to_code(std::string_view value);
/// Returns `true` if the `Code` value was removed by the generator and the test case should be ignored
bool is_ignored_code(std::string_view value);
/// Gets all `Code` names, sorted by `Code` value
std::vector<std::string_view> code_names();
/// Throws "Invalid Mnemonic value: ..."
Mnemonic to_mnemonic(std::string_view value);
/// Register names are lower case (eg. `"eax"`). An empty string returns `Register::None`. Throws "Invalid Register value: ..."
Register to_register(std::string_view value);
/// Gets a copy of the (lower case) register name -> `Register` dictionary
std::unordered_map<std::string, Register> clone_register_hashmap();
/// Throws "Invalid MemorySize value: ..."
MemorySize to_memory_size(std::string_view value);
/// Throws "Invalid DecoderError value: ..."
DecoderError to_decoder_error(std::string_view value);
/// Returns a `DecoderOptions` value. Throws "Invalid DecoderOptions value: ..."
std::uint32_t to_decoder_options(std::string_view value);
/// Throws "Invalid EncodingKind value: ..."
EncodingKind to_encoding_kind(std::string_view value);
/// Throws "Invalid TupleType value: ..."
TupleType to_tuple_type(std::string_view value);
/// Throws "Invalid MvexConvFn value: ..."
MvexConvFn to_mvex_conv_fn(std::string_view value);
/// Throws "Invalid MvexTupleTypeLutKind value: ..."
MvexTupleTypeLutKind to_mvex_tuple_type_lut_kind(std::string_view value);
/// Throws "Invalid CpuidFeature value: ..."
CpuidFeature to_cpuid_features(std::string_view value);
/// Throws "Invalid FlowControl value: ..."
FlowControl to_flow_control(std::string_view value);
/// Throws "Invalid OpCodeOperandKind value: ..."
OpCodeOperandKind to_op_code_operand_kind(std::string_view value);
/// Throws "Invalid ConditionCode value: ..."
ConditionCode to_condition_code(std::string_view value);
/// Throws "Invalid OptionsProps value: ..."
OptionsProps to_options_props(std::string_view value);
/// Throws "Invalid MemorySizeOptions value: ..."
MemorySizeOptions to_memory_size_options(std::string_view value);
/// Throws "Invalid NumberBase value: ..."
NumberBase to_number_base(std::string_view value);
/// Number of `NumberBase` values
std::size_t number_base_len();
/// `"true"` or `"false"`. Throws "Invalid boolean value: ..."
bool to_boolean(std::string_view value);
CC_b to_cc_b(std::string_view value);
CC_ae to_cc_ae(std::string_view value);
CC_e to_cc_e(std::string_view value);
CC_ne to_cc_ne(std::string_view value);
CC_be to_cc_be(std::string_view value);
CC_a to_cc_a(std::string_view value);
CC_p to_cc_p(std::string_view value);
CC_np to_cc_np(std::string_view value);
CC_l to_cc_l(std::string_view value);
CC_ge to_cc_ge(std::string_view value);
CC_le to_cc_le(std::string_view value);
CC_g to_cc_g(std::string_view value);

} // namespace iced_x86::tests
