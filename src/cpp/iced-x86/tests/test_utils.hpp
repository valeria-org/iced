// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

#include "generated/decoder_constants.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace iced_x86::tests {

/// Gets the path of `src/UnitTests/Intel` (shared test data used by all languages)
inline std::string get_unit_tests_base_dir() { return ICED_X86_UNIT_TESTS_DIR; }
inline std::string get_decoder_unit_tests_dir() { return get_unit_tests_base_dir() + "/Decoder"; }
inline std::string get_encoder_unit_tests_dir() { return get_unit_tests_base_dir() + "/Encoder"; }
inline std::string get_instr_info_unit_tests_dir() { return get_unit_tests_base_dir() + "/InstructionInfo"; }
inline std::string get_formatter_unit_tests_dir() { return get_unit_tests_base_dir() + "/Formatter"; }
inline std::string get_instruction_unit_tests_dir() { return get_unit_tests_base_dir() + "/Instruction"; }

/// Gets the default IP used by the decoder tests (16, 32 or 64-bit code)
inline std::uint64_t get_default_ip(std::uint32_t bitness) {
	switch (bitness) {
	case 16:
		return DecoderConstants::DEFAULT_IP16;
	case 32:
		return DecoderConstants::DEFAULT_IP32;
	case 64:
		return DecoderConstants::DEFAULT_IP64;
	default:
		throw std::runtime_error("Invalid bitness: " + std::to_string(bitness));
	}
}

} // namespace iced_x86::tests
