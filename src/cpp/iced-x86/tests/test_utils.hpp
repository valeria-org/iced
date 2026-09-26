// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

#include <string>

namespace iced_x86::tests {

/// Gets the path of `src/UnitTests/Intel` (shared test data used by all languages)
inline std::string get_unit_tests_base_dir() { return ICED_X86_UNIT_TESTS_DIR; }
inline std::string get_decoder_unit_tests_dir() { return get_unit_tests_base_dir() + "/Decoder"; }
inline std::string get_encoder_unit_tests_dir() { return get_unit_tests_base_dir() + "/Encoder"; }
inline std::string get_instr_info_unit_tests_dir() { return get_unit_tests_base_dir() + "/InstructionInfo"; }
inline std::string get_formatter_unit_tests_dir() { return get_unit_tests_base_dir() + "/Formatter"; }
inline std::string get_instruction_unit_tests_dir() { return get_unit_tests_base_dir() + "/Instruction"; }

} // namespace iced_x86::tests
