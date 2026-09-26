// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Helpers used by the encoder tests

#pragma once

#include "iced_x86/encoder.hpp"
#include "iced_x86/instruction.hpp"
#include "test_framework.hpp"
#include <cstdint>
#include <string>
#include <vector>

#if __has_include("iced_x86/decoder.hpp")
#define ICED_X86_TESTS_HAS_DECODER 1
#include "iced_x86/decoder.hpp"
#include "iced_x86/decoder_options.hpp"
#include "test_utils/decoder_test_utils.hpp"
#else
#define ICED_X86_TESTS_HAS_DECODER 0
#endif

#if __has_include("test_utils/abort_utils.hpp")
#include "test_utils/abort_utils.hpp"
#else
#define ICED_X86_TESTS_CAN_CHECK_ABORT 0
#endif

namespace iced_x86::tests {

// Returns eg. "12 AB 34"
inline std::string slice_u8_to_string(const std::vector<std::uint8_t>& bytes) {
	static constexpr char HEX[] = "0123456789ABCDEF";
	std::string s;
	for (const std::uint8_t b : bytes) {
		if (!s.empty())
			s.push_back(' ');
		s.push_back(HEX[b >> 4]);
		s.push_back(HEX[b & 0xF]);
	}
	return s;
}

} // namespace iced_x86::tests

// Rust: encode_ok!(bitness, instr)
#define ENCODE_OK(bitness, instr_result) \
	do { \
		::iced_x86::Encoder encode_ok_encoder_(bitness); \
		const ::iced_x86::Instruction encode_ok_instr_ = (instr_result).value(); \
		CHECK(encode_ok_encoder_.encode(encode_ok_instr_, 0x1234).is_ok()); \
	} while (0)

// Rust: encode_err!(bitness, instr)
#define ENCODE_ERR(bitness, instr_result) \
	do { \
		::iced_x86::Encoder encode_err_encoder_(bitness); \
		const ::iced_x86::Instruction encode_err_instr_ = (instr_result).value(); \
		CHECK(encode_err_encoder_.encode(encode_err_instr_, 0x1234).is_err()); \
	} while (0)
