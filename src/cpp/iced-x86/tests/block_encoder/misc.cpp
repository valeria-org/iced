// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Ported from src/rust/iced-x86/src/block_enc/tests/misc.rs

#include <utility>

#include "block_encoder/block_encoder_test_utils.hpp"
#include "iced_x86/code.hpp"
#include "iced_x86/memory_operand.hpp"
#include "iced_x86/register.hpp"

namespace iced_x86::tests::block_enc::misc {

TEST_CASE("block_encoder/misc/encode_zero_blocks") {
	{
		auto result = BlockEncoder::encode_slice(16, nullptr, 0, BlockEncoderOptions::NONE);
		REQUIRE(result.is_ok());
		CHECK(result.value().empty());
	}
	{
		auto result = BlockEncoder::encode_slice(32, nullptr, 0, BlockEncoderOptions::NONE);
		REQUIRE(result.is_ok());
		CHECK(result.value().empty());
	}
	{
		auto result = BlockEncoder::encode_slice(64, std::vector<InstructionBlock>{}, BlockEncoderOptions::NONE);
		REQUIRE(result.is_ok());
		CHECK(result.value().empty());
	}
}

TEST_CASE("block_encoder/misc/encode_zero_instructions") {
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		auto encode_result = BlockEncoder::encode(bitness, InstructionBlock(nullptr, 0, 0), BlockEncoderOptions::NONE);
		REQUIRE(encode_result.is_ok());
		const auto& result = encode_result.value();
		CHECK_EQ(result.rip, 0U);
		CHECK(result.code_buffer.empty());
		CHECK(result.reloc_infos.empty());
		CHECK(result.new_instruction_offsets.empty());
		CHECK(result.constant_offsets.empty());
	}
}

TEST_CASE("block_encoder/misc/default_args") {
	constexpr std::uint32_t BITNESS = 64;
	constexpr std::uint64_t ORIG_RIP = 0x1234'5678'9ABC'DE00;
	constexpr std::uint64_t NEW_RIP = 0x8000'0000'0000'0000;

	const std::vector<std::uint8_t> original_data = {
		/*0000*/ 0xB0, 0x00,                        // mov al,0
		/*0002*/ 0xEB, 0x09,                        // jmp short 123456789ABCDE0Dh
		/*0004*/ 0xB0, 0x01,                        // mov al,1
		/*0006*/ 0xE9, 0x03, 0x00, 0x00, 0x00,      // jmp near ptr 123456789ABCDE0Eh
		/*000B*/ 0xB0, 0x02,                        // mov al,2
	};
	auto instructions = decode(BITNESS, ORIG_RIP, original_data, DecoderOptions::NONE);
	auto encode_result = BlockEncoder::encode(BITNESS, InstructionBlock(instructions, NEW_RIP), BlockEncoderOptions::NONE);
	REQUIRE(encode_result.is_ok());
	const auto& result = encode_result.value();
	CHECK_EQ(result.rip, NEW_RIP);
	CHECK_EQ(result.code_buffer.size(), 0x28U);
	CHECK(result.reloc_infos.empty());
	CHECK(result.new_instruction_offsets.empty());
	CHECK(result.constant_offsets.empty());
}

TEST_CASE("block_encoder/misc/verify_result_vectors") {
	constexpr std::uint32_t BITNESS = 64;
	constexpr std::uint64_t ORIG_RIP1 = 0x1234'5678'9ABC'DE00;
	constexpr std::uint64_t ORIG_RIP2 = 0x2234'5678'9ABC'DE00;
	constexpr std::uint64_t NEW_RIP1 = 0x8000'0000'0000'0000;
	constexpr std::uint64_t NEW_RIP2 = 0x9000'0000'0000'0000;

	const std::uint32_t tests[] = {
		BlockEncoderOptions::RETURN_RELOC_INFOS,
		BlockEncoderOptions::RETURN_NEW_INSTRUCTION_OFFSETS,
		BlockEncoderOptions::RETURN_CONSTANT_OFFSETS,
	};
	for (std::uint32_t options : tests) {
		{
			auto instructions1 = decode(BITNESS, ORIG_RIP1, {0xE9, 0x56, 0x78, 0xA5, 0x5A}, DecoderOptions::NONE);
			auto encode_result = BlockEncoder::encode(BITNESS, InstructionBlock(instructions1, NEW_RIP1), options);
			REQUIRE(encode_result.is_ok());
			const auto& result = encode_result.value();
			CHECK_EQ(result.rip, NEW_RIP1);
			if ((options & BlockEncoderOptions::RETURN_RELOC_INFOS) != 0)
				CHECK_EQ(result.reloc_infos.size(), 1U);
			else
				CHECK(result.reloc_infos.empty());
			if ((options & BlockEncoderOptions::RETURN_NEW_INSTRUCTION_OFFSETS) != 0)
				CHECK_EQ(result.new_instruction_offsets.size(), 1U);
			else
				CHECK(result.new_instruction_offsets.empty());
			if ((options & BlockEncoderOptions::RETURN_CONSTANT_OFFSETS) != 0)
				CHECK_EQ(result.constant_offsets.size(), 1U);
			else
				CHECK(result.constant_offsets.empty());
		}
		{
			auto instructions1 = decode(BITNESS, ORIG_RIP1, {0xE9, 0x56, 0x78, 0xA5, 0x5A}, DecoderOptions::NONE);
			auto instructions2 = decode(BITNESS, ORIG_RIP2, {0x90, 0xE9, 0x56, 0x78, 0xA5, 0x5A}, DecoderOptions::NONE);
			const InstructionBlock blocks[] = {
				InstructionBlock(instructions1, NEW_RIP1),
				InstructionBlock(instructions2, NEW_RIP2),
			};
			auto encode_result = BlockEncoder::encode_slice(BITNESS, blocks, 2, options);
			REQUIRE(encode_result.is_ok());
			const auto& result = encode_result.value();
			REQUIRE_EQ(result.size(), 2U);
			CHECK_EQ(result[0].rip, NEW_RIP1);
			CHECK_EQ(result[1].rip, NEW_RIP2);
			if ((options & BlockEncoderOptions::RETURN_RELOC_INFOS) != 0) {
				CHECK_EQ(result[0].reloc_infos.size(), 1U);
				CHECK_EQ(result[1].reloc_infos.size(), 1U);
			} else {
				CHECK(result[0].reloc_infos.empty());
				CHECK(result[1].reloc_infos.empty());
			}
			if ((options & BlockEncoderOptions::RETURN_NEW_INSTRUCTION_OFFSETS) != 0) {
				CHECK_EQ(result[0].new_instruction_offsets.size(), 1U);
				CHECK_EQ(result[1].new_instruction_offsets.size(), 2U);
			} else {
				CHECK(result[0].new_instruction_offsets.empty());
				CHECK(result[1].new_instruction_offsets.empty());
			}
			if ((options & BlockEncoderOptions::RETURN_CONSTANT_OFFSETS) != 0) {
				CHECK_EQ(result[0].constant_offsets.size(), 1U);
				CHECK_EQ(result[1].constant_offsets.size(), 2U);
			} else {
				CHECK(result[0].constant_offsets.empty());
				CHECK(result[1].constant_offsets.empty());
			}
		}
	}
}

TEST_CASE("block_encoder/misc/encode_declare_byte") {
	constexpr std::uint32_t BITNESS = 64;
	constexpr std::uint64_t NEW_RIP = 0x8000'0000'0000'0000;

	const std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>> test_data[] = {
		{{0x5A}, {0x90, 0x5A, 0x90}},
		{{0xF0, 0xD2, 0x7A, 0x18, 0xA0}, {0x90, 0xF0, 0xD2, 0x7A, 0x18, 0xA0, 0x90}},
		{{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08},
			{0x90, 0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08, 0x90}},
	};

	for (const auto& info : test_data) {
		auto db = Instruction::with_declare_byte(info.first.data(), info.first.size());
		REQUIRE(db.is_ok());
		const Instruction instructions[] = {
			Instruction::with(Code::Nopd),
			db.value(),
			Instruction::with(Code::Nopd),
		};

		auto encode_result = BlockEncoder::encode(BITNESS, InstructionBlock(instructions, 3, NEW_RIP), BlockEncoderOptions::NONE);
		REQUIRE(encode_result.is_ok());
		const auto& result = encode_result.value();
		CHECK(result.code_buffer == info.second);
		CHECK_EQ(result.rip, NEW_RIP);
		CHECK(result.reloc_infos.empty());
		CHECK(result.new_instruction_offsets.empty());
		CHECK(result.constant_offsets.empty());
	}
}

TEST_CASE("block_encoder/misc/encode_with_invalid_bitness_fails_0") {
	const Instruction instructions[] = {Instruction()};
	CHECK(BlockEncoder::encode(0, InstructionBlock(instructions, 1, 0), BlockEncoderOptions::NONE).is_err());
}

TEST_CASE("block_encoder/misc/encode_with_invalid_bitness_fails_128") {
	const Instruction instructions[] = {Instruction()};
	CHECK(BlockEncoder::encode(128, InstructionBlock(instructions, 1, 0), BlockEncoderOptions::NONE).is_err());
}

TEST_CASE("block_encoder/misc/encode_slice_with_invalid_bitness_fails_0") {
	const Instruction instructions[] = {Instruction()};
	const InstructionBlock blocks[] = {InstructionBlock(instructions, 1, 0)};
	CHECK(BlockEncoder::encode_slice(0, blocks, 1, BlockEncoderOptions::NONE).is_err());
}

TEST_CASE("block_encoder/misc/encode_slice_with_invalid_bitness_fails_128") {
	const Instruction instructions[] = {Instruction()};
	const InstructionBlock blocks[] = {InstructionBlock(instructions, 1, 0)};
	CHECK(BlockEncoder::encode_slice(128, blocks, 1, BlockEncoderOptions::NONE).is_err());
}

TEST_CASE("block_encoder/misc/encode_rip_rel_mem_op") {
	auto instr = Instruction::with2(Code::Add_r32_rm32, Register::ECX,
		MemoryOperand(Register::RIP, Register::None, 1, static_cast<std::int64_t>(0x1234'5678'9ABC'DEF1), 8, false, Register::None));
	REQUIRE(instr.is_ok());
	const InstructionBlock blocks[] = {InstructionBlock(&instr.value(), 1, 0x1234'5678'ABCD'EF02)};
	auto vec_result = BlockEncoder::encode_slice(64, blocks, 1, BlockEncoderOptions::NONE);
	REQUIRE(vec_result.is_ok());
	REQUIRE_EQ(vec_result.value().size(), 1U);
	const auto& result = vec_result.value()[0];
	CHECK(result.code_buffer == (std::vector<std::uint8_t>{0x03, 0x0D, 0xE9, 0xEF, 0xEE, 0xEE}));
}

} // namespace iced_x86::tests::block_enc::misc
