// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Virtual address test cases (port of Rust's `test/va_test_case.rs`, `test/va_test_parser.rs`, `test/va_test_cases.rs`)

#pragma once

#include "iced_x86/register.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace iced_x86::tests {

struct VARegisterValue {
	Register register_;
	std::size_t element_index;
	std::size_t element_size;
	std::uint64_t value;
};

struct VirtualAddressTestCase {
	std::uint32_t bitness;
	std::string hex_bytes;
	std::uint32_t decoder_options;
	std::int32_t operand;
	std::int32_t used_mem_index;
	std::size_t element_index;
	std::uint64_t expected_value;
	std::vector<VARegisterValue> register_values;
};

/// Reads all test cases from a file. Throws `std::runtime_error` if the file couldn't be read or if it's invalid.
std::vector<VirtualAddressTestCase> read_va_test_cases(const std::string& filename);

/// Gets all test cases in `get_instruction_unit_tests_dir() + "/VirtualAddressTests.txt"` (read once)
const std::vector<VirtualAddressTestCase>& get_va_test_cases();

} // namespace iced_x86::tests
