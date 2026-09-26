// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Helpers used by the code assembler tests (hand written tests and the generated tests in tests/code_asm/generated/)

#pragma once

#include "iced_x86/code.hpp"
#include "iced_x86/code_asm.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/iced_error.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/memory_operand.hpp"
#include "iced_x86/register.hpp"
#include "iced_x86/rep_prefix_kind.hpp"
#include "generated/test_instr_flags.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <utility>

namespace iced_x86::tests::code_asm_tests {

using iced_x86::code_asm::CodeAssembler;
using iced_x86::code_asm::CodeLabel;

/// Id of the first label created by a `CodeAssembler`
inline constexpr std::uint64_t FIRST_LABEL_ID = 1;

/// Returns the value or fails the current test case if it's an error (Rust's `unwrap()`)
template <typename T>
T unwrap(Result<T> result) {
	REQUIRE_MSG(result.is_ok(), result.error().message());
	return std::move(result).value();
}

/// Creates a `CodeAssembler` and applies the `TestInstrFlags` options
CodeAssembler create_asm(std::uint32_t bitness, std::uint32_t flags);

/// Calls `create()` which must add exactly one instruction (`expected`), then assembles and decodes it and checks
/// that the decoded instruction is the same as the added instruction.
void test_instr(std::uint32_t bitness, void (*create)(CodeAssembler& a), Instruction expected, std::uint32_t flags, std::uint32_t decoder_options);

/// Same as `test_instr(bitness, create, expected, flags, decoder_options)` but the expected instruction is created by
/// `create_expected()` (used by the generated tests: it's much faster to compile)
void test_instr(std::uint32_t bitness, void (*create)(CodeAssembler& a), Result<Instruction> (*create_expected)(), std::uint32_t flags,
	std::uint32_t decoder_options);

/// Calls `create()` which must fail (sticky error) and not add an instruction
void test_invalid_instr(std::uint32_t bitness, void (*create)(CodeAssembler& a), std::uint32_t flags);

/// Sets the opmask register
inline Instruction add_op_mask(Instruction instruction, Register op_mask) {
	instruction.set_op_mask(op_mask);
	return instruction;
}

/// Sets the opmask register if it's not an error
Result<Instruction> add_op_mask(Result<Instruction> instruction, Register op_mask);

/// Creates a label and emits it (it's the label of the next instruction)
CodeLabel create_and_emit_label(CodeAssembler& a);

/// Sets the instruction's IP to the label id (the code assembler stores the label id in the IP field)
inline Instruction assign_label(Instruction instruction, std::uint64_t label) {
	instruction.set_ip(label);
	return instruction;
}

/// Sets the instruction's IP to the label id if it's not an error
Result<Instruction> assign_label(Result<Instruction> instruction, std::uint64_t label);

} // namespace iced_x86::tests::code_asm_tests
