// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's encoder/tests/non_decoded_tests.rs: instructions that can't be decoded but can be encoded
// (eg. `fstenv` with an FWAIT prefix, `db`/`dw`/`dd`/`dq`). Used by the encoder and formatter tests.

#pragma once

#include "iced_x86/instruction.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace iced_x86::tests {

struct NonDecodedInfo {
	// Encoded bytes (hex string, eg. "9B D9 30")
	const char* hex_bytes;
	Instruction instruction;
};

struct NonDecodedTest {
	std::uint32_t bitness;
	const char* hex_bytes;
	Instruction instruction;
};

// Rust: non_decoded_tests::get_infos(bitness)
const std::vector<NonDecodedInfo>& get_non_decoded_infos(std::uint32_t bitness);

// Rust: non_decoded_tests::get_tests()
std::vector<NonDecodedTest> get_non_decoded_tests();

} // namespace iced_x86::tests
