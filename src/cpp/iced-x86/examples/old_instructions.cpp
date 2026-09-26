// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include <cstdint>
#include <cstdio>
#include <string>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

/*
This function produces the following output:
731E0A03 bndmov bnd1, [eax]
731E0A07 mov tr3, esi
731E0A0A rdshr [eax]
731E0A0D dmint
731E0A0F svdc [eax], cs
731E0A12 cpu_read
731E0A14 pmvzb mm1, [eax]
731E0A17 frinear
731E0A19 altinst
*/
static void how_to_disassemble_old_instrs() {
	// clang-format off
	static const std::uint8_t bytes[] = {
		// bndmov bnd1,[eax]
		0x66, 0x0F, 0x1A, 0x08,
		// mov tr3,esi
		0x0F, 0x26, 0xDE,
		// rdshr [eax]
		0x0F, 0x36, 0x00,
		// dmint
		0x0F, 0x39,
		// svdc [eax],cs
		0x0F, 0x78, 0x08,
		// cpu_read
		0x0F, 0x3D,
		// pmvzb mm1,[eax]
		0x0F, 0x58, 0x08,
		// frinear
		0xDF, 0xFC,
		// altinst
		0x0F, 0x3F,
	};
	// clang-format on

	// Enable decoding of Cyrix/Geode instructions, Centaur ALTINST, MOV to/from TR
	// and MPX instructions.
	// There are other options to enable other instructions such as UMOV, KNC, etc.
	// These are deprecated instructions or only used by old CPUs so they're not
	// enabled by default. Some newer instructions also use the same opcodes as
	// some of these old instructions.
	constexpr std::uint32_t DECODER_OPTIONS =
		DecoderOptions::MPX | DecoderOptions::MOV_TR | DecoderOptions::CYRIX | DecoderOptions::CYRIX_DMI | DecoderOptions::ALTINST;
	auto decoder = Decoder::with_ip(32, bytes, 0x731E'0A03, DECODER_OPTIONS);

	NasmFormatter formatter;
	formatter.options_mut().set_space_after_operand_separator(true);
	std::string output;

	Instruction instruction;
	while (decoder.can_decode()) {
		decoder.decode_out(instruction);

		output.clear();
		formatter.format(instruction, output);

		std::printf("%08X %s\n", instruction.ip32(), output.c_str());
	}
}

int main() {
	how_to_disassemble_old_instrs();
	return 0;
}
