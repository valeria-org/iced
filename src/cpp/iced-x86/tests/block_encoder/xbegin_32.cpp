// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Ported from src/rust/iced-x86/src/block_enc/tests/xbegin_32.rs

#include "block_encoder/block_encoder_test_utils.hpp"

namespace iced_x86::tests::block_enc::xbegin_32 {

static constexpr std::uint32_t BITNESS = 32;
static constexpr std::uint64_t ORIG_RIP = 0x8000;
static constexpr std::uint64_t NEW_RIP = 0x8000'0000;

TEST_CASE("block_encoder/xbegin_32/xbegin_fwd_rel16") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0x66, 0xC7, 0xF8, 0x08, 0x00,// xbegin 0000800Fh
		/*0007*/ 0xB0, 0x01,// mov al,1
		/*0009*/ 0xC7, 0xF8, 0x02, 0x00, 0x00, 0x00,// xbegin 00008011h
		/*000F*/ 0xB0, 0x02,// mov al,2
		/*0011*/ 0xB0, 0x03,// mov al,3
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0x66, 0xC7, 0xF8, 0x07, 0x00,// xbegin 8000000Eh
		/*0007*/ 0xB0, 0x01,// mov al,1
		/*0009*/ 0x66, 0xC7, 0xF8, 0x02, 0x00,// xbegin 80000010h
		/*000E*/ 0xB0, 0x02,// mov al,2
		/*0010*/ 0xB0, 0x03,// mov al,3
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0007,
		0x0009,
		0x000E,
		0x0010,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, NEW_RIP, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/xbegin_32/xbegin_bwd_rel16") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x02,// mov al,2
		/*0002*/ 0xB0, 0x03,// mov al,3
		/*0004*/ 0xB0, 0x00,// mov al,0
		/*0006*/ 0x66, 0xC7, 0xF8, 0xF5, 0xFF,// xbegin 00008000h
		/*000B*/ 0xB0, 0x01,// mov al,1
		/*000D*/ 0xC7, 0xF8, 0xEF, 0xFF, 0xFF, 0xFF,// xbegin 00008002h
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x02,// mov al,2
		/*0002*/ 0xB0, 0x03,// mov al,3
		/*0004*/ 0xB0, 0x00,// mov al,0
		/*0006*/ 0x66, 0xC7, 0xF8, 0xF5, 0xFF,// xbegin 80000000h
		/*000B*/ 0xB0, 0x01,// mov al,1
		/*000D*/ 0x66, 0xC7, 0xF8, 0xF0, 0xFF,// xbegin 80000002h
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0004,
		0x0006,
		0x000B,
		0x000D,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	encode_test(BITNESS, ORIG_RIP, original_data, NEW_RIP, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/xbegin_32/xbegin_fwd_rel32") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0x66, 0xC7, 0xF8, 0x11, 0x00,// xbegin 80000018h
		/*0007*/ 0xB0, 0x01,// mov al,1
		/*0009*/ 0xC7, 0xF8, 0x09, 0x00, 0x00, 0x00,// xbegin 80000018h
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xC7, 0xF8, 0x10, 0x00, 0x01, 0x00,// xbegin 80000018h
		/*0008*/ 0xB0, 0x01,// mov al,1
		/*000A*/ 0xC7, 0xF8, 0x08, 0x00, 0x01, 0x00,// xbegin 80000018h
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0008,
		0x000A,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	constexpr std::uint64_t ORIG_RIP = 0x8000'0000;
	encode_test(BITNESS, ORIG_RIP, original_data, ORIG_RIP - 0x0001'0000, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

TEST_CASE("block_encoder/xbegin_32/xbegin_bwd_rel32") {
	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0x66, 0xC7, 0xF8, 0xF8, 0xFF,// xbegin 7FFFFFFFh
		/*0007*/ 0xB0, 0x01,// mov al,1
		/*0009*/ 0xC7, 0xF8, 0xF0, 0xFF, 0xFF, 0xFF,// xbegin 7FFFFFFFh
	};
	const std::vector<std::uint8_t> new_data = {
		/*0000*/ 0xB0, 0x00,// mov al,0
		/*0002*/ 0xC7, 0xF8, 0xF7, 0xFF, 0xFE, 0xFF,// xbegin 7FFFFFFFh
		/*0008*/ 0xB0, 0x01,// mov al,1
		/*000A*/ 0xC7, 0xF8, 0xEF, 0xFF, 0xFE, 0xFF,// xbegin 7FFFFFFFh
	};
	const std::vector<std::uint32_t> expected_instruction_offsets = {
		0x0000,
		0x0002,
		0x0008,
		0x000A,
	};
	const std::vector<RelocInfo> expected_reloc_infos;
	constexpr std::uint32_t OPTIONS = BlockEncoderOptions::NONE;
	constexpr std::uint64_t ORIG_RIP = 0x8000'0000;
	encode_test(BITNESS, ORIG_RIP, original_data, ORIG_RIP + 0x0001'0000, new_data, OPTIONS, DECODER_OPTIONS, expected_instruction_offsets, expected_reloc_infos);
}

} // namespace iced_x86::tests::block_enc::xbegin_32
