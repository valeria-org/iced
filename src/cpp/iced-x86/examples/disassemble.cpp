// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

static constexpr std::size_t HEXBYTES_COLUMN_BYTE_LENGTH = 10;
static constexpr std::uint32_t EXAMPLE_CODE_BITNESS = 64;
static constexpr std::uint64_t EXAMPLE_CODE_RIP = 0x0000'7FFA'C46A'CDA4;
static const std::uint8_t EXAMPLE_CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D,
	0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05,
	0x18, 0x57, 0x0A, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x85, 0xF0, 0x00, 0x00, 0x00, 0x4C, 0x8B,
	0x05, 0x2F, 0x24, 0x0A, 0x00, 0x48, 0x8D, 0x05, 0x78, 0x7C, 0x04, 0x00, 0x33, 0xFF,
};

/*
This function produces the following output:
00007FFAC46ACDA4 48895C2410           mov       [rsp+10h],rbx
00007FFAC46ACDA9 4889742418           mov       [rsp+18h],rsi
00007FFAC46ACDAE 55                   push      rbp
00007FFAC46ACDAF 57                   push      rdi
00007FFAC46ACDB0 4156                 push      r14
00007FFAC46ACDB2 488DAC2400FFFFFF     lea       rbp,[rsp-100h]
00007FFAC46ACDBA 4881EC00020000       sub       rsp,200h
00007FFAC46ACDC1 488B0518570A00       mov       rax,[rel 7FFA`C475`24E0h]
00007FFAC46ACDC8 4833C4               xor       rax,rsp
00007FFAC46ACDCB 488985F0000000       mov       [rbp+0F0h],rax
00007FFAC46ACDD2 4C8B052F240A00       mov       r8,[rel 7FFA`C474`F208h]
00007FFAC46ACDD9 488D05787C0400       lea       rax,[rel 7FFA`C46F`4A58h]
00007FFAC46ACDE0 33FF                 xor       edi,edi
*/
static void how_to_disassemble() {
	const auto& bytes = EXAMPLE_CODE;
	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, bytes, EXAMPLE_CODE_RIP, DecoderOptions::NONE);

	// Formatters: Masm*, Nasm*, Gas* (AT&T) and Intel* (XED).
	// For fastest code, see `SpecializedFormatter` which is ~3.3x faster. Use it if formatting
	// speed is more important than being able to re-assemble formatted instructions.
	NasmFormatter formatter;

	// Change some options, there are many more
	formatter.options_mut().set_digit_separator("`");
	formatter.options_mut().set_first_operand_char_index(10);

	// format() appends to a std::string (or you can pass in your own FormatterOutput)
	std::string output;

	// Initialize this outside the loop because decode_out() writes to every field
	Instruction instruction;

	// The decoder also has begin()/end() so you could use a range-for loop:
	//      for (const Instruction& instruction : decoder) { /* ... */ }
	// but can_decode()/decode_out() is a little faster:
	while (decoder.can_decode()) {
		// There's also a decode() method that returns an instruction but that also
		// means it copies an instruction (40 bytes):
		//     instruction = decoder.decode();
		decoder.decode_out(instruction);

		// Format the instruction ("disassemble" it)
		output.clear();
		formatter.format(instruction, output);

		// Eg. "00007FFAC46ACDB2 488DAC2400FFFFFF     lea       rbp,[rsp-100h]"
		std::printf("%016" PRIX64 " ", instruction.ip());
		const std::size_t start_index = static_cast<std::size_t>(instruction.ip() - EXAMPLE_CODE_RIP);
		const std::size_t instr_len = instruction.len();
		for (std::size_t i = 0; i < instr_len; i++)
			std::printf("%02X", bytes[start_index + i]);
		for (std::size_t i = instr_len; i < HEXBYTES_COLUMN_BYTE_LENGTH; i++)
			std::printf("  ");
		std::printf(" %s\n", output.c_str());
	}
}

int main() {
	how_to_disassemble();
	return 0;
}
