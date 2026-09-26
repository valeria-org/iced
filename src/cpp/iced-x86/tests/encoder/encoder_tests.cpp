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

} // namespace

TEST_CASE("encoder/encode_16") { encode(16); }

TEST_CASE("encoder/encode_32") { encode(32); }

TEST_CASE("encoder/encode_64") { encode(64); }

TEST_CASE("encoder/non_decode_encode_16") { non_decode_encode(16); }

TEST_CASE("encoder/non_decode_encode_32") { non_decode_encode(32); }

TEST_CASE("encoder/non_decode_encode_64") { non_decode_encode(64); }

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

TEST_CASE("encoder/invalid_displ_16") {
	constexpr std::uint32_t BITNESS = 16;

	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x0'0000, 2)));
	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x0'FFFF, 2)));
	ENCODE_ERR(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x1'0000, 2)));
	ENCODE_ERR(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0xFFFF'FFFF'FFFF'FFFF, 2)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_displ(0x0'0000, 2)));
	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_displ(0x0'FFFF, 2)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_displ(0x1'0000, 2)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_displ(0xFFFF'FFFF'FFFF'FFFF, 2)));

	for (std::uint32_t displ_size : {1U, 2U}) {
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, 0, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, -1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, 1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, -0x0'8000, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, 0x0'FFFF, displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, -0x0'8001, displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, 0x1'0000, displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, (-INT64_C(0x8000'0000'0000'0000)), displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, 0x7FFF'FFFF'FFFF'FFFF, displ_size)));
	}

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BP, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BP, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BP, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBP, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBP, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBP, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, -1, 0)));

	for (std::uint32_t displ_size : {1U, 4U}) {
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, -1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, (-INT64_C(0x8000'0000)), displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0xFFFF'FFFF, displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, (-INT64_C(0x8000'0001)), displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0x1'0000'0000, displ_size)));
	}
}

TEST_CASE("encoder/invalid_displ_32") {
	constexpr std::uint32_t BITNESS = 32;

	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x0'0000, 2)));
	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x0'FFFF, 2)));
	ENCODE_ERR(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x1'0000, 2)));
	ENCODE_ERR(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0xFFFF'FFFF'FFFF'FFFF, 2)));

	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x0'0000'0000, 4)));
	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x0'FFFF'FFFF, 4)));
	ENCODE_ERR(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x1'0000'0000, 4)));
	ENCODE_ERR(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0xFFFF'FFFF'FFFF'FFFF, 4)));

	for (std::uint32_t displ_size : {1U, 4U}) {
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, 0, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, 1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, -1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, (-INT64_C(0x8000'0000)), displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, 0xFFFF'FFFF, displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, (-INT64_C(0x8000'0001)), displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, 0x1'0000'0000, displ_size)));
	}

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BP, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BP, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BP, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::BX, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBP, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBP, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBP, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, -1, 0)));

	for (std::uint32_t displ_size : {1U, 4U}) {
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, -1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, (-INT64_C(0x8000'0000)), displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0xFFFF'FFFF, displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, (-INT64_C(0x8000'0001)), displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0x1'0000'0000, displ_size)));
	}
}

TEST_CASE("encoder/invalid_displ_64") {
	constexpr std::uint32_t BITNESS = 64;

	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x0'0000'0000, 4)));
	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x0'FFFF'FFFF, 4)));
	ENCODE_ERR(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x1'0000'0000, 4)));
	ENCODE_ERR(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0xFFFF'FFFF'FFFF'FFFF, 4)));

	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0x0000'0000'0000'0000, 8)));
	ENCODE_OK(BITNESS, Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(0xFFFF'FFFF'FFFF'FFFF, 8)));

	for (std::uint32_t displ_size : {1U, 8U}) {
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, 0, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, 1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, -1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, (-INT64_C(0x8000'0000)), displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, 0x7FFF'FFFF, displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, (-INT64_C(0x8000'0001)), displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::None, 0x8000'0000, displ_size)));
	}

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBP, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBP, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBP, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::R13D, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::R13D, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::R13D, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBP, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBP, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBP, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::R13, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::R13, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::R13, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, -1, 0)));

	ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, 0, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, 1, 0)));
	ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, -1, 0)));

	for (std::uint32_t displ_size : {1U, 4U}) {
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, -1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, (-INT64_C(0x8000'0000)), displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0xFFFF'FFFF, displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, (-INT64_C(0x8000'0001)), displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::EBX, 0x1'0000'0000, displ_size)));
	}

	for (std::uint32_t displ_size : {1U, 8U}) {
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, 0, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, 1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, -1, displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, (-INT64_C(0x8000'0000)), displ_size)));
		ENCODE_OK(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, 0x7FFF'FFFF, displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, (-INT64_C(0x8000'0001)), displ_size)));
		ENCODE_ERR(BITNESS, Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ_size(Register::RBX, 0x8000'0000, displ_size)));
	}
}

TEST_CASE("encoder/test_unsupported_bitness") {
	{
		Encoder encoder(16);
		CHECK(encoder.encode(Instruction::with2(Code::Mov_r64_rm64, Register::RAX, Register::RCX).value(), 0).is_err());
	}
	{
		Encoder encoder(32);
		CHECK(encoder.encode(Instruction::with2(Code::Mov_r64_rm64, Register::RAX, Register::RCX).value(), 0).is_err());
	}
	{
		Encoder encoder(64);
		CHECK(encoder.encode(Instruction::with(Code::Pushad), 0).is_err());
	}
}

TEST_CASE("encoder/test_too_long_instruction") {
	Encoder encoder(16);
	Instruction instr = Instruction::with2(Code::Add_rm32_imm32, MemoryOperand(Register::ESP, Register::None, 1, 0x1234'5678, 4, false, Register::SS),
		0x1234'5678)
							.value();
	instr.set_has_xacquire_prefix(true);
	instr.set_has_lock_prefix(true);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_wrong_op_kind") {
	Encoder encoder(64);
	Instruction instr = Instruction::with1(Code::Push_r64, Register::RAX).value();
	instr.set_op0_kind(OpKind::Immediate16);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_wrong_implied_register") {
	Encoder encoder(64);
	const Instruction instr = Instruction::with2(Code::In_AL_DX, Register::RAX, Register::EDX).value();
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_wrong_register") {
	Encoder encoder(64);
	const Instruction instr = Instruction::with1(Code::Push_r64, Register::EAX).value();
	CHECK(encoder.encode(instr, 0).is_err());
}

namespace {
using InstrOpKindTest = std::tuple<std::uint32_t, Instruction, OpKind>;
using InstrOpKind2Test = std::tuple<std::uint32_t, Instruction, OpKind, OpKind>;

// Sets op_index's op kind to FarBranch16 and bad_op_kind and verifies that encoding fails
void test_invalid_op_kinds(const std::vector<InstrOpKindTest>& tests, std::uint32_t op_index) {
	for (const auto& [bitness, orig_instr, bad_op_kind] : tests) {
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_op_kind(op_index, OpKind::FarBranch16);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_op_kind(op_index, bad_op_kind);
			CHECK(encoder.encode(instr, 0).is_err());
		}
	}
}

void test_invalid_op_kinds2(const std::vector<InstrOpKind2Test>& tests) {
	for (const auto& [bitness, orig_instr, bad_op_kind1, bad_op_kind0] : tests) {
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_op0_kind(OpKind::FarBranch16);
			instr.set_op1_kind(OpKind::FarBranch16);
			CHECK(encoder.encode(instr, 0).is_err());
		}

		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_op0_kind(bad_op_kind1);
			instr.set_op1_kind(bad_op_kind1);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_op1_kind(bad_op_kind1);
			CHECK(encoder.encode(instr, 0).is_err());
		}

		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_op0_kind(bad_op_kind0);
			instr.set_op1_kind(bad_op_kind0);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_op0_kind(bad_op_kind0);
			CHECK(encoder.encode(instr, 0).is_err());
		}
	}
}
} // namespace

TEST_CASE("encoder/test_invalid_maskmov") {
	const std::vector<InstrOpKindTest> tests = {
		{16, Instruction::with_maskmovq(16, Register::MM0, Register::MM1, Register::None).value(), OpKind::MemorySegRDI},
		{16, Instruction::with_maskmovdqu(16, Register::XMM0, Register::XMM1, Register::None).value(), OpKind::MemorySegRDI},
		{16, Instruction::with_vmaskmovdqu(16, Register::XMM0, Register::XMM1, Register::None).value(), OpKind::MemorySegRDI},
		{32, Instruction::with_maskmovq(32, Register::MM0, Register::MM1, Register::None).value(), OpKind::MemorySegRDI},
		{32, Instruction::with_maskmovdqu(32, Register::XMM0, Register::XMM1, Register::None).value(), OpKind::MemorySegRDI},
		{32, Instruction::with_vmaskmovdqu(32, Register::XMM0, Register::XMM1, Register::None).value(), OpKind::MemorySegRDI},
		{64, Instruction::with_maskmovq(64, Register::MM0, Register::MM1, Register::None).value(), OpKind::MemorySegDI},
		{64, Instruction::with_maskmovdqu(64, Register::XMM0, Register::XMM1, Register::None).value(), OpKind::MemorySegDI},
		{64, Instruction::with_vmaskmovdqu(64, Register::XMM0, Register::XMM1, Register::None).value(), OpKind::MemorySegDI},
	};
	test_invalid_op_kinds(tests, 0);
}

TEST_CASE("encoder/test_invalid_outs") {
	const std::vector<InstrOpKindTest> tests = {
		{16, Instruction::with_outsb(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{16, Instruction::with_outsw(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{16, Instruction::with_outsd(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{32, Instruction::with_outsb(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{32, Instruction::with_outsw(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{32, Instruction::with_outsd(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{64, Instruction::with_outsb(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI},
		{64, Instruction::with_outsw(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI},
		{64, Instruction::with_outsd(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI},
	};
	test_invalid_op_kinds(tests, 1);
}

TEST_CASE("encoder/test_invalid_movs") {
	const std::vector<InstrOpKind2Test> tests = {
		{16, Instruction::with_movsb(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{16, Instruction::with_movsw(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{16, Instruction::with_movsd(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{16, Instruction::with_movsq(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{32, Instruction::with_movsb(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{32, Instruction::with_movsw(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{32, Instruction::with_movsd(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{32, Instruction::with_movsq(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{64, Instruction::with_movsb(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI, OpKind::MemoryESDI},
		{64, Instruction::with_movsw(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI, OpKind::MemoryESDI},
		{64, Instruction::with_movsd(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI, OpKind::MemoryESDI},
		{64, Instruction::with_movsq(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI, OpKind::MemoryESDI},
	};
	test_invalid_op_kinds2(tests);
}

TEST_CASE("encoder/test_invalid_cmps") {
	const std::vector<InstrOpKind2Test> tests = {
		{16, Instruction::with_cmpsb(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{16, Instruction::with_cmpsw(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{16, Instruction::with_cmpsd(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{16, Instruction::with_cmpsq(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{32, Instruction::with_cmpsb(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{32, Instruction::with_cmpsw(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{32, Instruction::with_cmpsd(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{32, Instruction::with_cmpsq(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI, OpKind::MemoryESRDI},
		{64, Instruction::with_cmpsb(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI, OpKind::MemoryESDI},
		{64, Instruction::with_cmpsw(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI, OpKind::MemoryESDI},
		{64, Instruction::with_cmpsd(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI, OpKind::MemoryESDI},
		{64, Instruction::with_cmpsq(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI, OpKind::MemoryESDI},
	};
	test_invalid_op_kinds2(tests);
}

TEST_CASE("encoder/test_invalid_lods") {
	const std::vector<InstrOpKindTest> tests = {
		{16, Instruction::with_lodsb(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{16, Instruction::with_lodsw(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{16, Instruction::with_lodsd(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{16, Instruction::with_lodsq(16, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{32, Instruction::with_lodsb(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{32, Instruction::with_lodsw(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{32, Instruction::with_lodsd(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{32, Instruction::with_lodsq(32, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegRSI},
		{64, Instruction::with_lodsb(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI},
		{64, Instruction::with_lodsw(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI},
		{64, Instruction::with_lodsd(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI},
		{64, Instruction::with_lodsq(64, Register::None, RepPrefixKind::None).value(), OpKind::MemorySegSI},
	};
	test_invalid_op_kinds(tests, 1);
}

TEST_CASE("encoder/test_invalid_ins") {
	const std::vector<InstrOpKindTest> tests = {
		{16, Instruction::with_insb(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{16, Instruction::with_insw(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{16, Instruction::with_insd(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_insb(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_insw(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_insd(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{64, Instruction::with_insb(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
		{64, Instruction::with_insw(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
		{64, Instruction::with_insd(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
	};
	test_invalid_op_kinds(tests, 0);
}

TEST_CASE("encoder/test_invalid_stos") {
	const std::vector<InstrOpKindTest> tests = {
		{16, Instruction::with_stosb(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{16, Instruction::with_stosw(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{16, Instruction::with_stosd(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{16, Instruction::with_stosq(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_stosb(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_stosw(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_stosd(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_stosq(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{64, Instruction::with_stosb(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
		{64, Instruction::with_stosw(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
		{64, Instruction::with_stosd(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
		{64, Instruction::with_stosq(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
	};
	test_invalid_op_kinds(tests, 0);
}

TEST_CASE("encoder/test_invalid_scas") {
	const std::vector<InstrOpKindTest> tests = {
		{16, Instruction::with_scasb(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{16, Instruction::with_scasw(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{16, Instruction::with_scasd(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{16, Instruction::with_scasq(16, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_scasb(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_scasw(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_scasd(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{32, Instruction::with_scasq(32, RepPrefixKind::None).value(), OpKind::MemoryESRDI},
		{64, Instruction::with_scasb(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
		{64, Instruction::with_scasw(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
		{64, Instruction::with_scasd(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
		{64, Instruction::with_scasq(64, RepPrefixKind::None).value(), OpKind::MemoryESDI},
	};
	test_invalid_op_kinds(tests, 0);
}

TEST_CASE("encoder/test_invalid_xlatb") {
	const std::vector<std::tuple<std::uint32_t, Instruction, Register>> tests = {
		{16, Instruction::with1(Code::Xlat_m8, MemoryOperand(Register::BX, Register::AL, 1, 0, 0, false, Register::None)).value(), Register::RBX},
		{32, Instruction::with1(Code::Xlat_m8, MemoryOperand(Register::EBX, Register::AL, 1, 0, 0, false, Register::None)).value(), Register::RBX},
		{64, Instruction::with1(Code::Xlat_m8, MemoryOperand(Register::RBX, Register::AL, 1, 0, 0, false, Register::None)).value(), Register::BX},
	};
	for (const auto& [bitness, orig_instr, invalid_rbx] : tests) {
		{
			Encoder encoder(bitness);
			CHECK(encoder.encode(orig_instr, 0).is_ok());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_memory_base(invalid_rbx);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_memory_base(Register::ESI);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_memory_index(Register::AX);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_memory_index(Register::None);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		for (std::uint32_t scale : {2U, 4U, 8U}) {
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_memory_index_scale(scale);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		const std::uint32_t invalid_displ_size = bitness == 64 ? 4 : 8;
		const std::pair<std::uint64_t, std::uint32_t> displs[] = {{0, 1}, {1, invalid_displ_size}, {1, 1}};
		for (const auto& [displ, displ_size] : displs) {
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_memory_displacement64(displ);
			instr.set_memory_displ_size(displ_size);
			CHECK(encoder.encode(instr, 0).is_err());
		}
	}
}

TEST_CASE("encoder/test_invalid_const_imm_op") {
	Encoder encoder(64);
	const Instruction instr = Instruction::with2(Code::Rol_rm8_1, Register::AL, 0).value();
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_invalid_is5_imm_op") {
	for (std::uint32_t imm = 0; imm < 0x100; imm++) {
		Encoder encoder(64);
		const Instruction instr =
			Instruction::with5(Code::VEX_Vpermil2ps_xmm_xmm_xmmm128_xmm_imm4, Register::XMM0, Register::XMM1, Register::XMM2, Register::XMM3, imm).value();
		if (imm <= 0x0F)
			CHECK(encoder.encode(instr, 0).is_ok());
		else
			CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_encode_invalid_instr") {
	Encoder encoder(64);
	const Instruction instr;
	CHECK_EQ(instr.code(), Code::INVALID);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_high_r8_reg_with_rex_prefix") {
	for (Register reg : {Register::AH, Register::CH, Register::DH, Register::BH}) {
		Encoder encoder(64);
		const Instruction instr = Instruction::with2(Code::Movzx_r64_rm8, Register::RAX, reg).value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_evex_invalid_k1") {
	Encoder encoder(64);
	Instruction instr = Instruction::with2(Code::EVEX_Vucomiss_xmm_xmmm32_sae, Register::XMM0, Register::XMM1).value();
	instr.set_op_mask(Register::K1);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/encode_without_required_op_mask_register") {
	Encoder encoder(64);
	Instruction instr = Instruction::with2(Code::EVEX_Vpgatherdd_xmm_k1_vm32x, Register::XMM0,
		MemoryOperand(Register::RAX, Register::XMM1, 1, 0x10, 1, false, Register::None))
							.value();
	CHECK(encoder.encode(instr, 0).is_err());
	instr.set_op_mask(Register::K1);
	CHECK(encoder.encode(instr, 0).is_ok());
}

TEST_CASE("encoder/encode_invalid_sae") {
	Encoder encoder(64);
	Instruction instr = Instruction::with2(Code::EVEX_Vmovups_xmm_k1z_xmmm128, Register::XMM0, Register::XMM1).value();
	CHECK(encoder.encode(instr, 0).is_ok());
	instr.set_suppress_all_exceptions(true);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/encode_invalid_er") {
	for (std::size_t i = 0; i < IcedConstants::ROUNDING_CONTROL_ENUM_COUNT; i++) {
		const RoundingControl rc = static_cast<RoundingControl>(i);
		Encoder encoder(64);
		Instruction instr = Instruction::with2(Code::EVEX_Vmovups_xmm_k1z_xmmm128, Register::XMM0, Register::XMM1).value();
		instr.set_rounding_control(rc);
		if (rc == RoundingControl::None)
			CHECK(encoder.encode(instr, 0).is_ok());
		else
			CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/encode_invalid_bcst") {
	{
		Encoder encoder(64);
		Instruction instr = Instruction::with2(Code::EVEX_Vmovups_xmm_k1z_xmmm128, Register::XMM0, MemoryOperand::with_base(Register::RAX)).value();
		CHECK(encoder.encode(instr, 0).is_ok());
		instr.set_is_broadcast(true);
		CHECK(encoder.encode(instr, 0).is_err());
	}
	{
		Encoder encoder(64);
		Instruction instr =
			Instruction::with3(Code::EVEX_Vunpcklps_xmm_k1z_xmm_xmmm128b32, Register::XMM0, Register::XMM1, MemoryOperand::with_base(Register::RAX)).value();
		CHECK(encoder.encode(instr, 0).is_ok());
		instr.set_is_broadcast(true);
		CHECK(encoder.encode(instr, 0).is_ok());
	}
	{
		Encoder encoder(64);
		Instruction instr = Instruction::with3(Code::EVEX_Vunpcklps_xmm_k1z_xmm_xmmm128b32, Register::XMM0, Register::XMM1, Register::XMM2).value();
		CHECK(encoder.encode(instr, 0).is_ok());
		instr.set_is_broadcast(true);
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/encode_invalid_zmsk") {
	Encoder encoder(64);
	Instruction instr = Instruction::with2(Code::EVEX_Vmovss_m32_k1_xmm, MemoryOperand::with_base(Register::RAX), Register::XMM1).value();
	CHECK(encoder.encode(instr, 0).is_ok());
	instr.set_zeroing_masking(true);
	CHECK(encoder.encode(instr, 0).is_err());
	instr.set_op_mask(Register::K1);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/encode_invalid_abs_address") {
	const std::tuple<std::uint32_t, std::uint64_t, std::uint32_t> tests[] = {
		{16, 0x1234, 2},
		{16, 0x1234'5678, 4},
		{32, 0x1234, 2},
		{32, 0x1234'5678, 4},
		{64, 0x1234'5678, 4},
		{64, 0x1234'5678'9ABC'DEF0, 8},
	};
	for (const auto& [bitness, address, displ_size] : tests) {
		Register mem_reg;
		switch (displ_size) {
		case 2:
			mem_reg = Register::BX;
			break;
		case 4:
			mem_reg = Register::EBX;
			break;
		case 8:
			mem_reg = Register::RBX;
			break;
		default:
			FAIL("unreachable");
		}

		const Instruction orig_instr = Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(address, displ_size)).value();
		{
			Encoder encoder(bitness);
			CHECK(encoder.encode(orig_instr, 0).is_ok());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_memory_base(mem_reg);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_memory_index(mem_reg);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		for (std::uint32_t scale : {2U, 4U, 8U}) {
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_memory_index_scale(scale);
			CHECK(encoder.encode(instr, 0).is_err());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_op1_kind(OpKind::Immediate8);
			CHECK(encoder.encode(instr, 0).is_err());
		}
	}

	const std::tuple<std::uint32_t, std::uint64_t, std::uint32_t> tests2[] = {
		{16, 0x1234, 8},
		{32, 0x1234, 8},
		{64, 0x1234, 2},
	};
	for (const auto& [bitness, address, displ_size] : tests2) {
		Encoder encoder(bitness);
		const Instruction instr = Instruction::with2(Code::Mov_EAX_moffs32, Register::EAX, MemoryOperand::with_displ(address, displ_size)).value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_reg_op_not_allowed") {
	Encoder encoder(64);
	{
		const Instruction instr = Instruction::with2(Code::Lea_r32_m, Register::EAX, MemoryOperand::with_base(Register::RAX)).value();
		CHECK(encoder.encode(instr, 0).is_ok());
	}
	{
		const Instruction instr = Instruction::with2(Code::Lea_r32_m, Register::EAX, Register::ECX).value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_mem_op_not_allowed") {
	Encoder encoder(64);
	{
		const Instruction instr = Instruction::with2(Code::Movhlps_xmm_xmm, Register::XMM0, Register::XMM1).value();
		CHECK(encoder.encode(instr, 0).is_ok());
	}
	{
		const Instruction instr = Instruction::with2(Code::Movhlps_xmm_xmm, Register::XMM0, MemoryOperand::with_base(Register::RAX)).value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_regmem_op_is_wrong_size") {
	const std::vector<std::tuple<std::uint32_t, Instruction, Register>> tests = {
		{16, Instruction::with2(Code::Enqcmd_r16_m512, Register::AX, MemoryOperand::with_base(Register::BX)).value(), Register::EAX},
		{16, Instruction::with2(Code::Enqcmd_r32_m512, Register::EAX, MemoryOperand::with_base(Register::EAX)).value(), Register::AX},
		{16, Instruction::with2(Code::Enqcmd_r32_m512, Register::EAX, MemoryOperand::with_base(Register::EAX)).value(), Register::RAX},
		{32, Instruction::with2(Code::Enqcmd_r16_m512, Register::AX, MemoryOperand::with_base(Register::BX)).value(), Register::EAX},
		{32, Instruction::with2(Code::Enqcmd_r32_m512, Register::EAX, MemoryOperand::with_base(Register::EAX)).value(), Register::AX},
		{32, Instruction::with2(Code::Enqcmd_r32_m512, Register::EAX, MemoryOperand::with_base(Register::EAX)).value(), Register::RAX},
		{64, Instruction::with2(Code::Enqcmd_r32_m512, Register::EAX, MemoryOperand::with_base(Register::EAX)).value(), Register::RAX},
		{64, Instruction::with2(Code::Enqcmd_r64_m512, Register::RAX, MemoryOperand::with_base(Register::RAX)).value(), Register::EAX},
		{64, Instruction::with2(Code::Enqcmd_r64_m512, Register::RAX, MemoryOperand::with_base(Register::RAX)).value(), Register::AX},
	};
	for (const auto& [bitness, orig_instr, invalid_reg] : tests) {
		{
			Encoder encoder(bitness);
			CHECK(encoder.encode(orig_instr, 0).is_ok());
		}
		{
			Encoder encoder(bitness);
			Instruction instr = orig_instr;
			instr.set_op0_register(invalid_reg);
			CHECK(encoder.encode(instr, 0).is_err());
		}
	}
}

TEST_CASE("encoder/test_vsib_16bit_addr") {
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		Encoder encoder(bitness);
		Instruction instr = Instruction::with2(Code::EVEX_Vpgatherdd_xmm_k1_vm32x, Register::XMM0,
			MemoryOperand(Register::EAX, Register::XMM1, 1, 0x10, 1, false, Register::None))
								.value();
		instr.set_op_mask(Register::K1);
		CHECK(encoder.encode(instr, 0).is_ok());
		instr.set_memory_base(Register::BX);
		instr.set_memory_index(Register::SI);
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_expected_reg_or_mem_op_kind") {
	Encoder encoder(64);
	Instruction instr = Instruction::with2(Code::Add_rm8_imm8, Register::AL, 123).value();
	CHECK(encoder.encode(instr, 0).is_ok());
	instr.set_op0_kind(OpKind::Immediate8);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_16bit_addr_in_64bit_mode") {
	Encoder encoder(64);
	const Instruction instr = Instruction::with2(Code::Lea_r32_m, Register::EAX, MemoryOperand::with_base(Register::BX)).value();
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_64bit_addr_in_16_32bit_mode") {
	for (std::uint32_t bitness : {16U, 32U}) {
		Encoder encoder(bitness);
		const Instruction instr = Instruction::with2(Code::Lea_r32_m, Register::EAX, MemoryOperand::with_base(Register::RAX)).value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_invalid_16bit_mem_regs") {
	const std::pair<Register, Register> tests[] = {
		{Register::AX, Register::None},
		{Register::R8W, Register::None},
		{Register::BL, Register::None},
		{Register::None, Register::CX},
		{Register::None, Register::R9W},
		{Register::None, Register::SIL},
		{Register::BX, Register::BP},
		{Register::BP, Register::BX},
	};
	for (const auto& [base, index] : tests) {
		Encoder encoder(16);
		const Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_index(base, index)).value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_invalid_16bit_displ_size") {
	Encoder encoder(16);
	Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ(Register::BX, 1)).value();
	CHECK(encoder.encode(instr, 0).is_ok());
	instr.set_memory_displ_size(4);
	CHECK(encoder.encode(instr, 0).is_err());
	instr.set_memory_displ_size(8);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_invalid_32bit_displ_size") {
	Encoder encoder(32);
	Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ(Register::EAX, 1)).value();
	CHECK(encoder.encode(instr, 0).is_ok());
	instr.set_memory_displ_size(2);
	CHECK(encoder.encode(instr, 0).is_err());
	instr.set_memory_displ_size(8);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_invalid_64bit_displ_size") {
	Encoder encoder(64);
	Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_displ(Register::RAX, 1)).value();
	CHECK(encoder.encode(instr, 0).is_ok());
	instr.set_memory_displ_size(2);
	CHECK(encoder.encode(instr, 0).is_err());
	instr.set_memory_displ_size(4);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_invalid_ip_rel_memory") {
	const std::pair<Register, Register> regs[] = {{Register::EIP, Register::EDI}, {Register::RIP, Register::RDI}};
	for (const auto& [ip_reg, invalid_index] : regs) {
		{
			Encoder encoder(64);
			const Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand(ip_reg, Register::None, 1, 0, 8, false, Register::None)).value();
			CHECK(encoder.encode(instr, 0).is_ok());
		}
		for (std::uint32_t displ_size : {0U, 1U, 4U, 8U}) {
			Encoder encoder(64);
			const Instruction instr =
				Instruction::with1(Code::Not_rm8, MemoryOperand(ip_reg, Register::None, 1, 0, displ_size, false, Register::None)).value();
			CHECK(encoder.encode(instr, 0).is_ok());
		}
		{
			Encoder encoder(64);
			const Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand(ip_reg, Register::None, 1, 0, 2, false, Register::None)).value();
			CHECK(encoder.encode(instr, 0).is_err());
		}
		{
			Encoder encoder(64);
			const Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand(ip_reg, invalid_index, 1, 0, 8, false, Register::None)).value();
			CHECK(encoder.encode(instr, 0).is_err());
		}
		for (std::uint32_t scale : {2U, 4U, 8U}) {
			Encoder encoder(64);
			const Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand(ip_reg, Register::None, scale, 0, 8, false, Register::None)).value();
			CHECK(encoder.encode(instr, 0).is_err());
		}
	}
}

TEST_CASE("encoder/test_invalid_ip_rel_memory_16_32") {
	for (std::uint32_t bitness : {16U, 32U}) {
		const std::pair<Register, std::uint32_t> regs[] = {{Register::EIP, 4}, {Register::RIP, 8}};
		for (const auto& [ip_reg, displ_size] : regs) {
			Encoder encoder(bitness);
			const Instruction instr =
				Instruction::with1(Code::Not_rm8, MemoryOperand(ip_reg, Register::None, 1, 0, displ_size, false, Register::None)).value();
			CHECK(encoder.encode(instr, 0).is_err());
		}
	}
}

TEST_CASE("encoder/test_invalid_ip_rel_memory_sib_required") {
	{
		Encoder encoder(64);
		const Instruction instr = Instruction::with2(Code::VEX_Tileloaddt1_tmm_sibmem, Register::TMM1,
			MemoryOperand(Register::RCX, Register::RDX, 1, 0x1234'5678, 8, false, Register::None))
									  .value();
		CHECK(encoder.encode(instr, 0).is_ok());
	}
	{
		Encoder encoder(64);
		const Instruction instr = Instruction::with2(Code::VEX_Tileloaddt1_tmm_sibmem, Register::TMM1,
			MemoryOperand(Register::RIP, Register::None, 1, 0x1234'5678, 8, false, Register::None))
									  .value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
	{
		Encoder encoder(64);
		const Instruction instr = Instruction::with2(Code::VEX_Tileloaddt1_tmm_sibmem, Register::TMM1,
			MemoryOperand(Register::ECX, Register::EDX, 1, 0x1234'5678, 4, false, Register::None))
									  .value();
		CHECK(encoder.encode(instr, 0).is_ok());
	}
	{
		Encoder encoder(64);
		const Instruction instr = Instruction::with2(Code::VEX_Tileloaddt1_tmm_sibmem, Register::TMM1,
			MemoryOperand(Register::EIP, Register::None, 1, 0x1234'5678, 4, false, Register::None))
									  .value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_invalid_eip_rel_mem_target_addr") {
	for (std::uint64_t target : {UINT64_C(0), UINT64_C(0x7FFF'FFFF), UINT64_C(0xFFFF'FFFF)}) {
		Encoder encoder(64);
		const Instruction instr =
			Instruction::with1(Code::Not_rm8, MemoryOperand(Register::EIP, Register::None, 1, static_cast<std::int64_t>(target), 4, false, Register::None))
				.value();
		CHECK(encoder.encode(instr, 0).is_ok());
	}
	for (std::uint64_t target : {UINT64_C(0x1'0000'0000), UINT64_C(0xFFFF'FFFF'FFFF'FFFF)}) {
		Encoder encoder(64);
		const Instruction instr =
			Instruction::with1(Code::Not_rm8, MemoryOperand(Register::EIP, Register::None, 1, static_cast<std::int64_t>(target), 4, false, Register::None))
				.value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_vsib_with_offset_only_mem") {
	Encoder encoder(64);
	Instruction instr = Instruction::with2(Code::EVEX_Vpgatherdd_xmm_k1_vm32x, Register::XMM0,
		MemoryOperand(Register::RAX, Register::XMM1, 1, 0x1234'5678, 8, false, Register::None))
							.value();
	instr.set_op_mask(Register::K1);
	CHECK(encoder.encode(instr, 0).is_ok());
	instr.set_memory_base(Register::None);
	instr.set_memory_index(Register::None);
	CHECK(encoder.encode(instr, 0).is_err());
}

TEST_CASE("encoder/test_invalid_esp_rsp_index_regs") {
	for (Register sp_reg : {Register::ESP, Register::RSP}) {
		Encoder encoder(64);
		const Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand::with_base_index_scale(Register::None, sp_reg, 2)).value();
		CHECK(encoder.encode(instr, 0).is_err());
	}
}

TEST_CASE("encoder/test_rip_rel_dist_too_far_away") {
	constexpr std::size_t INSTR_LEN = 6;
	constexpr std::uint64_t INSTR_ADDR = 0x1234'5678'9ABC'DEF0;
	for (std::int64_t diff : {static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()), static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()),
			 INT64_C(-1), INT64_C(0), INT64_C(1), INT64_C(-0x1234'5678), INT64_C(0x1234'5678)}) {
		Encoder encoder(64);
		const std::int64_t target = static_cast<std::int64_t>(INSTR_ADDR + INSTR_LEN + static_cast<std::uint64_t>(diff));
		const Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand(Register::RIP, Register::None, 1, target, 8, false, Register::None)).value();
		auto result = encoder.encode(instr, INSTR_ADDR);
		REQUIRE_MSG(result.is_ok(), result.is_err() ? result.error().message() : "");
		CHECK_EQ(result.value(), INSTR_LEN);

		const auto bytes = encoder.take_buffer();
		auto decoder = Decoder::with_ip(64, bytes, INSTR_ADDR, DecoderOptions::NONE);
		const Instruction decoded = decoder.decode();
		CHECK_EQ(decoded.code(), Code::Not_rm8);
		CHECK_EQ(decoded.memory_base(), Register::RIP);
		CHECK_EQ(decoded.memory_displacement64(), static_cast<std::uint64_t>(target));
	}
	for (std::int64_t diff : {static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) - 1,
			 static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1, INT64_C(-0x1234'5678'9ABC'DEF0), INT64_C(0x1234'5678'9ABC'DEF0),
			 std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}) {
		Encoder encoder(64);
		const std::int64_t target = static_cast<std::int64_t>(INSTR_ADDR + INSTR_LEN + static_cast<std::uint64_t>(diff));
		const Instruction instr = Instruction::with1(Code::Not_rm8, MemoryOperand(Register::RIP, Register::None, 1, target, 8, false, Register::None)).value();
		CHECK(encoder.encode(instr, INSTR_ADDR).is_err());
	}
}

namespace {

using CreateBrInstrFn = Instruction (*)(Code code, std::uint32_t bitness, std::uint64_t target);

void test_invalid_br(std::uint32_t bitness, Code code, std::uint64_t instr_addr, std::size_t instr_len, std::uint64_t addr_mask,
	const std::vector<std::int64_t>& valid_diffs, const std::vector<std::int64_t>& invalid_diffs, CreateBrInstrFn create_instr) {
	for (const std::int64_t diff : valid_diffs) {
		Encoder encoder(bitness);
		const std::uint64_t target = (instr_addr + instr_len + static_cast<std::uint64_t>(diff)) & addr_mask;
		const Instruction instr = create_instr(code, bitness, target);
		auto result = encoder.encode(instr, instr_addr);
		REQUIRE_MSG(result.is_ok(), result.is_err() ? result.error().message() : "");
		CHECK_EQ(result.value(), instr_len);

		const auto bytes = encoder.take_buffer();
		auto decoder = Decoder::with_ip(bitness, bytes, instr_addr, DecoderOptions::NONE);
		const Instruction decoded = decoder.decode();
		CHECK_EQ(decoded.code(), code);
		CHECK_EQ(decoded.near_branch64(), target);
	}
	for (const std::int64_t diff : invalid_diffs) {
		Encoder encoder(bitness);
		const std::uint64_t target = (instr_addr + instr_len + static_cast<std::uint64_t>(diff)) & addr_mask;
		const Instruction instr = create_instr(code, bitness, target);
		CHECK(encoder.encode(instr, instr_addr).is_err());
	}
}

void test_invalid_jcc(std::uint32_t bitness, Code code, std::uint64_t instr_addr, std::size_t instr_len, std::uint64_t addr_mask,
	const std::vector<std::int64_t>& valid_diffs, const std::vector<std::int64_t>& invalid_diffs) {
	test_invalid_br(bitness, code, instr_addr, instr_len, addr_mask, valid_diffs, invalid_diffs,
		[](Code c, std::uint32_t, std::uint64_t target) { return Instruction::with_branch(c, target).value(); });
}

void test_invalid_xbegin(std::uint32_t bitness, Code code, std::uint64_t instr_addr, std::size_t instr_len, std::uint64_t addr_mask,
	const std::vector<std::int64_t>& valid_diffs, const std::vector<std::int64_t>& invalid_diffs) {
	test_invalid_br(bitness, code, instr_addr, instr_len, addr_mask, valid_diffs, invalid_diffs, [](Code c, std::uint32_t b, std::uint64_t target) {
		Instruction instr = Instruction::with_xbegin(b, target).value();
		instr.set_code(c);
		return instr;
	});
}

constexpr std::int64_t I8_MIN = std::numeric_limits<std::int8_t>::min();
constexpr std::int64_t I8_MAX = std::numeric_limits<std::int8_t>::max();
constexpr std::int64_t I16_MIN = std::numeric_limits<std::int16_t>::min();
constexpr std::int64_t I16_MAX = std::numeric_limits<std::int16_t>::max();
constexpr std::int64_t I32_MIN = std::numeric_limits<std::int32_t>::min();
constexpr std::int64_t I32_MAX = std::numeric_limits<std::int32_t>::max();
constexpr std::int64_t I64_MIN = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t I64_MAX = std::numeric_limits<std::int64_t>::max();

} // namespace

TEST_CASE("encoder/test_invalid_jcc_rel8_16") {
	const std::vector<std::int64_t> valid_diffs = {I8_MIN, I8_MAX, -1, 0, 1, -0x12, 0x12};
	const std::vector<std::int64_t> invalid_diffs = {I8_MIN - 1, I8_MAX + 1, -0x1234, 0x1234, I16_MIN, I16_MAX};
	test_invalid_jcc(16, Code::Je_rel8_16, 0x1234, 2, 0xFFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_jcc_rel8_32") {
	const std::vector<std::int64_t> valid_diffs = {I8_MIN, I8_MAX, -1, 0, 1, -0x12, 0x12};
	const std::vector<std::int64_t> invalid_diffs = {I8_MIN - 1, I8_MAX + 1, -0x1234'5678, 0x1234'5678, I32_MIN, I32_MAX};
	test_invalid_jcc(32, Code::Je_rel8_32, 0x1234'5678, 2, 0xFFFF'FFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_jcc_rel8_64") {
	const std::vector<std::int64_t> valid_diffs = {I8_MIN, I8_MAX, -1, 0, 1, -0x12, 0x12};
	const std::vector<std::int64_t> invalid_diffs = {I8_MIN - 1, I8_MAX + 1, -0x1234'5678'9ABC'DEF0, 0x1234'5678'9ABC'DEF0, I64_MIN, I64_MAX};
	test_invalid_jcc(64, Code::Je_rel8_64, 0x1234'5678'9ABC'DEF0, 2, 0xFFFF'FFFF'FFFF'FFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_jcc_rel16_16") {
	const std::vector<std::int64_t> valid_diffs = {I16_MIN, I16_MAX, -1, 0, 1, -0x1234, 0x1234};
	const std::vector<std::int64_t> invalid_diffs;
	test_invalid_jcc(16, Code::Je_rel16, 0x1234, 4, 0xFFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_jcc_rel32_32") {
	const std::vector<std::int64_t> valid_diffs = {I32_MIN, I32_MAX, -1, 0, 1, -0x1234'5678, 0x1234'5678};
	const std::vector<std::int64_t> invalid_diffs;
	test_invalid_jcc(32, Code::Je_rel32_32, 0x1234'5678, 6, 0xFFFF'FFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_jcc_rel32_64") {
	const std::vector<std::int64_t> valid_diffs = {I32_MIN, I32_MAX, -1, 0, 1, -0x1234'5678, 0x1234'5678};
	const std::vector<std::int64_t> invalid_diffs = {I32_MIN - 1, I32_MAX + 1, -0x1234'5678'9ABC'DEF0, 0x1234'5678'9ABC'DEF0, I64_MIN, I64_MAX};
	test_invalid_jcc(64, Code::Je_rel32_64, 0x1234'5678'9ABC'DEF0, 6, 0xFFFF'FFFF'FFFF'FFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_xbegin_rel16_16") {
	const std::vector<std::int64_t> valid_diffs = {I16_MIN, I16_MAX, -1, 0, 1, -0x1234, 0x1234};
	const std::vector<std::int64_t> invalid_diffs = {I16_MIN - 1, I16_MAX + 1, -0x1234'5678, 0x1234'5678, I32_MIN, I32_MAX};
	test_invalid_xbegin(16, Code::Xbegin_rel16, 0x1234, 4, 0xFFFF'FFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_xbegin_rel32_16") {
	const std::vector<std::int64_t> valid_diffs = {I32_MIN, I32_MAX, -1, 0, 1, -0x1234'5678, 0x1234'5678};
	const std::vector<std::int64_t> invalid_diffs;
	test_invalid_xbegin(16, Code::Xbegin_rel32, 0x1234, 7, 0xFFFF'FFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_xbegin_rel16_32") {
	const std::vector<std::int64_t> valid_diffs = {I16_MIN, I16_MAX, -1, 0, 1, -0x1234, 0x1234};
	const std::vector<std::int64_t> invalid_diffs = {I16_MIN - 1, I16_MAX + 1, -0x1234'5678, 0x1234'5678, I32_MIN, I32_MAX};
	test_invalid_xbegin(32, Code::Xbegin_rel16, 0x1234'5678, 5, 0xFFFF'FFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_xbegin_rel32_32") {
	const std::vector<std::int64_t> valid_diffs = {I32_MIN, I32_MAX, -1, 0, 1, -0x1234'5678, 0x1234'5678};
	const std::vector<std::int64_t> invalid_diffs;
	test_invalid_xbegin(32, Code::Xbegin_rel32, 0x1234'5678, 6, 0xFFFF'FFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_xbegin_rel16_64") {
	const std::vector<std::int64_t> valid_diffs = {I16_MIN, I16_MAX, -1, 0, 1, -0x1234, 0x1234};
	const std::vector<std::int64_t> invalid_diffs = {I16_MIN - 1, I16_MAX + 1, -0x1234'5678'9ABC'DEF0, 0x1234'5678'9ABC'DEF0, I64_MIN, I64_MAX};
	test_invalid_xbegin(64, Code::Xbegin_rel16, 0x1234'5678'9ABC'DEF0, 5, 0xFFFF'FFFF'FFFF'FFFF, valid_diffs, invalid_diffs);
}

TEST_CASE("encoder/test_invalid_xbegin_rel32_64") {
	const std::vector<std::int64_t> valid_diffs = {I32_MIN, I32_MAX, -1, 0, 1, -0x1234'5678, 0x1234'5678};
	const std::vector<std::int64_t> invalid_diffs = {I32_MIN - 1, I32_MAX + 1, -0x1234'5678'9ABC'DEF0, 0x1234'5678'9ABC'DEF0, I64_MIN, I64_MAX};
	test_invalid_xbegin(64, Code::Xbegin_rel32, 0x1234'5678'9ABC'DEF0, 6, 0xFFFF'FFFF'FFFF'FFFF, valid_diffs, invalid_diffs);
}

} // namespace iced_x86::tests
