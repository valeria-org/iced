// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "iced_x86/code_asm.hpp"

using namespace iced_x86;
using namespace iced_x86::code_asm;

// Like Rust's assert!(): also checked in release builds (unlike assert())
static void check(bool condition, const char* message) {
	if (!condition) {
		std::fprintf(stderr, "Check failed: %s\n", message);
		std::abort();
	}
}

static Result<void> how_to_use_code_assembler() {
	// You can also call CodeAssembler::create(64) which returns a Result<CodeAssembler>
	// instead of aborting if the bitness is invalid
	CodeAssembler a(64);

	// Anytime you add something to a register (or subtract from it), you create a
	// memory operand. You can also call word_ptr(), dword_bcst() etc to create memory
	// operands.
	static_cast<void>(rax);                 // register
	static_cast<void>(rax + 0);             // memory with no size hint
	static_cast<void>(ptr(rax));            // memory with no size hint
	static_cast<void>(rax + rcx * 4 - 123); // memory with no size hint
	// To create a memory operand with only a displacement or only a base register,
	// you can call one of the memory fns:
	static_cast<void>(qword_ptr(123));  // memory with a qword size hint
	static_cast<void>(dword_bcst(rcx)); // memory (broadcast) with a dword size hint
	// To add a segment override, call the segment methods:
	static_cast<void>(ptr(rax).fs()); // fs:[rax]

	// Each mnemonic is a method. Mnemonics that are C++ keywords get a `_` suffix, eg. xor_(), and_(), int_()
	a.push(rcx);
	// There are a few exceptions where you must append `_<opcount>` to the mnemonic to
	// get the instruction you need:
	a.ret();
	a.ret_1(123);
	// Use byte_ptr(), word_bcst(), etc to force the arg to a memory operand and to add a
	// size hint
	a.xor_(byte_ptr(rdx + r14 * 4 + 123), 0x10);
	// Prefixes are also methods
	a.rep().stosd();
	// Sometimes, you must help the compiler pick the right overload. 64-bit immediates must be
	// std::int64_t/std::uint64_t (a `ULL` suffix is ambiguous if std::uint64_t is `unsigned long`):
	a.mov(rax, std::uint64_t{0x1234'5678'9ABC'DEF0});

	// Errors are sticky: instead of checking the result of each call, the first error is
	// saved, the following calls are ignored and assemble() returns the error. You can
	// also check it with has_error() and error().

	// Create labels that can be referenced by code
	CodeLabel loop_lbl1 = a.create_label();
	CodeLabel after_loop1 = a.create_label();
	a.mov(ecx, 10);
	a.set_label(loop_lbl1);
	// If needed, a zero-bytes instruction can be used as a label but this is optional
	a.zero_bytes();
	a.dec(ecx);
	a.jp(after_loop1);
	a.jne(loop_lbl1);
	a.set_label(after_loop1);

	// It's possible to reference labels with RIP-relative addressing
	CodeLabel skip_data = a.create_label();
	CodeLabel data = a.create_label();
	a.jmp(skip_data);
	a.set_label(data);
	a.db({0x90, 0xCC, 0xF1, 0x90});
	a.set_label(skip_data);
	a.lea(rax, ptr(data));

	// AVX512 opmasks, {z}, {sae}, {er} and broadcasting are also supported:
	a.vsqrtps(zmm16.k2().z(), dword_bcst(rcx));
	a.vsqrtps(zmm1.k2().z(), zmm23.rd_sae());
	// Sometimes, the encoder doesn't know if you want VEX or EVEX encoding.
	// You can force EVEX globally like so:
	a.set_prefer_vex(false);
	a.vucomiss(xmm31, xmm15.sae());
	a.vucomiss(xmm31, ptr(rcx));
	// or call vex()/evex() to override the encoding option:
	a.evex().vucomiss(xmm31, xmm15.sae());
	a.vex().vucomiss(xmm15, xmm14);

	// Encode all added instructions.
	// Use `assemble_options()` if you must get the address of a label
	auto bytes = a.assemble(0x1234'5678);
	if (!bytes)
		return bytes.error();
	check(bytes.value().size() == 82, "bytes.size() == 82");
	// If you don't want to encode them, you can get all instructions by calling
	// one of these methods:
	const std::vector<Instruction>& instrs = a.instructions(); // Get a reference to the internal vector
	check(instrs.size() == 20, "instrs.size() == 20");
	std::vector<Instruction> instrs2 = a.take_instructions(); // Take ownership of the vector with all instructions
	check(instrs2.size() == 20, "instrs2.size() == 20");
	check(a.instructions().empty(), "a.instructions().empty()");

	return {};
}

int main() {
	auto result = how_to_use_code_assembler();
	if (!result) {
		std::fprintf(stderr, "Error: %s\n", result.error().message());
		return 1;
	}
	return 0;
}
