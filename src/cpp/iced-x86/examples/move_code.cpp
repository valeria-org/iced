// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

// Decodes instructions from some address, then encodes them starting at some
// other address. This can be used to hook a function. You decode enough instructions
// until you have enough bytes to add a JMP instruction that jumps to your code.
// Your code will then conditionally jump to the original code that you re-encoded.
//
// This code uses the BlockEncoder which will help with some things, eg. converting
// short branches to longer branches if the target is too far away.
//
// 64-bit mode also supports RIP relative addressing, but the encoder can't rewrite
// those to use a longer displacement. If any of the moved instructions have RIP
// relative addressing and it tries to access data too far away, the encoder will fail.
// The easiest solution is to use OS alloc functions that allocate memory close to the
// original code (+/-2GB).

static constexpr std::uint32_t EXAMPLE_CODE_BITNESS = 64;
static constexpr std::uint64_t EXAMPLE_CODE_RIP = 0x0000'7FFA'C46A'CDA4;
static const std::uint8_t EXAMPLE_CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D,
	0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05,
	0x18, 0x57, 0x0A, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x85, 0xF0, 0x00, 0x00, 0x00, 0x4C, 0x8B,
	0x05, 0x2F, 0x24, 0x0A, 0x00, 0x48, 0x8D, 0x05, 0x78, 0x7C, 0x04, 0x00, 0x33, 0xFF,
};

static void disassemble(const std::vector<std::uint8_t>& data, std::uint64_t ip) {
	NasmFormatter formatter;
	std::string output;
	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, data, ip, DecoderOptions::NONE);
	for (const Instruction& instruction : decoder) {
		output.clear();
		formatter.format(instruction, output);
		std::printf("%016" PRIX64 " %s\n", instruction.ip(), output.c_str());
	}
	std::printf("\n");
}

/*
This function produces the following output:
Original code:
00007FFAC46ACDA4 mov [rsp+10h],rbx
00007FFAC46ACDA9 mov [rsp+18h],rsi
00007FFAC46ACDAE push rbp
00007FFAC46ACDAF push rdi
00007FFAC46ACDB0 push r14
00007FFAC46ACDB2 lea rbp,[rsp-100h]
00007FFAC46ACDBA sub rsp,200h
00007FFAC46ACDC1 mov rax,[rel 7FFAC47524E0h]
00007FFAC46ACDC8 xor rax,rsp
00007FFAC46ACDCB mov [rbp+0F0h],rax
00007FFAC46ACDD2 mov r8,[rel 7FFAC474F208h]
00007FFAC46ACDD9 lea rax,[rel 7FFAC46F4A58h]
00007FFAC46ACDE0 xor edi,edi

Original + patched code:
00007FFAC46ACDA4 mov rax,123456789ABCDEF0h
00007FFAC46ACDAE jmp rax
00007FFAC46ACDB0 push r14
00007FFAC46ACDB2 lea rbp,[rsp-100h]
00007FFAC46ACDBA sub rsp,200h
00007FFAC46ACDC1 mov rax,[rel 7FFAC47524E0h]
00007FFAC46ACDC8 xor rax,rsp
00007FFAC46ACDCB mov [rbp+0F0h],rax
00007FFAC46ACDD2 mov r8,[rel 7FFAC474F208h]
00007FFAC46ACDD9 lea rax,[rel 7FFAC46F4A58h]
00007FFAC46ACDE0 xor edi,edi

Moved code:
00007FFAC48ACDA4 mov [rsp+10h],rbx
00007FFAC48ACDA9 mov [rsp+18h],rsi
00007FFAC48ACDAE push rbp
00007FFAC48ACDAF push rdi
00007FFAC48ACDB0 jmp 00007FFAC46ACDB0h
*/
static Result<void> how_to_move_code() {
	std::vector<std::uint8_t> example_code(EXAMPLE_CODE, EXAMPLE_CODE + sizeof(EXAMPLE_CODE));
	std::printf("Original code:\n");
	disassemble(example_code, EXAMPLE_CODE_RIP);

	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, example_code, EXAMPLE_CODE_RIP, DecoderOptions::NONE);

	// In 64-bit mode, we need 12 bytes to jump to any address:
	//      mov rax,imm64   // 10
	//      jmp rax         // 2
	// We overwrite rax because it's probably not used by the called function.
	// In 32-bit mode, a normal JMP is just 5 bytes
	const std::uint32_t required_bytes = 10 + 2;
	std::uint32_t total_bytes = 0;
	std::vector<Instruction> orig_instructions;
	for (const Instruction& instr : decoder) {
		orig_instructions.push_back(instr);
		total_bytes += static_cast<std::uint32_t>(instr.len());
		if (instr.is_invalid())
			return IcedError("Found garbage");
		if (total_bytes >= required_bytes)
			break;

		switch (instr.flow_control()) {
		case FlowControl::Next:
			break;

		case FlowControl::UnconditionalBranch:
			if (instr.op0_kind() == OpKind::NearBranch64) {
				[[maybe_unused]] const std::uint64_t target = instr.near_branch_target();
				// You could check if it's just jumping forward a few bytes and follow it
				// but this is a simple example so we'll fail.
			}
			return IcedError("Not supported by this simple example");

		case FlowControl::IndirectBranch:
		case FlowControl::ConditionalBranch:
		case FlowControl::Return:
		case FlowControl::Call:
		case FlowControl::IndirectCall:
		case FlowControl::Interrupt:
		case FlowControl::XbeginXabortXend:
		case FlowControl::Exception:
		default:
			return IcedError("Not supported by this simple example");
		}
	}
	if (total_bytes < required_bytes)
		return IcedError("Not enough bytes!");
	if (orig_instructions.empty())
		return IcedError("No instructions");
	// Create a JMP instruction that branches to the original code, except those instructions
	// that we'll re-encode. We don't need to do it if it already ends in 'ret'
	const Instruction& last_instr = orig_instructions.back();
	const std::uint64_t jmp_back_addr = last_instr.next_ip();
	if (last_instr.flow_control() != FlowControl::Return) {
		auto jmp = Instruction::with_branch(Code::Jmp_rel32_64, jmp_back_addr);
		if (!jmp)
			return jmp.error();
		orig_instructions.push_back(jmp.value());
	}

	// Relocate the code to some new location. It can fix short/near branches and
	// convert them to short/near/long forms if needed. This also works even if it's a
	// jrcxz/loop/loopcc instruction which only have short forms.
	//
	// It can currently only fix RIP relative operands if the new location is within 2GB
	// of the target data location.
	//
	// Note that a block is not the same thing as a basic block. A block can contain any
	// number of instructions, including any number of branch instructions. One block
	// should be enough unless you must relocate different blocks to different locations.
	const std::uint64_t relocated_base_address = EXAMPLE_CODE_RIP + 0x20'0000;
	InstructionBlock block(orig_instructions, relocated_base_address);
	// This method can also encode more than one block but that's rarely needed, see above comment.
	auto result = BlockEncoder::encode(decoder.bitness(), block, BlockEncoderOptions::NONE);
	if (!result)
		return result.error();
	const std::vector<std::uint8_t>& new_code = result.value().code_buffer;

	// Patch the original code. Pretend that we use some OS API to write to memory...
	// We could use the BlockEncoder/Encoder for this but it's easy to do yourself too.
	// This is 'mov rax,imm64; jmp rax'
	constexpr std::uint64_t YOUR_FUNC = 0x1234'5678'9ABC'DEF0; // Address of your code
	example_code[0] = 0x48;                                     // \ 'MOV RAX,imm64'
	example_code[1] = 0xB8;                                     // /
	std::uint64_t v = YOUR_FUNC;
	for (std::size_t i = 2; i < 10; i++) {
		example_code[i] = static_cast<std::uint8_t>(v);
		v >>= 8;
	}
	example_code[10] = 0xFF; // \ JMP RAX
	example_code[11] = 0xE0; // /

	// Disassemble it
	std::printf("Original + patched code:\n");
	disassemble(example_code, EXAMPLE_CODE_RIP);

	// Disassemble the moved code
	std::printf("Moved code:\n");
	disassemble(new_code, relocated_base_address);

	return {};
}

int main() {
	auto result = how_to_move_code();
	if (!result) {
		std::fprintf(stderr, "Error: %s\n", result.error().message());
		return 1;
	}
	return 0;
}
