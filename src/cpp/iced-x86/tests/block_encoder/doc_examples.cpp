// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// The doc examples in src/rust/iced-x86/src/block_enc.rs (BlockEncoder::encode() and encode_slice())

#include <string>
#include <vector>

#include "block_encoder/block_encoder_test_utils.hpp"
#include "iced_x86/decoder.hpp"

namespace iced_x86::tests::block_enc::doc_examples {

static std::vector<Instruction> decode_all(const std::uint8_t* bytes, std::size_t size, std::uint64_t ip) {
	std::vector<Instruction> instructions;
	Decoder decoder = Decoder::with_ip(64, bytes, size, ip, DecoderOptions::NONE);
	while (decoder.can_decode())
		instructions.push_back(decoder.decode());
	return instructions;
}

TEST_CASE("block_encoder/doc_examples/encode") {
	// je short $-2
	// add dh,cl
	// sbb r9d,ebx
	const std::uint8_t bytes[] = {0x75, 0xFC, 0x00, 0xCE, 0x41, 0x19, 0xD9};
	auto instructions = decode_all(bytes, sizeof(bytes), 0x1234'5678'9ABC'DEF0);

	// orig_rip + 8
	InstructionBlock block(instructions, 0x1234'5678'9ABC'DEF8);
	auto result = BlockEncoder::encode(64, block, BlockEncoderOptions::NONE);
	REQUIRE_MSG(result.is_ok(), result.error().message());
	CHECK(result.value().code_buffer == (std::vector<std::uint8_t>{0x75, 0xF4, 0x00, 0xCE, 0x41, 0x19, 0xD9}));
}

TEST_CASE("block_encoder/doc_examples/encode_slice") {
	// je short $-2
	// add dh,cl
	// sbb r9d,ebx
	const std::uint8_t bytes1[] = {0x75, 0xFC, 0x00, 0xCE, 0x41, 0x19, 0xD9};
	auto instructions1 = decode_all(bytes1, sizeof(bytes1), 0x1234'5678'9ABC'DEF0);

	// je short $
	const std::uint8_t bytes2[] = {0x75, 0xFE};
	auto instructions2 = decode_all(bytes2, sizeof(bytes2), 0x1234'5678);

	const InstructionBlock blocks[] = {
		// orig_rip + 8
		InstructionBlock(instructions1, 0x1234'5678'9ABC'DEF8),
		// a new ip
		InstructionBlock(instructions2, 0x8000'4000'2000'1000),
	};
	auto result = BlockEncoder::encode_slice(64, blocks, 2, BlockEncoderOptions::NONE);
	REQUIRE_MSG(result.is_ok(), result.error().message());
	const auto& bytes = result.value();
	REQUIRE_EQ(bytes.size(), 2U);
	CHECK(bytes[0].code_buffer == (std::vector<std::uint8_t>{0x75, 0xF4, 0x00, 0xCE, 0x41, 0x19, 0xD9}));
	CHECK(bytes[1].code_buffer == (std::vector<std::uint8_t>{0x75, 0xFE}));
}

TEST_CASE("block_encoder/doc_examples/multiple_instructions_same_ip_fails") {
	// Two blocks with the same instruction IPs
	const std::uint8_t bytes[] = {0x90, 0x90};
	auto instructions = decode_all(bytes, sizeof(bytes), 0x1000);
	const InstructionBlock blocks[] = {
		InstructionBlock(instructions, 0x2000),
		InstructionBlock(instructions, 0x3000),
	};
	auto result = BlockEncoder::encode_slice(64, blocks, 2, BlockEncoderOptions::NONE);
	REQUIRE(result.is_err());
	CHECK_EQ(std::string(result.error().message()), std::string("Multiple instructions with the same IP: 0x1001"));
}

} // namespace iced_x86::tests::block_enc::doc_examples
