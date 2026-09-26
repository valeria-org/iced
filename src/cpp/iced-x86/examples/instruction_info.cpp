// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

static constexpr std::uint32_t EXAMPLE_CODE_BITNESS = 64;
static constexpr std::uint64_t EXAMPLE_CODE_RIP = 0x0000'7FFA'C46A'CDA4;
static const std::uint8_t EXAMPLE_CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D,
	0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05,
	0x18, 0x57, 0x0A, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x85, 0xF0, 0x00, 0x00, 0x00, 0x4C, 0x8B,
	0x05, 0x2F, 0x24, 0x0A, 0x00, 0x48, 0x8D, 0x05, 0x78, 0x7C, 0x04, 0x00, 0x33, 0xFF,
};

static std::string flags(std::uint32_t rf) {
	std::string sb;
	auto append = [&sb](const char* s) {
		if (!sb.empty())
			sb += ", ";
		sb += s;
	};

	if ((rf & RflagsBits::OF) != 0)
		append("OF");
	if ((rf & RflagsBits::SF) != 0)
		append("SF");
	if ((rf & RflagsBits::ZF) != 0)
		append("ZF");
	if ((rf & RflagsBits::AF) != 0)
		append("AF");
	if ((rf & RflagsBits::CF) != 0)
		append("CF");
	if ((rf & RflagsBits::PF) != 0)
		append("PF");
	if ((rf & RflagsBits::DF) != 0)
		append("DF");
	if ((rf & RflagsBits::IF) != 0)
		append("IF");
	if ((rf & RflagsBits::AC) != 0)
		append("AC");
	if ((rf & RflagsBits::UIF) != 0)
		append("UIF");
	if (sb.empty())
		sb = "<empty>";
	return sb;
}

/*
This function produces the following output:
00007FFAC46ACDA4 mov [rsp+10h],rbx
    OpCode: o64 89 /r
    Instruction: MOV r/m64, r64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_rm64_r64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 4, size = 1
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_or_mem
    Op1: r64_reg
    Used reg: RSP:Read
    Used reg: RBX:Read
    Used mem: [SS:RSP+0x10;UInt64;Write]
00007FFAC46ACDA9 mov [rsp+18h],rsi
    OpCode: o64 89 /r
    Instruction: MOV r/m64, r64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_rm64_r64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 4, size = 1
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_or_mem
    Op1: r64_reg
    Used reg: RSP:Read
    Used reg: RSI:Read
    Used mem: [SS:RSP+0x18;UInt64;Write]
00007FFAC46ACDAE push rbp
    OpCode: o64 50+ro
    Instruction: PUSH r64
    Encoding: Legacy
    Mnemonic: Push
    Code: Push_r64
    CpuidFeature: X64
    FlowControl: Next
    SP Increment: -8
    Op0Access: Read
    Op0: r64_opcode
    Used reg: RBP:Read
    Used reg: RSP:ReadWrite
    Used mem: [SS:RSP+0xFFFFFFFFFFFFFFF8;UInt64;Write]
00007FFAC46ACDAF push rdi
    OpCode: o64 50+ro
    Instruction: PUSH r64
    Encoding: Legacy
    Mnemonic: Push
    Code: Push_r64
    CpuidFeature: X64
    FlowControl: Next
    SP Increment: -8
    Op0Access: Read
    Op0: r64_opcode
    Used reg: RDI:Read
    Used reg: RSP:ReadWrite
    Used mem: [SS:RSP+0xFFFFFFFFFFFFFFF8;UInt64;Write]
00007FFAC46ACDB0 push r14
    OpCode: o64 50+ro
    Instruction: PUSH r64
    Encoding: Legacy
    Mnemonic: Push
    Code: Push_r64
    CpuidFeature: X64
    FlowControl: Next
    SP Increment: -8
    Op0Access: Read
    Op0: r64_opcode
    Used reg: R14:Read
    Used reg: RSP:ReadWrite
    Used mem: [SS:RSP+0xFFFFFFFFFFFFFFF8;UInt64;Write]
00007FFAC46ACDB2 lea rbp,[rsp-100h]
    OpCode: o64 8D /r
    Instruction: LEA r64, m
    Encoding: Legacy
    Mnemonic: Lea
    Code: Lea_r64_m
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 4, size = 4
    Op0Access: Write
    Op1Access: NoMemAccess
    Op0: r64_reg
    Op1: mem
    Used reg: RBP:Write
    Used reg: RSP:Read
00007FFAC46ACDBA sub rsp,200h
    OpCode: o64 81 /5 id
    Instruction: SUB r/m64, imm32
    Encoding: Legacy
    Mnemonic: Sub
    Code: Sub_rm64_imm32
    CpuidFeature: X64
    FlowControl: Next
    Immediate offset = 3, size = 4
    RFLAGS Written: OF, SF, ZF, AF, CF, PF
    RFLAGS Modified: OF, SF, ZF, AF, CF, PF
    Op0Access: ReadWrite
    Op1Access: Read
    Op0: r64_or_mem
    Op1: imm32sex64
    Used reg: RSP:ReadWrite
00007FFAC46ACDC1 mov rax,[7FFAC47524E0h]
    OpCode: o64 8B /r
    Instruction: MOV r64, r/m64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_r64_rm64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 3, size = 4
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_reg
    Op1: r64_or_mem
    Used reg: RAX:Write
    Used mem: [DS:0x7FFAC47524E0;UInt64;Read]
00007FFAC46ACDC8 xor rax,rsp
    OpCode: o64 33 /r
    Instruction: XOR r64, r/m64
    Encoding: Legacy
    Mnemonic: Xor
    Code: Xor_r64_rm64
    CpuidFeature: X64
    FlowControl: Next
    RFLAGS Written: SF, ZF, PF
    RFLAGS Cleared: OF, CF
    RFLAGS Undefined: AF
    RFLAGS Modified: OF, SF, ZF, AF, CF, PF
    Op0Access: ReadWrite
    Op1Access: Read
    Op0: r64_reg
    Op1: r64_or_mem
    Used reg: RAX:ReadWrite
    Used reg: RSP:Read
00007FFAC46ACDCB mov [rbp+0F0h],rax
    OpCode: o64 89 /r
    Instruction: MOV r/m64, r64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_rm64_r64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 3, size = 4
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_or_mem
    Op1: r64_reg
    Used reg: RBP:Read
    Used reg: RAX:Read
    Used mem: [SS:RBP+0xF0;UInt64;Write]
00007FFAC46ACDD2 mov r8,[7FFAC474F208h]
    OpCode: o64 8B /r
    Instruction: MOV r64, r/m64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_r64_rm64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 3, size = 4
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_reg
    Op1: r64_or_mem
    Used reg: R8:Write
    Used mem: [DS:0x7FFAC474F208;UInt64;Read]
00007FFAC46ACDD9 lea rax,[7FFAC46F4A58h]
    OpCode: o64 8D /r
    Instruction: LEA r64, m
    Encoding: Legacy
    Mnemonic: Lea
    Code: Lea_r64_m
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 3, size = 4
    Op0Access: Write
    Op1Access: NoMemAccess
    Op0: r64_reg
    Op1: mem
    Used reg: RAX:Write
00007FFAC46ACDE0 xor edi,edi
    OpCode: o32 33 /r
    Instruction: XOR r32, r/m32
    Encoding: Legacy
    Mnemonic: Xor
    Code: Xor_r32_rm32
    CpuidFeature: INTEL386
    FlowControl: Next
    RFLAGS Cleared: OF, SF, CF
    RFLAGS Set: ZF, PF
    RFLAGS Undefined: AF
    RFLAGS Modified: OF, SF, ZF, AF, CF, PF
    Op0Access: Write
    Op1Access: None
    Op0: r32_reg
    Op1: r32_or_mem
    Used reg: RDI:Write
*/
static void how_to_get_instruction_info() {
	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, EXAMPLE_CODE, sizeof(EXAMPLE_CODE), EXAMPLE_CODE_RIP, DecoderOptions::NONE);

	// Use a factory to create the instruction info if you need register and
	// memory usage. If it's something else, eg. encoding, flags, etc, there
	// are Instruction methods that can be used instead.
	InstructionInfoFactory info_factory;
	Instruction instr;
	while (decoder.can_decode()) {
		decoder.decode_out(instr);

		// Gets offsets in the instruction of the displacement and immediates and their sizes.
		// This can be useful if there are relocations in the binary. The encoder has a similar
		// method. This method must be called after decode() and you must pass in the last
		// instruction decode() returned.
		const ConstantOffsets offsets = decoder.get_constant_offsets(instr);

		// For quick hacks, it's fine to use to_string() to format an instruction,
		// but for real code, use a formatter, eg. MasmFormatter. See other examples.
		std::printf("%016" PRIX64 " %s\n", instr.ip(), to_string(instr).c_str());

		const OpCodeInfo& op_code = instr.op_code();
		// The returned reference is valid until the next info() call
		const InstructionInfo& info = info_factory.info(instr);
		const FpuStackIncrementInfo fpu_info = instr.fpu_stack_increment_info();
		std::printf("    OpCode: %s\n", std::string(op_code.op_code_string()).c_str());
		std::printf("    Instruction: %s\n", std::string(op_code.instruction_string()).c_str());
		std::printf("    Encoding: %s\n", to_string(instr.encoding()));
		std::printf("    Mnemonic: %s\n", to_string(instr.mnemonic()));
		std::printf("    Code: %s\n", to_string(instr.code()));
		std::string cpuid_features;
		for (CpuidFeature cpuid_feature : instr.cpuid_features()) {
			if (!cpuid_features.empty())
				cpuid_features += " and ";
			cpuid_features += to_string(cpuid_feature);
		}
		std::printf("    CpuidFeature: %s\n", cpuid_features.c_str());
		std::printf("    FlowControl: %s\n", to_string(instr.flow_control()));
		if (fpu_info.writes_top()) {
			if (fpu_info.increment() == 0)
				std::printf("    FPU TOP: the instruction overwrites TOP\n");
			else
				std::printf("    FPU TOP inc: %d\n", fpu_info.increment());
			std::printf("    FPU TOP cond write: %s\n", fpu_info.conditional() ? "true" : "false");
		}
		if (offsets.has_displacement())
			std::printf("    Displacement offset = %zu, size = %zu\n", offsets.displacement_offset(), offsets.displacement_size());
		if (offsets.has_immediate())
			std::printf("    Immediate offset = %zu, size = %zu\n", offsets.immediate_offset(), offsets.immediate_size());
		if (offsets.has_immediate2())
			std::printf("    Immediate #2 offset = %zu, size = %zu\n", offsets.immediate_offset2(), offsets.immediate_size2());
		if (instr.is_stack_instruction())
			std::printf("    SP Increment: %d\n", instr.stack_pointer_increment());
		if (instr.condition_code() != ConditionCode::None)
			std::printf("    Condition code: %s\n", to_string(instr.condition_code()));
		if (instr.rflags_read() != RflagsBits::NONE)
			std::printf("    RFLAGS Read: %s\n", flags(instr.rflags_read()).c_str());
		if (instr.rflags_written() != RflagsBits::NONE)
			std::printf("    RFLAGS Written: %s\n", flags(instr.rflags_written()).c_str());
		if (instr.rflags_cleared() != RflagsBits::NONE)
			std::printf("    RFLAGS Cleared: %s\n", flags(instr.rflags_cleared()).c_str());
		if (instr.rflags_set() != RflagsBits::NONE)
			std::printf("    RFLAGS Set: %s\n", flags(instr.rflags_set()).c_str());
		if (instr.rflags_undefined() != RflagsBits::NONE)
			std::printf("    RFLAGS Undefined: %s\n", flags(instr.rflags_undefined()).c_str());
		if (instr.rflags_modified() != RflagsBits::NONE)
			std::printf("    RFLAGS Modified: %s\n", flags(instr.rflags_modified()).c_str());
		const auto op_kinds = instr.op_kinds();
		if (std::any_of(op_kinds.begin(), op_kinds.end(), [](OpKind op_kind) { return op_kind == OpKind::Memory; })) {
			const std::size_t size = memory_size_ext::size(instr.memory_size());
			if (size != 0)
				std::printf("    Memory size: %zu\n", size);
		}
		for (std::uint32_t i = 0; i < instr.op_count(); i++)
			std::printf("    Op%uAccess: %s\n", i, to_string(info.op_access(i)));
		for (std::uint32_t i = 0; i < op_code.op_count(); i++)
			std::printf("    Op%u: %s\n", i, to_string(op_code.op_kind(i)));
		for (const UsedRegister& reg_info : info.used_registers())
			std::printf("    Used reg: %s\n", to_string(reg_info).c_str());
		for (const UsedMemory& mem_info : info.used_memory())
			std::printf("    Used mem: %s\n", to_string(mem_info).c_str());
	}
}

int main() {
	how_to_get_instruction_info();
	return 0;
}
