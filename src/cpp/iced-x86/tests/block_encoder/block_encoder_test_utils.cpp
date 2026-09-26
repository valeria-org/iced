// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Ported from src/rust/iced-x86/src/block_enc/tests/mod.rs

#include "block_encoder/block_encoder_test_utils.hpp"

#include <algorithm>

#include "generated/decoder_constants.hpp"
#include "iced_x86/constant_offsets.hpp"
#include "iced_x86/decoder.hpp"

namespace iced_x86::tests::block_enc {

static std::uint64_t get_default_ip(std::uint32_t bitness) {
	switch (bitness) {
	case 16:
		return DecoderConstants::DEFAULT_IP16;
	case 32:
		return DecoderConstants::DEFAULT_IP32;
	case 64:
		return DecoderConstants::DEFAULT_IP64;
	default:
		FAIL("Invalid bitness");
	}
}

std::vector<Instruction> decode(std::uint32_t bitness, std::uint64_t rip, const std::uint8_t* data, std::size_t size, std::uint32_t options) {
	Decoder decoder = Decoder::with_ip(bitness, data, size, get_default_ip(bitness), options);
	decoder.set_ip(rip);
	std::vector<Instruction> instructions;
	while (decoder.can_decode())
		instructions.push_back(decoder.decode());
	return instructions;
}

static std::vector<RelocInfo> sort(std::vector<RelocInfo> vec) {
	std::sort(vec.begin(), vec.end(), [](const RelocInfo& a, const RelocInfo& b) {
		if (a.address != b.address)
			return a.address < b.address;
		return a.kind < b.kind;
	});
	return vec;
}

void encode_test(std::uint32_t bitness, std::uint64_t orig_rip, const std::vector<std::uint8_t>& original_data, std::uint64_t new_rip,
	const std::vector<std::uint8_t>& new_data, std::uint32_t options, std::uint32_t decoder_options,
	const std::vector<std::uint32_t>& expected_instruction_offsets, const std::vector<RelocInfo>& expected_reloc_infos) {
	auto orig_instrs = decode(bitness, orig_rip, original_data, decoder_options);
	options |= BlockEncoderOptions::RETURN_RELOC_INFOS | BlockEncoderOptions::RETURN_NEW_INSTRUCTION_OFFSETS | BlockEncoderOptions::RETURN_CONSTANT_OFFSETS;
	auto encode_result = BlockEncoder::encode(bitness, InstructionBlock(orig_instrs, new_rip), options);
	REQUIRE_MSG(encode_result.is_ok(), encode_result.error().message());
	auto& result = encode_result.value();
	const auto& encoded_bytes = result.code_buffer;
	CHECK(encoded_bytes == new_data);
	CHECK_EQ(result.rip, new_rip);
	const auto& reloc_infos = result.reloc_infos;
	const auto& new_instruction_offsets = result.new_instruction_offsets;
	const auto& constant_offsets = result.constant_offsets;
	REQUIRE_EQ(new_instruction_offsets.size(), orig_instrs.size());
	REQUIRE_EQ(constant_offsets.size(), orig_instrs.size());
	CHECK(sort(reloc_infos) == sort(expected_reloc_infos));
	CHECK(new_instruction_offsets == expected_instruction_offsets);

	std::vector<ConstantOffsets> expected_constant_offsets;
	expected_constant_offsets.reserve(constant_offsets.size());
	Decoder decoder = Decoder::with_ip(bitness, encoded_bytes.data(), encoded_bytes.size(), get_default_ip(bitness), decoder_options);
	Instruction instr;
	for (auto offset : new_instruction_offsets) {
		if (offset == UINT32_MAX)
			expected_constant_offsets.push_back(ConstantOffsets{});
		else {
			REQUIRE(decoder.try_set_position(offset).is_ok());
			decoder.set_ip(new_rip + offset);
			decoder.decode_out(instr);
			expected_constant_offsets.push_back(decoder.get_constant_offsets(instr));
		}
	}
	CHECK(constant_offsets == expected_constant_offsets);
}

} // namespace iced_x86::tests::block_enc
