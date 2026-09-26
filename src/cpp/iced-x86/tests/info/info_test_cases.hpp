// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

#include "iced_x86/code.hpp"
#include "iced_x86/cpuid_feature.hpp"
#include "iced_x86/encoding_kind.hpp"
#include "iced_x86/flow_control.hpp"
#include "iced_x86/instruction_info.hpp"
#include "iced_x86/memory_size.hpp"
#include "iced_x86/op_access.hpp"
#include "iced_x86/register.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace iced_x86::tests::instr_info {

struct InstrInfoTestCase {
	std::uint32_t line_number = 0;
	std::uint32_t bitness = 0;
	std::string hex_bytes;
	std::uint64_t ip = 0;
	Code code = Code::INVALID;
	std::uint32_t decoder_options = 0;
	EncodingKind encoding = EncodingKind::Legacy;
	std::vector<CpuidFeature> cpuid_features;
	std::uint32_t rflags_read = 0;
	std::uint32_t rflags_undefined = 0;
	std::uint32_t rflags_written = 0;
	std::uint32_t rflags_cleared = 0;
	std::uint32_t rflags_set = 0;
	std::int32_t stack_pointer_increment = 0;
	bool is_privileged = false;
	bool is_stack_instruction = false;
	bool is_save_restore_instruction = false;
	bool is_special = false;
	std::vector<UsedRegister> used_registers;
	std::vector<UsedMemory> used_memory;
	FlowControl flow_control = FlowControl::Next;
	OpAccess op0_access = OpAccess::None;
	OpAccess op1_access = OpAccess::None;
	OpAccess op2_access = OpAccess::None;
	OpAccess op3_access = OpAccess::None;
	OpAccess op4_access = OpAccess::None;
	std::int32_t fpu_top_increment = 0;
	bool fpu_conditional_top = false;
	bool fpu_writes_top = false;
};

// instr_info_test_parser.cpp
std::vector<InstrInfoTestCase> read_instr_info_test_cases(std::uint32_t bitness);
// Cached test cases (bitness = 16, 32 or 64)
const std::vector<InstrInfoTestCase>& get_instr_info_test_cases(std::uint32_t bitness);

// Same as Rust's `str::splitn(max_parts, sep)`: returns at most `max_parts` parts (the last one contains the rest of the string)
std::vector<std::string_view> splitn(std::string_view s, std::size_t max_parts, char sep);

} // namespace iced_x86::tests::instr_info
