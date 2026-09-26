// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Ported from src/rust/iced-x86/src/block_enc/tests/jmp_16.rs

#include "block_encoder/block_encoder_test_utils.hpp"

namespace iced_x86::tests::block_enc::jmp_16 {

static constexpr std::uint32_t BITNESS = 16;
static constexpr std::uint64_t ORIG_RIP = 0x8000;
static constexpr std::uint64_t NEW_RIP = 0xF000;

TEST_CASE("block_encoder/jmp_16/jmp_fwd") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xEB, 0x07,// jmp short 800Bh
		/*0004*/ 0xB0, 0x01,// mov al,1
		/*0006*/ 0xE9, 0x02, 0x00,// jmp near ptr 800Bh
		/*0009*/ 0xB0, 0x02,// mov al,2
		/*000B*/ 0x90,// nop
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xEB, 0x06,// jmp short 0F00Ah
		/*0004*/ 0xB0, 0x01,// mov al,1
		/*0006*/ 0xEB, 0x02,// jmp short 0F00Ah
		/*0008*/ 0xB0, 0x02,// mov al,2
		/*000A*/ 0x90,// nop
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0004,
		0x0006,
		0x0008,
		0x000A,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, NEW_RIP, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/jmp_16/jmp_bwd") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0x90,// nop
		/*0001*/ 0xB0, 0x00,// mov al,0
		/*0003*/ 0xEB, 0xFB,// jmp short 8000h
		/*0005*/ 0xB0, 0x01,// mov al,1
		/*0007*/ 0xE9, 0xF6, 0xFF,// jmp near ptr 8000h
		/*000A*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0x90,// nop
		/*0001*/ 0xB0, 0x00,// mov al,0
		/*0003*/ 0xEB, 0xFB,// jmp short 0F000h
		/*0005*/ 0xB0, 0x01,// mov al,1
		/*0007*/ 0xEB, 0xF7,// jmp short 0F000h
		/*0009*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0001,
		0x0003,
		0x0005,
		0x0007,
		0x0009,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, NEW_RIP, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/jmp_16/jmp_other_short_os") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0x66, 0xEB, 0x0A,// jmp short 0000800Fh
		/*0005*/ 0xB0, 0x01,// mov al,1
		/*0007*/ 0x66, 0xE9, 0x02, 0x00, 0x00, 0x00,// jmp near ptr 0000800Fh
		/*000D*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0x66, 0xEB, 0x0B,// jmp short 0000800Fh
		/*0005*/ 0xB0, 0x01,// mov al,1
		/*0007*/ 0x66, 0xEB, 0x06,// jmp short 0000800Fh
		/*000A*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0005,
		0x0007,
		0x000A,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, ORIG_RIP - 1, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/jmp_16/jmp_other_near_os") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0x66, 0xEB, 0x0A,// jmp short 0000800Fh
		/*0005*/ 0xB0, 0x01,// mov al,1
		/*0007*/ 0x66, 0xE9, 0x02, 0x00, 0x00, 0x00,// jmp near ptr 0000800Fh
		/*000D*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0x66, 0xE9, 0x07, 0xF0, 0xFF, 0xFF,// jmp near ptr 0000800Fh
		/*0008*/ 0xB0, 0x01,// mov al,1
		/*000A*/ 0x66, 0xE9, 0xFF, 0xEF, 0xFF, 0xFF,// jmp near ptr 0000800Fh
		/*0010*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0008,
		0x000A,
		0x0010,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, ORIG_RIP + 0x1000, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/jmp_16/jmp_other_short") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xEB, 0x07,// jmp short 800Bh
		/*0004*/ 0xB0, 0x01,// mov al,1
		/*0006*/ 0xE9, 0x02, 0x00,// jmp near ptr 800Bh
		/*0009*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xEB, 0x08,// jmp short 800Bh
		/*0004*/ 0xB0, 0x01,// mov al,1
		/*0006*/ 0xEB, 0x04,// jmp short 800Bh
		/*0008*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0004,
		0x0006,
		0x0008,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, ORIG_RIP - 1, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/jmp_16/jmp_other_near") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xEB, 0x07,// jmp short 800Bh
		/*0004*/ 0xB0, 0x01,// mov al,1
		/*0006*/ 0xE9, 0x02, 0x00,// jmp near ptr 800Bh
		/*0009*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xE9, 0x06, 0xF0,// jmp near ptr 800Bh
		/*0005*/ 0xB0, 0x01,// mov al,1
		/*0007*/ 0xE9, 0x01, 0xF0,// jmp near ptr 800Bh
		/*000A*/ 0xB0, 0x02,// mov al,2
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0005,
		0x0007,
		0x000A,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, ORIG_RIP + 0x1000, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/jmp_16/jmp_fwd_no_opt") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xEB, 0x07,// jmp short 800Bh
		/*0004*/ 0xB0, 0x01,// mov al,1
		/*0006*/ 0xE9, 0x02, 0x00,// jmp near ptr 800Bh
		/*0009*/ 0xB0, 0x02,// mov al,2
		/*000B*/ 0x90,// nop
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xEB, 0x07,// jmp short 800Bh
		/*0004*/ 0xB0, 0x01,// mov al,1
		/*0006*/ 0xE9, 0x02, 0x00,// jmp near ptr 800Bh
		/*0009*/ 0xB0, 0x02,// mov al,2
		/*000B*/ 0x90,// nop
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0004,
		0x0006,
		0x0009,
		0x000B,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::DONT_FIX_BRANCHES;
	encode_test(BITNESS, ORIG_RIP, original_data, NEW_RIP, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

} // namespace iced_x86::tests::block_enc::jmp_16
