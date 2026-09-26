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

/*
This function produces the following output:
00001248FC840000 push    %rbp
00001248FC840001 push    %rdi
00001248FC840002 push    %rsi
00001248FC840003 sub     $0x50,%rsp
00001248FC84000A vzeroupper
00001248FC84000D lea     0x60(%rsp),%rbp
00001248FC840012 mov     %rcx,%rsi
00001248FC840015 lea     -0x38(%rbp),%rdi
00001248FC840019 mov     $0xA,%ecx
00001248FC84001E xor     %eax,%eax
00001248FC840020 rep stos %eax,(%rdi)
00001248FC840022 cmp     $0x12345678,%rsi
00001248FC840029 jne     0x00001248FC84002C
00001248FC84002B nop
00001248FC84002C xor     %r15d,%r15d
00001248FC84002F lea     0x1248FC840037,%r14
00001248FC840036 nop
00001248FC840037 .byte   0x12,0x34,0x56,0x78
*/
static Result<void> how_to_encode_instructions() {
	const std::uint32_t bitness = 64;

	// All created instructions get an IP of 0. The label id is just an IP.
	// The branch instruction's *target* IP should be equal to the IP of the
	// target instruction.
	std::uint64_t label_id = 1;
	auto create_label = [&label_id]() { return label_id++; };
	auto add_label = [](std::uint64_t id, Result<Instruction> instruction) {
		if (instruction)
			instruction.value().set_ip(id);
		return instruction;
	};

	const std::uint64_t label1 = create_label();

	// Most Instruction::with*() methods return a Result<Instruction> (they fail if the operands are invalid)
	std::vector<Result<Instruction>> results = {
		Instruction::with1(Code::Push_r64, Register::RBP),
		Instruction::with1(Code::Push_r64, Register::RDI),
		Instruction::with1(Code::Push_r64, Register::RSI),
		Instruction::with2(Code::Sub_rm64_imm32, Register::RSP, 0x50),
		Instruction::with(Code::VEX_Vzeroupper),
		Instruction::with2(Code::Lea_r64_m, Register::RBP, MemoryOperand::with_base_displ(Register::RSP, 0x60)),
		Instruction::with2(Code::Mov_r64_rm64, Register::RSI, Register::RCX),
		Instruction::with2(Code::Lea_r64_m, Register::RDI, MemoryOperand::with_base_displ(Register::RBP, -0x38)),
		Instruction::with2(Code::Mov_r32_imm32, Register::ECX, 0x0A),
		Instruction::with2(Code::Xor_r32_rm32, Register::EAX, Register::EAX),
		Instruction::with_rep_stosd(bitness),
		Instruction::with2(Code::Cmp_rm64_imm32, Register::RSI, 0x1234'5678),
		// Create a branch instruction that references label1
		Instruction::with_branch(Code::Jne_rel32_64, label1),
		Instruction::with(Code::Nopd),
		// Add the instruction that is the target of the branch
		add_label(label1, Instruction::with2(Code::Xor_r32_rm32, Register::R15D, Register::R15D)),
	};

	// Create an instruction that accesses some data using an RIP relative memory operand
	const std::uint64_t data1 = create_label();
	results.push_back(Instruction::with2(Code::Lea_r64_m, Register::R14, MemoryOperand::with_base_displ(Register::RIP, static_cast<std::int64_t>(data1))));
	results.push_back(Instruction::with(Code::Nopd));
	static const std::uint8_t raw_data[] = {0x12, 0x34, 0x56, 0x78};
	results.push_back(add_label(data1, Instruction::with_declare_byte(raw_data, sizeof(raw_data))));

	std::vector<Instruction> instructions;
	for (const Result<Instruction>& result : results) {
		if (!result)
			return result.error();
		instructions.push_back(result.value());
	}

	// Use BlockEncoder to encode a block of instructions. This block can contain any
	// number of branches and any number of instructions. It does support encoding more
	// than one block but it's rarely needed.
	// It uses Encoder to encode all instructions.
	// If the target of a branch is too far away, it can fix it to use a longer branch.
	// This can be disabled by enabling some BlockEncoderOptions flags.
	const std::uint64_t target_rip = 0x0000'1248'FC84'0000;
	InstructionBlock block(instructions, target_rip);
	auto result = BlockEncoder::encode(bitness, block, BlockEncoderOptions::NONE);
	if (!result) {
		std::fprintf(stderr, "Failed to encode it: %s\n", result.error().message());
		return result.error();
	}

	// Now disassemble the encoded instructions. Note that the 'jmp near'
	// instruction was turned into a 'jmp short' instruction because we
	// didn't disable branch optimizations.
	const std::vector<std::uint8_t>& bytes = result.value().code_buffer;
	std::string output;
	const std::uint8_t* bytes_code = bytes.data();
	const std::size_t bytes_code_len = bytes.size() - sizeof(raw_data);
	const std::uint8_t* bytes_data = bytes.data() + bytes_code_len;
	auto decoder = Decoder::with_ip(bitness, bytes_code, bytes_code_len, target_rip, DecoderOptions::NONE);
	GasFormatter formatter;
	formatter.options_mut().set_first_operand_char_index(8);
	for (const Instruction& instruction : decoder) {
		output.clear();
		formatter.format(instruction, output);
		std::printf("%016" PRIX64 " %s\n", instruction.ip(), output.c_str());
	}
	auto db = Instruction::with_declare_byte(bytes_data, sizeof(raw_data));
	if (!db)
		return db.error();
	output.clear();
	formatter.format(db.value(), output);
	std::printf("%016" PRIX64 " %s\n", decoder.ip(), output.c_str());
	return {};
}

int main() {
	auto result = how_to_encode_instructions();
	if (!result) {
		std::fprintf(stderr, "Error: %s\n", result.error().message());
		return 1;
	}
	return 0;
}
