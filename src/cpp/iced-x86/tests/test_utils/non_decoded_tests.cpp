// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's encoder/tests/non_decoded_tests.rs (also used by the formatter tests)

#include "test_utils/non_decoded_tests.hpp"
#include "iced_x86/code_size.hpp"
#include "iced_x86/memory_operand.hpp"
#include <cstdint>

namespace iced_x86::tests {

namespace {

Instruction c16(Instruction instruction) {
	instruction.set_code_size(CodeSize::Code16);
	return instruction;
}

Instruction c32(Instruction instruction) {
	instruction.set_code_size(CodeSize::Code32);
	return instruction;
}

Instruction c64(Instruction instruction) {
	instruction.set_code_size(CodeSize::Code64);
	return instruction;
}

std::vector<NonDecodedInfo> create_infos16() {
	return {
		{"0F", c16(Instruction::with1(Code::Popw_CS, Register::CS).value())},
		{"9B D9 30", c16(Instruction::with1(Code::Fstenv_m14byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 D9 30", c16(Instruction::with1(Code::Fstenv_m14byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::FS)).value())},
		{"9B 66 D9 30", c16(Instruction::with1(Code::Fstenv_m28byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 66 D9 30", c16(Instruction::with1(Code::Fstenv_m28byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::FS)).value())},
		{"9B D9 38", c16(Instruction::with1(Code::Fstcw_m2byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 D9 38", c16(Instruction::with1(Code::Fstcw_m2byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::FS)).value())},
		{"9B DB E0", c16(Instruction::with(Code::Feni))},
		{"9B DB E1", c16(Instruction::with(Code::Fdisi))},
		{"9B DB E2", c16(Instruction::with(Code::Fclex))},
		{"9B DB E3", c16(Instruction::with(Code::Finit))},
		{"9B DB E4", c16(Instruction::with(Code::Fsetpm))},
		{"9B DD 30", c16(Instruction::with1(Code::Fsave_m94byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 DD 30", c16(Instruction::with1(Code::Fsave_m94byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::FS)).value())},
		{"9B 66 DD 30", c16(Instruction::with1(Code::Fsave_m108byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 66 DD 30", c16(Instruction::with1(Code::Fsave_m108byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::FS)).value())},
		{"9B DD 38", c16(Instruction::with1(Code::Fstsw_m2byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 DD 38", c16(Instruction::with1(Code::Fstsw_m2byte, MemoryOperand(Register::BX, Register::SI, 1, 0, 0, false, Register::FS)).value())},
		{"9B DF E0", c16(Instruction::with1(Code::Fstsw_AX, Register::AX).value())},
		{"9B DF E1", c16(Instruction::with1(Code::Fstdw_AX, Register::AX).value())},
		{"9B DF E2", c16(Instruction::with1(Code::Fstsg_AX, Register::AX).value())},
		{"", c16(Instruction::with(Code::Zero_bytes))},
		{"77", c16(Instruction::with_declare_byte_1(0x77))},
		{"77 A9", c16(Instruction::with_declare_byte_2(0x77, 0xA9))},
		{"77 A9 CE", c16(Instruction::with_declare_byte_3(0x77, 0xA9, 0xCE))},
		{"77 A9 CE 9D", c16(Instruction::with_declare_byte_4(0x77, 0xA9, 0xCE, 0x9D))},
		{"77 A9 CE 9D 55", c16(Instruction::with_declare_byte_5(0x77, 0xA9, 0xCE, 0x9D, 0x55))},
		{"77 A9 CE 9D 55 05", c16(Instruction::with_declare_byte_6(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05))},
		{"77 A9 CE 9D 55 05 42", c16(Instruction::with_declare_byte_7(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42))},
		{"77 A9 CE 9D 55 05 42 6C", c16(Instruction::with_declare_byte_8(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C))},
		{"77 A9 CE 9D 55 05 42 6C 86", c16(Instruction::with_declare_byte_9(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86))},
		{"77 A9 CE 9D 55 05 42 6C 86 32", c16(Instruction::with_declare_byte_10(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE", c16(Instruction::with_declare_byte_11(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F", c16(Instruction::with_declare_byte_12(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34", c16(Instruction::with_declare_byte_13(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34 27", c16(Instruction::with_declare_byte_14(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34 27 AA", c16(Instruction::with_declare_byte_15(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34 27 AA 08", c16(Instruction::with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08))},
		{"A977", c16(Instruction::with_declare_word_1(0x77A9))},
		{"A977 9DCE", c16(Instruction::with_declare_word_2(0x77A9, 0xCE9D))},
		{"A977 9DCE 0555", c16(Instruction::with_declare_word_3(0x77A9, 0xCE9D, 0x5505))},
		{"A977 9DCE 0555 6C42", c16(Instruction::with_declare_word_4(0x77A9, 0xCE9D, 0x5505, 0x426C))},
		{"A977 9DCE 0555 6C42 3286", c16(Instruction::with_declare_word_5(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632))},
		{"A977 9DCE 0555 6C42 3286 4FFE", c16(Instruction::with_declare_word_6(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F))},
		{"A977 9DCE 0555 6C42 3286 4FFE 2734", c16(Instruction::with_declare_word_7(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427))},
		{"A977 9DCE 0555 6C42 3286 4FFE 2734 08AA", c16(Instruction::with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08))},
		{"9DCEA977", c16(Instruction::with_declare_dword_1(0x77A9'CE9D))},
		{"9DCEA977 6C420555", c16(Instruction::with_declare_dword_2(0x77A9'CE9D, 0x5505'426C))},
		{"9DCEA977 6C420555 4FFE3286", c16(Instruction::with_declare_dword_3(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F))},
		{"9DCEA977 6C420555 4FFE3286 08AA2734", c16(Instruction::with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08))},
		{"6C4205559DCEA977", c16(Instruction::with_declare_qword_1(0x77A9'CE9D'5505'426C))},
		{"6C4205559DCEA977 08AA27344FFE3286", c16(Instruction::with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08))},
	};
}

std::vector<NonDecodedInfo> create_infos32() {
	return {
		{"66 0F", c32(Instruction::with1(Code::Popw_CS, Register::CS).value())},
		{"9B 66 D9 30", c32(Instruction::with1(Code::Fstenv_m14byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 66 D9 30", c32(Instruction::with1(Code::Fstenv_m14byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B D9 30", c32(Instruction::with1(Code::Fstenv_m28byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 D9 30", c32(Instruction::with1(Code::Fstenv_m28byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B D9 38", c32(Instruction::with1(Code::Fstcw_m2byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 D9 38", c32(Instruction::with1(Code::Fstcw_m2byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B DB E0", c32(Instruction::with(Code::Feni))},
		{"9B DB E1", c32(Instruction::with(Code::Fdisi))},
		{"9B DB E2", c32(Instruction::with(Code::Fclex))},
		{"9B DB E3", c32(Instruction::with(Code::Finit))},
		{"9B DB E4", c32(Instruction::with(Code::Fsetpm))},
		{"9B 66 DD 30", c32(Instruction::with1(Code::Fsave_m94byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 66 DD 30", c32(Instruction::with1(Code::Fsave_m94byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B DD 30", c32(Instruction::with1(Code::Fsave_m108byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 DD 30", c32(Instruction::with1(Code::Fsave_m108byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B DD 38", c32(Instruction::with1(Code::Fstsw_m2byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 DD 38", c32(Instruction::with1(Code::Fstsw_m2byte, MemoryOperand(Register::EAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B DF E0", c32(Instruction::with1(Code::Fstsw_AX, Register::AX).value())},
		{"9B DF E1", c32(Instruction::with1(Code::Fstdw_AX, Register::AX).value())},
		{"9B DF E2", c32(Instruction::with1(Code::Fstsg_AX, Register::AX).value())},
		{"", c32(Instruction::with(Code::Zero_bytes))},
		{"77", c32(Instruction::with_declare_byte_1(0x77))},
		{"77 A9", c32(Instruction::with_declare_byte_2(0x77, 0xA9))},
		{"77 A9 CE", c32(Instruction::with_declare_byte_3(0x77, 0xA9, 0xCE))},
		{"77 A9 CE 9D", c32(Instruction::with_declare_byte_4(0x77, 0xA9, 0xCE, 0x9D))},
		{"77 A9 CE 9D 55", c32(Instruction::with_declare_byte_5(0x77, 0xA9, 0xCE, 0x9D, 0x55))},
		{"77 A9 CE 9D 55 05", c32(Instruction::with_declare_byte_6(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05))},
		{"77 A9 CE 9D 55 05 42", c32(Instruction::with_declare_byte_7(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42))},
		{"77 A9 CE 9D 55 05 42 6C", c32(Instruction::with_declare_byte_8(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C))},
		{"77 A9 CE 9D 55 05 42 6C 86", c32(Instruction::with_declare_byte_9(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86))},
		{"77 A9 CE 9D 55 05 42 6C 86 32", c32(Instruction::with_declare_byte_10(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE", c32(Instruction::with_declare_byte_11(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F", c32(Instruction::with_declare_byte_12(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34", c32(Instruction::with_declare_byte_13(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34 27", c32(Instruction::with_declare_byte_14(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34 27 AA", c32(Instruction::with_declare_byte_15(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34 27 AA 08", c32(Instruction::with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08))},
		{"A977", c32(Instruction::with_declare_word_1(0x77A9))},
		{"A977 9DCE", c32(Instruction::with_declare_word_2(0x77A9, 0xCE9D))},
		{"A977 9DCE 0555", c32(Instruction::with_declare_word_3(0x77A9, 0xCE9D, 0x5505))},
		{"A977 9DCE 0555 6C42", c32(Instruction::with_declare_word_4(0x77A9, 0xCE9D, 0x5505, 0x426C))},
		{"A977 9DCE 0555 6C42 3286", c32(Instruction::with_declare_word_5(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632))},
		{"A977 9DCE 0555 6C42 3286 4FFE", c32(Instruction::with_declare_word_6(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F))},
		{"A977 9DCE 0555 6C42 3286 4FFE 2734", c32(Instruction::with_declare_word_7(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427))},
		{"A977 9DCE 0555 6C42 3286 4FFE 2734 08AA", c32(Instruction::with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08))},
		{"9DCEA977", c32(Instruction::with_declare_dword_1(0x77A9'CE9D))},
		{"9DCEA977 6C420555", c32(Instruction::with_declare_dword_2(0x77A9'CE9D, 0x5505'426C))},
		{"9DCEA977 6C420555 4FFE3286", c32(Instruction::with_declare_dword_3(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F))},
		{"9DCEA977 6C420555 4FFE3286 08AA2734", c32(Instruction::with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08))},
		{"6C4205559DCEA977", c32(Instruction::with_declare_qword_1(0x77A9'CE9D'5505'426C))},
		{"6C4205559DCEA977 08AA27344FFE3286", c32(Instruction::with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08))},
	};
}

std::vector<NonDecodedInfo> create_infos64() {
	return {
		{"9B 66 D9 30", c64(Instruction::with1(Code::Fstenv_m14byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 66 D9 30", c64(Instruction::with1(Code::Fstenv_m14byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B D9 30", c64(Instruction::with1(Code::Fstenv_m28byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 D9 30", c64(Instruction::with1(Code::Fstenv_m28byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B D9 38", c64(Instruction::with1(Code::Fstcw_m2byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 D9 38", c64(Instruction::with1(Code::Fstcw_m2byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B DB E0", c64(Instruction::with(Code::Feni))},
		{"9B DB E1", c64(Instruction::with(Code::Fdisi))},
		{"9B DB E2", c64(Instruction::with(Code::Fclex))},
		{"9B DB E3", c64(Instruction::with(Code::Finit))},
		{"9B DB E4", c64(Instruction::with(Code::Fsetpm))},
		{"9B 66 DD 30", c64(Instruction::with1(Code::Fsave_m94byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 66 DD 30", c64(Instruction::with1(Code::Fsave_m94byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B DD 30", c64(Instruction::with1(Code::Fsave_m108byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 DD 30", c64(Instruction::with1(Code::Fsave_m108byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B DD 38", c64(Instruction::with1(Code::Fstsw_m2byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::None)).value())},
		{"9B 64 DD 38", c64(Instruction::with1(Code::Fstsw_m2byte, MemoryOperand(Register::RAX, Register::None, 1, 0, 0, false, Register::FS)).value())},
		{"9B DF E0", c64(Instruction::with1(Code::Fstsw_AX, Register::AX).value())},
		{"", c64(Instruction::with(Code::Zero_bytes))},
		{"77", c64(Instruction::with_declare_byte_1(0x77))},
		{"77 A9", c64(Instruction::with_declare_byte_2(0x77, 0xA9))},
		{"77 A9 CE", c64(Instruction::with_declare_byte_3(0x77, 0xA9, 0xCE))},
		{"77 A9 CE 9D", c64(Instruction::with_declare_byte_4(0x77, 0xA9, 0xCE, 0x9D))},
		{"77 A9 CE 9D 55", c64(Instruction::with_declare_byte_5(0x77, 0xA9, 0xCE, 0x9D, 0x55))},
		{"77 A9 CE 9D 55 05", c64(Instruction::with_declare_byte_6(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05))},
		{"77 A9 CE 9D 55 05 42", c64(Instruction::with_declare_byte_7(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42))},
		{"77 A9 CE 9D 55 05 42 6C", c64(Instruction::with_declare_byte_8(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C))},
		{"77 A9 CE 9D 55 05 42 6C 86", c64(Instruction::with_declare_byte_9(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86))},
		{"77 A9 CE 9D 55 05 42 6C 86 32", c64(Instruction::with_declare_byte_10(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE", c64(Instruction::with_declare_byte_11(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F", c64(Instruction::with_declare_byte_12(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34", c64(Instruction::with_declare_byte_13(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34 27", c64(Instruction::with_declare_byte_14(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34 27 AA", c64(Instruction::with_declare_byte_15(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA))},
		{"77 A9 CE 9D 55 05 42 6C 86 32 FE 4F 34 27 AA 08", c64(Instruction::with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08))},
		{"A977", c64(Instruction::with_declare_word_1(0x77A9))},
		{"A977 9DCE", c64(Instruction::with_declare_word_2(0x77A9, 0xCE9D))},
		{"A977 9DCE 0555", c64(Instruction::with_declare_word_3(0x77A9, 0xCE9D, 0x5505))},
		{"A977 9DCE 0555 6C42", c64(Instruction::with_declare_word_4(0x77A9, 0xCE9D, 0x5505, 0x426C))},
		{"A977 9DCE 0555 6C42 3286", c64(Instruction::with_declare_word_5(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632))},
		{"A977 9DCE 0555 6C42 3286 4FFE", c64(Instruction::with_declare_word_6(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F))},
		{"A977 9DCE 0555 6C42 3286 4FFE 2734", c64(Instruction::with_declare_word_7(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427))},
		{"A977 9DCE 0555 6C42 3286 4FFE 2734 08AA", c64(Instruction::with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08))},
		{"9DCEA977", c64(Instruction::with_declare_dword_1(0x77A9'CE9D))},
		{"9DCEA977 6C420555", c64(Instruction::with_declare_dword_2(0x77A9'CE9D, 0x5505'426C))},
		{"9DCEA977 6C420555 4FFE3286", c64(Instruction::with_declare_dword_3(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F))},
		{"9DCEA977 6C420555 4FFE3286 08AA2734", c64(Instruction::with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08))},
		{"6C4205559DCEA977", c64(Instruction::with_declare_qword_1(0x77A9'CE9D'5505'426C))},
		{"6C4205559DCEA977 08AA27344FFE3286", c64(Instruction::with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08))},
	};
}

} // namespace

const std::vector<NonDecodedInfo>& get_non_decoded_infos(std::uint32_t bitness) {
	static const std::vector<NonDecodedInfo> infos16 = create_infos16();
	static const std::vector<NonDecodedInfo> infos32 = create_infos32();
	static const std::vector<NonDecodedInfo> infos64 = create_infos64();
	switch (bitness) {
	case 16:
		return infos16;
	case 32:
		return infos32;
	case 64:
		return infos64;
	default:
		throw std::logic_error("Invalid bitness");
	}
}

std::vector<NonDecodedTest> get_non_decoded_tests() {
	std::vector<NonDecodedTest> result;
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		for (const auto& info : get_non_decoded_infos(bitness))
			result.push_back(NonDecodedTest{bitness, info.hex_bytes, info.instruction});
	}
	return result;
}

} // namespace iced_x86::tests
