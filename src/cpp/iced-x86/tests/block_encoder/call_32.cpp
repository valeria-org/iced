// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Ported from src/rust/iced-x86/src/block_enc/tests/call_32.rs

#include "block_encoder/block_encoder_test_utils.hpp"

namespace iced_x86::tests::block_enc::call_32 {

static constexpr std::uint32_t BITNESS = 32;
static constexpr std::uint64_t ORIG_RIP = 0x8000;
static constexpr std::uint64_t NEW_RIP = 0x8000'0000;

TEST_CASE("block_encoder/call_32/call_near_fwd") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xE8, 0x07, 0x00, 0x00, 0x00,// call 0000800Ch
		/*0005*/ 0xB0, 0x00,// mov al,0
		/*0007*/ 0xB8, 0x78, 0x56, 0x34, 0x12,// mov eax,12345678h
		/*000C*/ 0x90,// nop
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xE8, 0x07, 0x00, 0x00, 0x00,// call 8000000Ch
		/*0005*/ 0xB0, 0x00,// mov al,0
		/*0007*/ 0xB8, 0x78, 0x56, 0x34, 0x12,// mov eax,12345678h
		/*000C*/ 0x90,// nop
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0005,
		0x0007,
		0x000C,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, NEW_RIP, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/call_32/call_near_bwd") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0x90,// nop
		/*0001*/ 0xE8, 0xFA, 0xFF, 0xFF, 0xFF,// call 00008000h
		/*0006*/ 0xB0, 0x00,// mov al,0
		/*0008*/ 0xB8, 0x78, 0x56, 0x34, 0x12,// mov eax,12345678h
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0x90,// nop
		/*0001*/ 0xE8, 0xFA, 0xFF, 0xFF, 0xFF,// call 80000000h
		/*0006*/ 0xB0, 0x00,// mov al,0
		/*0008*/ 0xB8, 0x78, 0x56, 0x34, 0x12,// mov eax,12345678h
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0001,
		0x0006,
		0x0008,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, NEW_RIP, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/call_32/call_near_other_near") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xE8, 0x07, 0x00, 0x00, 0x00,// call 0000800Ch
		/*0005*/ 0xB0, 0x00,// mov al,0
		/*0007*/ 0xB8, 0x78, 0x56, 0x34, 0x12,// mov eax,12345678h
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xE8, 0x08, 0x00, 0x00, 0x00,// call 0000800Ch
		/*0005*/ 0xB0, 0x00,// mov al,0
		/*0007*/ 0xB8, 0x78, 0x56, 0x34, 0x12,// mov eax,12345678h
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0005,
		0x0007,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, ORIG_RIP - 1, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/call_32/call_near_other_near_os") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0x66, 0xE8, 0x07, 0x00,// call 800Bh
		/*0004*/ 0xB0, 0x00,// mov al,0
		/*0006*/ 0xB8, 0x78, 0x56, 0x34, 0x12,// mov eax,12345678h
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0x66, 0xE8, 0x07, 0x80,// call 800Bh
		/*0004*/ 0xB0, 0x00,// mov al,0
		/*0006*/ 0xB8, 0x78, 0x56, 0x34, 0x12,// mov eax,12345678h
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0004,
		0x0006,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, NEW_RIP, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

} // namespace iced_x86::tests::block_enc::call_32
