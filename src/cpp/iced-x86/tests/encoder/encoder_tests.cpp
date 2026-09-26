// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's encoder/tests/mod.rs

#include "encoder/encoder_test_utils.hpp"
#include "encoder/op_code_test_case.hpp"
#include "iced_x86/code_ext.hpp"
#include "iced_x86/encoder.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/memory_operand.hpp"
#include "iced_x86/op_code_info.hpp"
#include "internal/encoder/encoder_internal.hpp"
#include "internal/encoder/op_code_handler.hpp"
#include "test_framework.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/non_decoded_tests.hpp"
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace iced_x86::tests {

namespace {

using internal::EncoderInternal;

#if ICED_X86_TESTS_HAS_DECODER

ConstantOffsets fix_constant_offsets(const ConstantOffsets& co, std::size_t orig_len, std::size_t new_len) {
	const std::uint8_t diff = static_cast<std::uint8_t>(orig_len - new_len);
	std::uint8_t displacement_offset = static_cast<std::uint8_t>(co.displacement_offset());
	std::uint8_t immediate_offset = static_cast<std::uint8_t>(co.immediate_offset());
	std::uint8_t immediate_offset2 = static_cast<std::uint8_t>(co.immediate_offset2());
	if (co.has_displacement())
		displacement_offset = static_cast<std::uint8_t>(displacement_offset + diff);
	if (co.has_immediate())
		immediate_offset = static_cast<std::uint8_t>(immediate_offset + diff);
	if (co.has_immediate2())
		immediate_offset2 = static_cast<std::uint8_t>(immediate_offset2 + diff);
	return ConstantOffsets(displacement_offset, static_cast<std::uint8_t>(co.displacement_size()), immediate_offset,
		static_cast<std::uint8_t>(co.immediate_size()), immediate_offset2, static_cast<std::uint8_t>(co.immediate_size2()));
}

void verify_constant_offsets(const ConstantOffsets& expected, const ConstantOffsets& actual) {
	CHECK_EQ(actual.immediate_offset(), expected.immediate_offset());
	CHECK_EQ(actual.immediate_size(), expected.immediate_size());
	CHECK_EQ(actual.immediate_offset2(), expected.immediate_offset2());
	CHECK_EQ(actual.immediate_size2(), expected.immediate_size2());
	CHECK_EQ(actual.displacement_offset(), expected.displacement_offset());
	CHECK_EQ(actual.displacement_size(), expected.displacement_size());
}

void encode_test(const DecoderTestInfo& info) {
	const auto orig_bytes = to_vec_u8(info.hex_bytes());
	auto decoder = create_decoder(info.bitness(), orig_bytes, info.ip(), info.decoder_options()).decoder;
	const std::uint64_t orig_rip = decoder.ip();
	const Instruction orig_instr = decoder.decode();
	const ConstantOffsets orig_co = decoder.get_constant_offsets(orig_instr);
	REQUIRE_EQ(orig_instr.code(), info.code());
	CHECK_EQ(orig_instr.len(), orig_bytes.size());
	CHECK(orig_instr.len() <= IcedConstants::MAX_INSTRUCTION_LENGTH);
	CHECK_EQ(orig_instr.ip16(), static_cast<std::uint16_t>(orig_rip));
	CHECK_EQ(orig_instr.ip32(), static_cast<std::uint32_t>(orig_rip));
	CHECK_EQ(orig_instr.ip(), orig_rip);
	const std::uint64_t after_rip = decoder.ip();
	CHECK_EQ(orig_instr.next_ip16(), static_cast<std::uint16_t>(after_rip));
	CHECK_EQ(orig_instr.next_ip32(), static_cast<std::uint32_t>(after_rip));
	CHECK_EQ(orig_instr.next_ip(), after_rip);

	Encoder encoder(decoder.bitness());
	CHECK_EQ(encoder.bitness(), info.bitness());
	const Instruction orig_instr_copy = orig_instr;
	auto encode_result = encoder.encode(orig_instr, orig_rip);
	REQUIRE_MSG(encode_result.is_ok(), std::string(encode_result.is_err() ? encode_result.error().message() : "") + " (" + info.hex_bytes() + ")");
	const std::size_t encoded_instr_len = encode_result.value();
	const ConstantOffsets encoded_co = fix_constant_offsets(encoder.get_constant_offsets(), orig_instr.len(), encoded_instr_len);
	verify_constant_offsets(orig_co, encoded_co);
	const auto encoded_bytes = encoder.take_buffer();
	CHECK_EQ(encoded_instr_len, encoded_bytes.size());
	CHECK(orig_instr.eq_all_bits(orig_instr_copy));

	const auto expected_bytes = to_vec_u8(info.encoded_hex_bytes());
	if (expected_bytes != encoded_bytes) {
		CHECK_EQ(slice_u8_to_string(encoded_bytes), slice_u8_to_string(expected_bytes));
		FAIL("Invalid encoded bytes");
	}

	auto new_decoder = create_decoder(info.bitness(), encoded_bytes, info.ip(), info.decoder_options()).decoder;
	Instruction new_instr = new_decoder.decode();
	CHECK_EQ(new_instr.code(), info.code());
	CHECK_EQ(new_instr.len(), encoded_bytes.size());
	new_instr.set_len(orig_instr.len());
	new_instr.set_next_ip(orig_instr.next_ip());
	CHECK(orig_instr.eq_all_bits(new_instr));
}

void encode(std::uint32_t bitness) {
	for (const auto& info : encoder_tests(true, false)) {
		if (info.bitness() == bitness)
			encode_test(info);
	}
}

#endif

void non_decode_encode(std::uint32_t bitness) {
	constexpr std::uint64_t RIP = 0;
	for (const auto& tc : get_non_decoded_tests()) {
		if (tc.bitness != bitness)
			continue;
		const auto expected_bytes = to_vec_u8(tc.hex_bytes);
		Encoder encoder(bitness);
		CHECK_EQ(encoder.bitness(), bitness);
		const std::size_t encoded_instr_len = encoder.encode(tc.instruction, RIP).value();
		const auto encoded_bytes = encoder.take_buffer();
		CHECK(encoded_bytes == expected_bytes);
		CHECK_EQ(encoded_instr_len, encoded_bytes.size());
	}
}

#if ICED_X86_TESTS_HAS_DECODER

std::vector<std::pair<std::uint32_t, std::shared_ptr<DecoderTestInfo>>> get_invalid_test_cases() {
	std::vector<std::pair<std::uint32_t, std::shared_ptr<DecoderTestInfo>>> result;
	for (auto& tc_value : encoder_tests(false, false)) {
		auto tc = std::make_shared<DecoderTestInfo>(std::move(tc_value));
		if (code32_only().count(tc->code()) != 0)
			result.emplace_back(64, tc);
		if (code64_only().count(tc->code()) != 0) {
			result.emplace_back(16, tc);
			result.emplace_back(32, tc);
		}
	}
	return result;
}

void encode_invalid_test(std::uint32_t invalid_bitness, const DecoderTestInfo& tc) {
	const auto orig_bytes = to_vec_u8(tc.hex_bytes());
	auto decoder = create_decoder(tc.bitness(), orig_bytes, tc.ip(), tc.decoder_options()).decoder;
	const std::uint64_t orig_rip = decoder.ip();
	const Instruction orig_instr = decoder.decode();
	REQUIRE_EQ(orig_instr.code(), tc.code());
	CHECK_EQ(orig_instr.len(), orig_bytes.size());
	CHECK(orig_instr.len() <= IcedConstants::MAX_INSTRUCTION_LENGTH);
	CHECK_EQ(orig_instr.ip16(), static_cast<std::uint16_t>(orig_rip));
	CHECK_EQ(orig_instr.ip32(), static_cast<std::uint32_t>(orig_rip));
	CHECK_EQ(orig_instr.ip(), orig_rip);
	const std::uint64_t after_rip = decoder.ip();
	CHECK_EQ(orig_instr.next_ip16(), static_cast<std::uint16_t>(after_rip));
	CHECK_EQ(orig_instr.next_ip32(), static_cast<std::uint32_t>(after_rip));
	CHECK_EQ(orig_instr.next_ip(), after_rip);

	Encoder encoder(invalid_bitness);
	auto result = encoder.encode(orig_instr, orig_rip);
	REQUIRE(result.is_err());
	const char* expected_err = invalid_bitness == 64 ? EncoderInternal::ERROR_ONLY_1632_BIT_MODE : EncoderInternal::ERROR_ONLY_64_BIT_MODE;
	CHECK_EQ(std::string(result.error().message()), std::string(expected_err));
}

#endif

} // namespace

#if ICED_X86_TESTS_HAS_DECODER

TEST_CASE("encoder/encode_16") { encode(16); }

TEST_CASE("encoder/encode_32") { encode(32); }

TEST_CASE("encoder/encode_64") { encode(64); }

#endif

TEST_CASE("encoder/non_decode_encode_16") { non_decode_encode(16); }

TEST_CASE("encoder/non_decode_encode_32") { non_decode_encode(32); }

TEST_CASE("encoder/non_decode_encode_64") { non_decode_encode(64); }

#if ICED_X86_TESTS_HAS_DECODER

TEST_CASE("encoder/encode_invalid") {
	for (const auto& i : get_invalid_test_cases())
		encode_invalid_test(i.first, *i.second);
}

TEST_CASE("encoder/encode_with_error") {
	// xchg ah,[rdx+rsi+16h]
	const std::vector<std::uint8_t> bytes = {0x86, 0x64, 0x32, 0x16};
	Decoder decoder(64, bytes, DecoderOptions::NONE);
	Instruction instr = decoder.decode();

	Encoder encoder(decoder.bitness());
	CHECK(encoder.encode(instr, instr.ip()).is_ok());
	instr.set_op1_register(Register::CR0);
	CHECK(encoder.encode(instr, instr.ip()).is_err());
	instr.set_op1_register(Register::AL);
	CHECK(encoder.encode(instr, instr.ip()).is_ok());
}

#endif

#if ICED_X86_TESTS_CAN_CHECK_ABORT
TEST_CASE("encoder/new_panics_if_bitness_0") {
	CHECK(aborts([] { Encoder encoder(0); }));
}

TEST_CASE("encoder/new_panics_if_bitness_128") {
	CHECK(aborts([] { Encoder encoder(128); }));
}
#endif

TEST_CASE("encoder/try_new_fails_if_bitness_0") { CHECK(Encoder::try_new(0).is_err()); }

TEST_CASE("encoder/try_new_fails_if_bitness_128") { CHECK(Encoder::try_new(128).is_err()); }

TEST_CASE("encoder/with_capacity_fails_if_bitness_0") { CHECK(Encoder::try_with_capacity(0, 1).is_err()); }

TEST_CASE("encoder/with_capacity_failss_if_bitness_128") { CHECK(Encoder::try_with_capacity(128, 1).is_err()); }

TEST_CASE("encoder/try_with_capacity_works") {
	auto encoder = Encoder::try_with_capacity(64, 211).value();
	const auto buffer = encoder.take_buffer();
	CHECK(buffer.empty());
	CHECK_EQ(buffer.capacity(), static_cast<std::size_t>(211));
}

TEST_CASE("encoder/set_buffer_works") {
	Encoder encoder(64);
	encoder.set_buffer(std::vector<std::uint8_t>{10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0});
	CHECK((encoder.take_buffer() == std::vector<std::uint8_t>{10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}));
}

TEST_CASE("encoder/encode_invalid_code_value_is_an_error") {
	Instruction instr;
	instr.set_code(Code::INVALID);

	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		Encoder encoder(bitness);
		auto result = encoder.encode(instr, 0);
		REQUIRE(result.is_err());
		CHECK_EQ(std::string(internal::INVALID_HANDLER_ERROR_MESSAGE), std::string(result.error().message()));
	}
}

TEST_CASE("encoder/displsize_eq_1_uses_long_form_if_it_does_not_fit_in_1_byte") {
	constexpr std::uint64_t RIP = 0;

	const auto memory16 = MemoryOperand::with_base_displ_size(Register::SI, 0x1234, 1);
	const auto memory32 = MemoryOperand::with_base_displ_size(Register::ESI, 0x1234'5678, 1);
	const auto memory64 = MemoryOperand::with_base_displ_size(Register::R14, 0x1234'5678, 1);

	const std::vector<std::tuple<std::uint32_t, const char*, std::uint64_t, Instruction>> tests = {
		{16, "0F10 8C 3412", RIP, Instruction::with2(Code::Movups_xmm_xmmm128, Register::XMM1, memory16).value()},
		{32, "0F10 8E 78563412", RIP, Instruction::with2(Code::Movups_xmm_xmmm128, Register::XMM1, memory32).value()},
		{64, "41 0F10 8E 78563412", RIP, Instruction::with2(Code::Movups_xmm_xmmm128, Register::XMM1, memory64).value()},

		{16, "C5F8 10 8C 3412", RIP, Instruction::with2(Code::VEX_Vmovups_xmm_xmmm128, Register::XMM1, memory16).value()},
		{32, "C5F8 10 8E 78563412", RIP, Instruction::with2(Code::VEX_Vmovups_xmm_xmmm128, Register::XMM1, memory32).value()},
		{64, "C4C178 10 8E 78563412", RIP, Instruction::with2(Code::VEX_Vmovups_xmm_xmmm128, Register::XMM1, memory64).value()},

		{16, "62 F17C08 10 8C 3412", RIP, Instruction::with2(Code::EVEX_Vmovups_xmm_k1z_xmmm128, Register::XMM1, memory16).value()},
		{32, "62 F17C08 10 8E 78563412", RIP, Instruction::with2(Code::EVEX_Vmovups_xmm_k1z_xmmm128, Register::XMM1, memory32).value()},
		{64, "62 D17C08 10 8E 78563412", RIP, Instruction::with2(Code::EVEX_Vmovups_xmm_k1z_xmmm128, Register::XMM1, memory64).value()},

		{16, "8F E878C0 8C 3412 A5", RIP, Instruction::with3(Code::XOP_Vprotb_xmm_xmmm128_imm8, Register::XMM1, memory16, 0xA5).value()},
		{32, "8F E878C0 8E 78563412 A5", RIP, Instruction::with3(Code::XOP_Vprotb_xmm_xmmm128_imm8, Register::XMM1, memory32, 0xA5).value()},
		{64, "8F C878C0 8E 78563412 A5", RIP, Instruction::with3(Code::XOP_Vprotb_xmm_xmmm128_imm8, Register::XMM1, memory64, 0xA5).value()},

		{16, "0F0F 8C 3412 0C", RIP, Instruction::with2(Code::D3NOW_Pi2fw_mm_mmm64, Register::MM1, memory16).value()},
		{32, "0F0F 8E 78563412 0C", RIP, Instruction::with2(Code::D3NOW_Pi2fw_mm_mmm64, Register::MM1, memory32).value()},
		{64, "0F0F 8E 78563412 0C", RIP, Instruction::with2(Code::D3NOW_Pi2fw_mm_mmm64, Register::MM1, memory64).value()},

		{64, "62 D17808 28 8E 78563412", RIP, Instruction::with2(Code::MVEX_Vmovaps_zmm_k1_zmmmt, Register::ZMM1, memory64).value()},
	};

	// If it fails, add more tests above (16-bit, 32-bit, and 64-bit test cases)
	static_assert(IcedConstants::ENCODING_KIND_ENUM_COUNT == 6, "");

	for (const auto& [bitness, hex_bytes, rip, instruction] : tests) {
		const auto expected_bytes = to_vec_u8(hex_bytes);
		Encoder encoder(bitness);
		const std::size_t encoded_length = encoder.encode(instruction, rip).value();
		CHECK(expected_bytes == encoder.take_buffer());
		CHECK_EQ(expected_bytes.size(), encoded_length);
	}
}

namespace {
void encode_no_displ_test(std::uint32_t bitness, const Instruction& instr, const std::vector<std::uint8_t>& expected) {
	Encoder encoder(bitness);
	const std::size_t len = encoder.encode(instr, 0).value();
	const auto actual = encoder.take_buffer();
	CHECK_EQ(len, actual.size());
	CHECK(actual == expected);
}
} // namespace

TEST_CASE("encoder/encode_bp_with_no_displ") {
	encode_no_displ_test(16, Instruction::with2(Code::Mov_r16_rm16, Register::AX, MemoryOperand::with_base(Register::BP)).value(), {0x8B, 0x46, 0x00});
}

TEST_CASE("encoder/encode_ebp_with_no_displ") {
	encode_no_displ_test(32, Instruction::with2(Code::Mov_r32_rm32, Register::EAX, MemoryOperand::with_base(Register::EBP)).value(), {0x8B, 0x45, 0x00});
}

TEST_CASE("encoder/encode_ebp_edx_with_no_displ") {
	encode_no_displ_test(32, Instruction::with2(Code::Mov_r32_rm32, Register::EAX, MemoryOperand::with_base_index(Register::EBP, Register::EDX)).value(),
		{0x8B, 0x44, 0x15, 0x00});
}

TEST_CASE("encoder/encode_r13d_with_no_displ") {
	encode_no_displ_test(64, Instruction::with2(Code::Mov_r32_rm32, Register::EAX, MemoryOperand::with_base(Register::R13D)).value(),
		{0x67, 0x41, 0x8B, 0x45, 0x00});
}

TEST_CASE("encoder/encode_r13d_edx_with_no_displ") {
	encode_no_displ_test(64, Instruction::with2(Code::Mov_r32_rm32, Register::EAX, MemoryOperand::with_base_index(Register::R13D, Register::EDX)).value(),
		{0x67, 0x41, 0x8B, 0x44, 0x15, 0x00});
}

TEST_CASE("encoder/encode_rbp_with_no_displ") {
	encode_no_displ_test(64, Instruction::with2(Code::Mov_r64_rm64, Register::RAX, MemoryOperand::with_base(Register::RBP)).value(),
		{0x48, 0x8B, 0x45, 0x00});
}

TEST_CASE("encoder/encode_rbp_rdx_with_no_displ") {
	encode_no_displ_test(64, Instruction::with2(Code::Mov_r64_rm64, Register::RAX, MemoryOperand::with_base_index(Register::RBP, Register::RDX)).value(),
		{0x48, 0x8B, 0x44, 0x15, 0x00});
}

TEST_CASE("encoder/encode_r13_with_no_displ") {
	encode_no_displ_test(64, Instruction::with2(Code::Mov_r64_rm64, Register::RAX, MemoryOperand::with_base(Register::R13)).value(),
		{0x49, 0x8B, 0x45, 0x00});
}

TEST_CASE("encoder/encode_r13_rdx_with_no_displ") {
	encode_no_displ_test(64, Instruction::with2(Code::Mov_r64_rm64, Register::RAX, MemoryOperand::with_base_index(Register::R13, Register::RDX)).value(),
		{0x49, 0x8B, 0x44, 0x15, 0x00});
}

TEST_CASE("encoder/verify_encoder_options") {
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		const Encoder encoder(bitness);
		CHECK(!encoder.prevent_vex2());
		CHECK_EQ(encoder.vex_wig(), 0U);
		CHECK_EQ(encoder.vex_lig(), 0U);
		CHECK_EQ(encoder.evex_wig(), 0U);
		CHECK_EQ(encoder.evex_lig(), 0U);
		CHECK_EQ(encoder.mvex_wig(), 0U);
	}
}

TEST_CASE("encoder/get_set_wig_lig_options") {
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		Encoder encoder(bitness);

		encoder.set_vex_lig(1);
		encoder.set_vex_wig(0);
		CHECK_EQ(encoder.vex_wig(), 0U);
		CHECK_EQ(encoder.vex_lig(), 1U);
		encoder.set_vex_wig(1);
		CHECK_EQ(encoder.vex_wig(), 1U);
		CHECK_EQ(encoder.vex_lig(), 1U);

		encoder.set_vex_wig(0xFFFF'FFFE);
		CHECK_EQ(encoder.vex_wig(), 0U);
		CHECK_EQ(encoder.vex_lig(), 1U);
		encoder.set_vex_wig(0xFFFF'FFFF);
		CHECK_EQ(encoder.vex_wig(), 1U);
		CHECK_EQ(encoder.vex_lig(), 1U);

		encoder.set_vex_wig(1);
		encoder.set_vex_lig(0);
		CHECK_EQ(encoder.vex_lig(), 0U);
		CHECK_EQ(encoder.vex_wig(), 1U);
		encoder.set_vex_lig(1);
		CHECK_EQ(encoder.vex_lig(), 1U);
		CHECK_EQ(encoder.vex_wig(), 1U);

		encoder.set_vex_lig(0xFFFF'FFFE);
		CHECK_EQ(encoder.vex_lig(), 0U);
		CHECK_EQ(encoder.vex_wig(), 1U);
		encoder.set_vex_lig(0xFFFF'FFFF);
		CHECK_EQ(encoder.vex_lig(), 1U);
		CHECK_EQ(encoder.vex_wig(), 1U);

		encoder.set_evex_lig(3);
		encoder.set_evex_wig(0);
		CHECK_EQ(encoder.evex_wig(), 0U);
		CHECK_EQ(encoder.evex_lig(), 3U);
		encoder.set_evex_wig(1);
		CHECK_EQ(encoder.evex_wig(), 1U);
		CHECK_EQ(encoder.evex_lig(), 3U);

		encoder.set_evex_wig(0xFFFF'FFFE);
		CHECK_EQ(encoder.evex_wig(), 0U);
		CHECK_EQ(encoder.evex_lig(), 3U);
		encoder.set_evex_wig(0xFFFF'FFFF);
		CHECK_EQ(encoder.evex_wig(), 1U);
		CHECK_EQ(encoder.evex_lig(), 3U);

		encoder.set_evex_wig(1);
		encoder.set_evex_lig(0);
		CHECK_EQ(encoder.evex_lig(), 0U);
		CHECK_EQ(encoder.evex_wig(), 1U);
		encoder.set_evex_lig(1);
		CHECK_EQ(encoder.evex_lig(), 1U);
		CHECK_EQ(encoder.evex_wig(), 1U);
		encoder.set_evex_lig(2);
		CHECK_EQ(encoder.evex_lig(), 2U);
		CHECK_EQ(encoder.evex_wig(), 1U);
		encoder.set_evex_lig(3);
		CHECK_EQ(encoder.evex_lig(), 3U);
		CHECK_EQ(encoder.evex_wig(), 1U);

		encoder.set_evex_lig(0xFFFF'FFFC);
		CHECK_EQ(encoder.evex_lig(), 0U);
		CHECK_EQ(encoder.evex_wig(), 1U);
		encoder.set_evex_lig(0xFFFF'FFFD);
		CHECK_EQ(encoder.evex_lig(), 1U);
		CHECK_EQ(encoder.evex_wig(), 1U);
		encoder.set_evex_lig(0xFFFF'FFFE);
		CHECK_EQ(encoder.evex_lig(), 2U);
		CHECK_EQ(encoder.evex_wig(), 1U);
		encoder.set_evex_lig(0xFFFF'FFFF);
		CHECK_EQ(encoder.evex_lig(), 3U);
		CHECK_EQ(encoder.evex_wig(), 1U);

		encoder.set_mvex_wig(0);
		CHECK_EQ(encoder.mvex_wig(), 0U);
		encoder.set_mvex_wig(1);
		CHECK_EQ(encoder.mvex_wig(), 1U);

		encoder.set_mvex_wig(0xFFFF'FFFE);
		CHECK_EQ(encoder.mvex_wig(), 0U);
		encoder.set_mvex_wig(0xFFFF'FFFF);
		CHECK_EQ(encoder.mvex_wig(), 1U);
	}
}

#if ICED_X86_TESTS_HAS_DECODER

namespace {
// (hex_bytes, expected_bytes, code, wig, lig)
using WigLigTest = std::tuple<const char*, const char*, Code, std::uint32_t, std::uint32_t>;

Instruction decode_one_64(const std::vector<std::uint8_t>& bytes) {
	constexpr std::uint32_t BITNESS = 64;
	auto decoder = create_decoder(BITNESS, bytes, get_default_ip(BITNESS), 0).decoder;
	return decoder.decode();
}
} // namespace

TEST_CASE("encoder/prevent_vex2_encoding") {
	const std::vector<std::tuple<const char*, const char*, Code, bool>> tests = {
		{"C5FC 10 10", "C4E17C 10 10", Code::VEX_Vmovups_ymm_ymmm256, true},
		{"C5FC 10 10", "C5FC 10 10", Code::VEX_Vmovups_ymm_ymmm256, false},
	};
	for (const auto& [hex_bytes, expected_hex_bytes, code, prevent_vex2] : tests) {
		const auto bytes = to_vec_u8(hex_bytes);
		const Instruction instr = decode_one_64(bytes);
		CHECK_EQ(instr.code(), code);
		Encoder encoder(64);
		encoder.set_prevent_vex2(prevent_vex2);
		(void)encoder.encode(instr, instr.ip()).value();
		const auto encoded_bytes = encoder.take_buffer();
		const auto expected_bytes = to_vec_u8(expected_hex_bytes);
		CHECK(encoded_bytes == expected_bytes);
	}
}

TEST_CASE("encoder/test_vex_wig_lig") {
	const std::vector<WigLigTest> tests = {
		{"C5CA 10 CD", "C5CA 10 CD", Code::VEX_Vmovss_xmm_xmm_xmm, 0, 0},
		{"C5CA 10 CD", "C5CE 10 CD", Code::VEX_Vmovss_xmm_xmm_xmm, 0, 1},
		{"C5CA 10 CD", "C5CA 10 CD", Code::VEX_Vmovss_xmm_xmm_xmm, 1, 0},
		{"C5CA 10 CD", "C5CE 10 CD", Code::VEX_Vmovss_xmm_xmm_xmm, 1, 1},

		{"C4414A 10 CD", "C4414A 10 CD", Code::VEX_Vmovss_xmm_xmm_xmm, 0, 0},
		{"C4414A 10 CD", "C4414E 10 CD", Code::VEX_Vmovss_xmm_xmm_xmm, 0, 1},
		{"C4414A 10 CD", "C441CA 10 CD", Code::VEX_Vmovss_xmm_xmm_xmm, 1, 0},
		{"C4414A 10 CD", "C441CE 10 CD", Code::VEX_Vmovss_xmm_xmm_xmm, 1, 1},

		{"C5F9 50 D3", "C5F9 50 D3", Code::VEX_Vmovmskpd_r32_xmm, 0, 0},
		{"C5F9 50 D3", "C5F9 50 D3", Code::VEX_Vmovmskpd_r32_xmm, 0, 1},
		{"C5F9 50 D3", "C5F9 50 D3", Code::VEX_Vmovmskpd_r32_xmm, 1, 0},
		{"C5F9 50 D3", "C5F9 50 D3", Code::VEX_Vmovmskpd_r32_xmm, 1, 1},

		{"C4C179 50 D3", "C4C179 50 D3", Code::VEX_Vmovmskpd_r32_xmm, 0, 0},
		{"C4C179 50 D3", "C4C179 50 D3", Code::VEX_Vmovmskpd_r32_xmm, 0, 1},
		{"C4C179 50 D3", "C4C179 50 D3", Code::VEX_Vmovmskpd_r32_xmm, 1, 0},
		{"C4C179 50 D3", "C4C179 50 D3", Code::VEX_Vmovmskpd_r32_xmm, 1, 1},
	};
	for (const auto& [hex_bytes, expected_hex_bytes, code, wig, lig] : tests) {
		const auto bytes = to_vec_u8(hex_bytes);
		const Instruction instr = decode_one_64(bytes);
		CHECK_EQ(instr.code(), code);
		Encoder encoder(64);
		encoder.set_vex_wig(wig);
		encoder.set_vex_lig(lig);
		(void)encoder.encode(instr, instr.ip()).value();
		const auto encoded_bytes = encoder.take_buffer();
		const auto expected_bytes = to_vec_u8(expected_hex_bytes);
		CHECK(encoded_bytes == expected_bytes);
	}
}

TEST_CASE("encoder/test_evex_wig_lig") {
	const std::vector<WigLigTest> tests = {
		{"62 F14E08 10 D3", "62 F14E08 10 D3", Code::EVEX_Vmovss_xmm_k1z_xmm_xmm, 0, 0},
		{"62 F14E08 10 D3", "62 F14E28 10 D3", Code::EVEX_Vmovss_xmm_k1z_xmm_xmm, 0, 1},
		{"62 F14E08 10 D3", "62 F14E48 10 D3", Code::EVEX_Vmovss_xmm_k1z_xmm_xmm, 0, 2},
		{"62 F14E08 10 D3", "62 F14E68 10 D3", Code::EVEX_Vmovss_xmm_k1z_xmm_xmm, 0, 3},

		{"62 F14E08 10 D3", "62 F14E08 10 D3", Code::EVEX_Vmovss_xmm_k1z_xmm_xmm, 1, 0},
		{"62 F14E08 10 D3", "62 F14E28 10 D3", Code::EVEX_Vmovss_xmm_k1z_xmm_xmm, 1, 1},
		{"62 F14E08 10 D3", "62 F14E48 10 D3", Code::EVEX_Vmovss_xmm_k1z_xmm_xmm, 1, 2},
		{"62 F14E08 10 D3", "62 F14E68 10 D3", Code::EVEX_Vmovss_xmm_k1z_xmm_xmm, 1, 3},

		{"62 F14D0B 60 50 01", "62 F14D0B 60 50 01", Code::EVEX_Vpunpcklbw_xmm_k1z_xmm_xmmm128, 0, 0},
		{"62 F14D0B 60 50 01", "62 F14D0B 60 50 01", Code::EVEX_Vpunpcklbw_xmm_k1z_xmm_xmmm128, 0, 1},
		{"62 F14D0B 60 50 01", "62 F14D0B 60 50 01", Code::EVEX_Vpunpcklbw_xmm_k1z_xmm_xmmm128, 0, 2},
		{"62 F14D0B 60 50 01", "62 F14D0B 60 50 01", Code::EVEX_Vpunpcklbw_xmm_k1z_xmm_xmmm128, 0, 3},

		{"62 F14D0B 60 50 01", "62 F1CD0B 60 50 01", Code::EVEX_Vpunpcklbw_xmm_k1z_xmm_xmmm128, 1, 0},
		{"62 F14D0B 60 50 01", "62 F1CD0B 60 50 01", Code::EVEX_Vpunpcklbw_xmm_k1z_xmm_xmmm128, 1, 1},
		{"62 F14D0B 60 50 01", "62 F1CD0B 60 50 01", Code::EVEX_Vpunpcklbw_xmm_k1z_xmm_xmmm128, 1, 2},
		{"62 F14D0B 60 50 01", "62 F1CD0B 60 50 01", Code::EVEX_Vpunpcklbw_xmm_k1z_xmm_xmmm128, 1, 3},

		{"62 F17C0B 51 50 01", "62 F17C0B 51 50 01", Code::EVEX_Vsqrtps_xmm_k1z_xmmm128b32, 0, 0},
		{"62 F17C0B 51 50 01", "62 F17C0B 51 50 01", Code::EVEX_Vsqrtps_xmm_k1z_xmmm128b32, 0, 1},
		{"62 F17C0B 51 50 01", "62 F17C0B 51 50 01", Code::EVEX_Vsqrtps_xmm_k1z_xmmm128b32, 0, 2},
		{"62 F17C0B 51 50 01", "62 F17C0B 51 50 01", Code::EVEX_Vsqrtps_xmm_k1z_xmmm128b32, 0, 3},

		{"62 F17C0B 51 50 01", "62 F17C0B 51 50 01", Code::EVEX_Vsqrtps_xmm_k1z_xmmm128b32, 1, 0},
		{"62 F17C0B 51 50 01", "62 F17C0B 51 50 01", Code::EVEX_Vsqrtps_xmm_k1z_xmmm128b32, 1, 1},
		{"62 F17C0B 51 50 01", "62 F17C0B 51 50 01", Code::EVEX_Vsqrtps_xmm_k1z_xmmm128b32, 1, 2},
		{"62 F17C0B 51 50 01", "62 F17C0B 51 50 01", Code::EVEX_Vsqrtps_xmm_k1z_xmmm128b32, 1, 3},
	};
	for (const auto& [hex_bytes, expected_hex_bytes, code, wig, lig] : tests) {
		const auto bytes = to_vec_u8(hex_bytes);
		const Instruction instr = decode_one_64(bytes);
		CHECK_EQ(instr.code(), code);
		Encoder encoder(64);
		encoder.set_evex_wig(wig);
		encoder.set_evex_lig(lig);
		(void)encoder.encode(instr, instr.ip()).value();
		const auto encoded_bytes = encoder.take_buffer();
		const auto expected_bytes = to_vec_u8(expected_hex_bytes);
		CHECK(encoded_bytes == expected_bytes);
	}
}

#endif

TEST_CASE("encoder/verify_memory_operand_ctors") {
	{
		const auto op = MemoryOperand::new_(Register::RCX, Register::RSI, 4, -0x1234'5678'9ABC'DEF1, 8, true, Register::FS);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::RSI);
		CHECK_EQ(op.scale, 4U);
		CHECK_EQ(op.displacement, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 8U);
		CHECK(op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::FS);
	}
	{
		const MemoryOperand op(Register::RCX, Register::RSI, 4, -0x1234'5678'9ABC'DEF1, 8, true, Register::FS);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::RSI);
		CHECK_EQ(op.scale, 4U);
		CHECK_EQ(op.displacement, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 8U);
		CHECK(op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::FS);
	}
	{
		const auto op = MemoryOperand::with_base_index_scale_bcst_seg(Register::RCX, Register::RSI, 4, true, Register::FS);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::RSI);
		CHECK_EQ(op.scale, 4U);
		CHECK_EQ(op.displacement, 0);
		CHECK_EQ(op.displ_size, 0U);
		CHECK(op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::FS);
	}
	{
		const auto op = MemoryOperand::with_base_displ_size_bcst_seg(Register::RCX, -0x1234'5678'9ABC'DEF1, 8, true, Register::FS);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::None);
		CHECK_EQ(op.scale, 1U);
		CHECK_EQ(op.displacement, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 8U);
		CHECK(op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::FS);
	}
	{
		const auto op = MemoryOperand::with_index_scale_displ_size_bcst_seg(Register::RSI, 4, -0x1234'5678'9ABC'DEF1, 8, true, Register::FS);
		CHECK_EQ(op.base, Register::None);
		CHECK_EQ(op.index, Register::RSI);
		CHECK_EQ(op.scale, 4U);
		CHECK_EQ(op.displacement, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 8U);
		CHECK(op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::FS);
	}
	{
		const auto op = MemoryOperand::with_base_displ_bcst_seg(Register::RCX, -0x1234'5678'9ABC'DEF1, true, Register::FS);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::None);
		CHECK_EQ(op.scale, 1U);
		CHECK_EQ(op.displacement, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 1U);
		CHECK(op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::FS);
	}
	{
		const auto op = MemoryOperand::with_base_index_scale_displ_size(Register::RCX, Register::RSI, 4, -0x1234'5678'9ABC'DEF1, 8);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::RSI);
		CHECK_EQ(op.scale, 4U);
		CHECK_EQ(op.displacement, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 8U);
		CHECK(!op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::None);
	}
	{
		const auto op = MemoryOperand::with_base_index_scale(Register::RCX, Register::RSI, 4);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::RSI);
		CHECK_EQ(op.scale, 4U);
		CHECK_EQ(op.displacement, 0);
		CHECK_EQ(op.displ_size, 0U);
		CHECK(!op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::None);
	}
	{
		const auto op = MemoryOperand::with_base_index(Register::RCX, Register::RSI);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::RSI);
		CHECK_EQ(op.scale, 1U);
		CHECK_EQ(op.displacement, 0);
		CHECK_EQ(op.displ_size, 0U);
		CHECK(!op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::None);
	}
	{
		const auto op = MemoryOperand::with_base_displ_size(Register::RCX, -0x1234'5678'9ABC'DEF1, 8);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::None);
		CHECK_EQ(op.scale, 1U);
		CHECK_EQ(op.displacement, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 8U);
		CHECK(!op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::None);
	}
	{
		const auto op = MemoryOperand::with_index_scale_displ_size(Register::RSI, 4, -0x1234'5678'9ABC'DEF1, 8);
		CHECK_EQ(op.base, Register::None);
		CHECK_EQ(op.index, Register::RSI);
		CHECK_EQ(op.scale, 4U);
		CHECK_EQ(op.displacement, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 8U);
		CHECK(!op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::None);
	}
	{
		const auto op = MemoryOperand::with_base_displ(Register::RCX, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::None);
		CHECK_EQ(op.scale, 1U);
		CHECK_EQ(op.displacement, -0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 1U);
		CHECK(!op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::None);
	}
	{
		const auto op = MemoryOperand::with_base(Register::RCX);
		CHECK_EQ(op.base, Register::RCX);
		CHECK_EQ(op.index, Register::None);
		CHECK_EQ(op.scale, 1U);
		CHECK_EQ(op.displacement, 0);
		CHECK_EQ(op.displ_size, 0U);
		CHECK(!op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::None);
	}
	{
		const auto op = MemoryOperand::with_displ(0x1234'5678'9ABC'DEF1, 8);
		CHECK_EQ(op.base, Register::None);
		CHECK_EQ(op.index, Register::None);
		CHECK_EQ(op.scale, 1U);
		CHECK_EQ(op.displacement, 0x1234'5678'9ABC'DEF1);
		CHECK_EQ(op.displ_size, 8U);
		CHECK(!op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::None);
	}
	{
		// Rust: MemoryOperand::default()
		const MemoryOperand op;
		CHECK_EQ(op.base, Register::None);
		CHECK_EQ(op.index, Register::None);
		CHECK_EQ(op.scale, 0U);
		CHECK_EQ(op.displacement, 0);
		CHECK_EQ(op.displ_size, 0U);
		CHECK(!op.is_broadcast);
		CHECK_EQ(op.segment_prefix, Register::None);
	}
}

namespace {

const std::vector<OpCodeInfoTestCase>& get_op_code_info_test_cases() {
	static const std::vector<OpCodeInfoTestCase> test_cases = read_op_code_info_test_cases(get_encoder_unit_tests_dir() + "/OpCodeInfos.txt");
	return test_cases;
}

void test_op_code_info(const OpCodeInfoTestCase& tc) {
	const OpCodeInfo& info = code_ext::op_code(tc.code);
	CHECK_EQ(info.code(), tc.code);
	CHECK_EQ(std::string(info.op_code_string()), tc.op_code_string);
	CHECK_EQ(std::string(info.instruction_string()), tc.instruction_string);
	CHECK_EQ(to_string(info), tc.instruction_string);
	CHECK_EQ(info.mnemonic(), tc.mnemonic);
	CHECK_EQ(info.encoding(), tc.encoding);
	CHECK_EQ(info.is_instruction(), tc.is_instruction);
	CHECK_EQ(info.mode16(), tc.mode16);
	CHECK_EQ(info.is_available_in_mode(16), tc.mode16);
	CHECK_EQ(info.mode32(), tc.mode32);
	CHECK_EQ(info.is_available_in_mode(32), tc.mode32);
	CHECK_EQ(info.mode64(), tc.mode64);
	CHECK_EQ(info.is_available_in_mode(64), tc.mode64);
	CHECK_EQ(info.fwait(), tc.fwait);
	CHECK_EQ(info.operand_size(), tc.operand_size);
	CHECK_EQ(info.address_size(), tc.address_size);
	CHECK_EQ(info.l(), tc.l);
	CHECK_EQ(info.w(), tc.w);
	CHECK_EQ(info.is_lig(), tc.is_lig);
	CHECK_EQ(info.is_wig(), tc.is_wig);
	CHECK_EQ(info.is_wig32(), tc.is_wig32);
	CHECK_EQ(info.tuple_type(), tc.tuple_type);
	CHECK_EQ(info.memory_size(), tc.memory_size);
	CHECK_EQ(info.broadcast_memory_size(), tc.broadcast_memory_size);
	CHECK_EQ(info.decoder_option(), tc.decoder_option);
	CHECK_EQ(info.can_broadcast(), tc.can_broadcast);
	CHECK_EQ(info.can_use_rounding_control(), tc.can_use_rounding_control);
	CHECK_EQ(info.can_suppress_all_exceptions(), tc.can_suppress_all_exceptions);
	CHECK_EQ(info.can_use_op_mask_register(), tc.can_use_op_mask_register);
	CHECK_EQ(info.require_op_mask_register(), tc.require_op_mask_register);
	if (tc.require_op_mask_register) {
		CHECK(info.can_use_op_mask_register());
		CHECK(!info.can_use_zeroing_masking());
	}
	CHECK_EQ(info.can_use_zeroing_masking(), tc.can_use_zeroing_masking);
	CHECK_EQ(info.can_use_lock_prefix(), tc.can_use_lock_prefix);
	CHECK_EQ(info.can_use_xacquire_prefix(), tc.can_use_xacquire_prefix);
	CHECK_EQ(info.can_use_xrelease_prefix(), tc.can_use_xrelease_prefix);
	CHECK_EQ(info.can_use_rep_prefix(), tc.can_use_rep_prefix);
	CHECK_EQ(info.can_use_repne_prefix(), tc.can_use_repne_prefix);
	CHECK_EQ(info.can_use_bnd_prefix(), tc.can_use_bnd_prefix);
	CHECK_EQ(info.can_use_hint_taken_prefix(), tc.can_use_hint_taken_prefix);
	CHECK_EQ(info.can_use_notrack_prefix(), tc.can_use_notrack_prefix);
	CHECK_EQ(info.ignores_rounding_control(), tc.ignores_rounding_control);
	CHECK_EQ(info.amd_lock_reg_bit(), tc.amd_lock_reg_bit);
	CHECK_EQ(info.default_op_size64(), tc.default_op_size64);
	CHECK_EQ(info.force_op_size64(), tc.force_op_size64);
	CHECK_EQ(info.intel_force_op_size64(), tc.intel_force_op_size64);
	CHECK_EQ(info.must_be_cpl0(), tc.cpl0 && !tc.cpl1 && !tc.cpl2 && !tc.cpl3);
	CHECK_EQ(info.cpl0(), tc.cpl0);
	CHECK_EQ(info.cpl1(), tc.cpl1);
	CHECK_EQ(info.cpl2(), tc.cpl2);
	CHECK_EQ(info.cpl3(), tc.cpl3);
	CHECK_EQ(info.is_input_output(), tc.is_input_output);
	CHECK_EQ(info.is_nop(), tc.is_nop);
	CHECK_EQ(info.is_reserved_nop(), tc.is_reserved_nop);
	CHECK_EQ(info.is_serializing_intel(), tc.is_serializing_intel);
	CHECK_EQ(info.is_serializing_amd(), tc.is_serializing_amd);
	CHECK_EQ(info.may_require_cpl0(), tc.may_require_cpl0);
	CHECK_EQ(info.is_cet_tracked(), tc.is_cet_tracked);
	CHECK_EQ(info.is_non_temporal(), tc.is_non_temporal);
	CHECK_EQ(info.is_fpu_no_wait(), tc.is_fpu_no_wait);
	CHECK_EQ(info.ignores_mod_bits(), tc.ignores_mod_bits);
	CHECK_EQ(info.no66(), tc.no66);
	CHECK_EQ(info.nfx(), tc.nfx);
	CHECK_EQ(info.requires_unique_reg_nums(), tc.requires_unique_reg_nums);
	CHECK_EQ(info.requires_unique_dest_reg_num(), tc.requires_unique_dest_reg_num);
	CHECK_EQ(info.is_privileged(), tc.is_privileged);
	CHECK_EQ(info.is_save_restore(), tc.is_save_restore);
	CHECK_EQ(info.is_stack_instruction(), tc.is_stack_instruction);
	CHECK_EQ(info.ignores_segment(), tc.ignores_segment);
	CHECK_EQ(info.is_op_mask_read_write(), tc.is_op_mask_read_write);
	CHECK_EQ(info.real_mode(), tc.real_mode);
	CHECK_EQ(info.protected_mode(), tc.protected_mode);
	CHECK_EQ(info.virtual8086_mode(), tc.virtual8086_mode);
	CHECK_EQ(info.compatibility_mode(), tc.compatibility_mode);
	CHECK_EQ(info.long_mode(), tc.long_mode);
	CHECK_EQ(info.use_outside_smm(), tc.use_outside_smm);
	CHECK_EQ(info.use_in_smm(), tc.use_in_smm);
	CHECK_EQ(info.use_outside_enclave_sgx(), tc.use_outside_enclave_sgx);
	CHECK_EQ(info.use_in_enclave_sgx1(), tc.use_in_enclave_sgx1);
	CHECK_EQ(info.use_in_enclave_sgx2(), tc.use_in_enclave_sgx2);
	CHECK_EQ(info.use_outside_vmx_op(), tc.use_outside_vmx_op);
	CHECK_EQ(info.use_in_vmx_root_op(), tc.use_in_vmx_root_op);
	CHECK_EQ(info.use_in_vmx_non_root_op(), tc.use_in_vmx_non_root_op);
	CHECK_EQ(info.use_outside_seam(), tc.use_outside_seam);
	CHECK_EQ(info.use_in_seam(), tc.use_in_seam);
	CHECK_EQ(info.tdx_non_root_gen_ud(), tc.tdx_non_root_gen_ud);
	CHECK_EQ(info.tdx_non_root_gen_ve(), tc.tdx_non_root_gen_ve);
	CHECK_EQ(info.tdx_non_root_may_gen_ex(), tc.tdx_non_root_may_gen_ex);
	CHECK_EQ(info.intel_vm_exit(), tc.intel_vm_exit);
	CHECK_EQ(info.intel_may_vm_exit(), tc.intel_may_vm_exit);
	CHECK_EQ(info.intel_smm_vm_exit(), tc.intel_smm_vm_exit);
	CHECK_EQ(info.amd_vm_exit(), tc.amd_vm_exit);
	CHECK_EQ(info.amd_may_vm_exit(), tc.amd_may_vm_exit);
	CHECK_EQ(info.tsx_abort(), tc.tsx_abort);
	CHECK_EQ(info.tsx_impl_abort(), tc.tsx_impl_abort);
	CHECK_EQ(info.tsx_may_abort(), tc.tsx_may_abort);
	CHECK_EQ(info.intel_decoder16(), tc.intel_decoder16);
	CHECK_EQ(info.intel_decoder32(), tc.intel_decoder32);
	CHECK_EQ(info.intel_decoder64(), tc.intel_decoder64);
	CHECK_EQ(info.amd_decoder16(), tc.amd_decoder16);
	CHECK_EQ(info.amd_decoder32(), tc.amd_decoder32);
	CHECK_EQ(info.amd_decoder64(), tc.amd_decoder64);
	CHECK_EQ(info.table(), tc.table);
	CHECK_EQ(info.mandatory_prefix(), tc.mandatory_prefix);
	CHECK_EQ(info.op_code(), tc.op_code);
	CHECK_EQ(info.op_code_len(), tc.op_code_len);
	CHECK_EQ(info.is_group(), tc.is_group);
	CHECK_EQ(info.group_index(), tc.group_index);
	CHECK_EQ(info.is_rm_group(), tc.is_rm_group);
	CHECK_EQ(info.rm_group_index(), tc.rm_group_index);
	CHECK_EQ(info.op_count(), tc.op_count);
	CHECK_EQ(info.op0_kind(), tc.op_kinds[0]);
	CHECK_EQ(info.op1_kind(), tc.op_kinds[1]);
	CHECK_EQ(info.op2_kind(), tc.op_kinds[2]);
	CHECK_EQ(info.op3_kind(), tc.op_kinds[3]);
	CHECK_EQ(info.op4_kind(), tc.op_kinds[4]);
	CHECK_EQ(info.op_kind(0), tc.op_kinds[0]);
	CHECK_EQ(info.op_kind(1), tc.op_kinds[1]);
	CHECK_EQ(info.op_kind(2), tc.op_kinds[2]);
	CHECK_EQ(info.op_kind(3), tc.op_kinds[3]);
	CHECK_EQ(info.op_kind(4), tc.op_kinds[4]);
	CHECK_EQ(info.try_op_kind(0).value(), tc.op_kinds[0]);
	CHECK_EQ(info.try_op_kind(1).value(), tc.op_kinds[1]);
	CHECK_EQ(info.try_op_kind(2).value(), tc.op_kinds[2]);
	CHECK_EQ(info.try_op_kind(3).value(), tc.op_kinds[3]);
	CHECK_EQ(info.try_op_kind(4).value(), tc.op_kinds[4]);
	const auto op_kinds = info.op_kinds();
	CHECK_EQ(op_kinds.size(), static_cast<std::size_t>(tc.op_count));
	for (std::size_t i = 0; i < op_kinds.size(); i++) {
		CHECK_EQ(op_kinds[i], info.op_kind(static_cast<std::uint32_t>(i)));
		CHECK_EQ(op_kinds[i], info.try_op_kind(static_cast<std::uint32_t>(i)).value());
	}
	static_assert(IcedConstants::MAX_OP_COUNT == 5, "");
	for (std::uint32_t i = tc.op_count; i < IcedConstants::MAX_OP_COUNT; i++) {
		CHECK_EQ(info.op_kind(i), OpCodeOperandKind::None);
		CHECK_EQ(info.try_op_kind(i).value(), OpCodeOperandKind::None);
	}
	CHECK_EQ(info.mvex_eh_bit(), tc.mvex.eh_bit);
	CHECK_EQ(info.mvex_can_use_eviction_hint(), tc.mvex.can_use_eviction_hint);
	CHECK_EQ(info.mvex_can_use_imm_rounding_control(), tc.mvex.can_use_imm_rounding_control);
	CHECK_EQ(info.mvex_ignores_op_mask_register(), tc.mvex.ignores_op_mask_register);
	CHECK_EQ(info.mvex_no_sae_rc(), tc.mvex.no_sae_rc);
	CHECK_EQ(info.mvex_tuple_type_lut_kind(), tc.mvex.tuple_type_lut_kind);
	CHECK_EQ(info.mvex_conversion_func(), tc.mvex.conversion_func);
	CHECK_EQ(info.mvex_valid_conversion_funcs_mask(), tc.mvex.valid_conversion_funcs_mask);
	CHECK_EQ(info.mvex_valid_swizzle_funcs_mask(), tc.mvex.valid_swizzle_funcs_mask);
}

} // namespace

TEST_CASE("encoder/test_all_op_code_infos") {
	REQUIRE(!get_op_code_info_test_cases().empty());
	for (const auto& tc : get_op_code_info_test_cases())
		test_op_code_info(tc);
}

TEST_CASE("encoder/op_kind_panics_if_invalid_input") {
	const OpCodeInfo& op_code = code_ext::op_code(Code::Aaa);
#ifndef NDEBUG
#if ICED_X86_TESTS_CAN_CHECK_ABORT
	CHECK(aborts([&op_code] { (void)op_code.op_kind(IcedConstants::MAX_OP_COUNT); }));
#endif
#else
	(void)op_code.op_kind(IcedConstants::MAX_OP_COUNT);
#endif
}

TEST_CASE("encoder/op_kind_fails_if_invalid_input") {
	const OpCodeInfo& op_code = code_ext::op_code(Code::Aaa);
	CHECK(op_code.try_op_kind(IcedConstants::MAX_OP_COUNT).is_err());
}

TEST_CASE("encoder/verify_instruction_op_code_info") {
	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const Code code = static_cast<Code>(i);
		Instruction instr;
		instr.set_code(code);
		CHECK(&code_ext::op_code(code) == &instr.op_code());
	}
}

TEST_CASE("encoder/make_sure_all_code_values_are_tested_exactly_once") {
	std::vector<bool> tested(IcedConstants::CODE_ENUM_COUNT, false);
	for (const auto& tc : get_op_code_info_test_cases()) {
		CHECK(!tested[static_cast<std::size_t>(tc.code)]);
		tested[static_cast<std::size_t>(tc.code)] = true;
	}
	std::string s;
	const auto names = code_names();
	for (std::size_t i = 0; i < tested.size(); i++) {
		if (!tested[i] && !is_ignored_code(names[i])) {
			if (!s.empty())
				s.push_back(',');
			s.append(names[i]);
		}
	}
	CHECK_EQ(s, std::string());
}

TEST_CASE("encoder/op_code_info_is_available_in_mode_fails_if_invalid_bitness_0") {
	CHECK(!code_ext::op_code(Code::Nopd).is_available_in_mode(0));
}

TEST_CASE("encoder/op_code_info_is_available_in_mode_panics_if_invalid_bitness_128") {
	CHECK(!code_ext::op_code(Code::Nopd).is_available_in_mode(128));
}

TEST_CASE("encoder/write_byte_works") {
	Encoder encoder(64);
	const Instruction instr = Instruction::with2(Code::Add_r64_rm64, Register::R8, Register::RBP).value();
	encoder.write_u8(0x90);
	CHECK_EQ(encoder.encode(instr, 0x5555'5555).value(), static_cast<std::size_t>(3));
	encoder.write_u8(0xCC);
	CHECK((encoder.take_buffer() == std::vector<std::uint8_t>{0x90, 0x4C, 0x03, 0xC5, 0xCC}));
}

// ENCODER_TESTS_PART2

} // namespace iced_x86::tests
