// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's decoder/tests/mod.rs

#include "test_framework.hpp"
#include "test_utils.hpp"
#include "test_utils/decoder_test_utils.hpp"
#include "test_utils/from_str_conv.hpp"

#include "iced_x86/code_ext.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/iced_constants.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace iced_x86::tests {

namespace {

const Code NON_DECODED_CODE_VALUES[17] = {
	Code::DeclareByte,
	Code::DeclareDword,
	Code::DeclareQword,
	Code::DeclareWord,
	Code::Zero_bytes,
	Code::Fclex,
	Code::Fdisi,
	Code::Feni,
	Code::Finit,
	Code::Fsave_m108byte,
	Code::Fsave_m94byte,
	Code::Fsetpm,
	Code::Fstcw_m2byte,
	Code::Fstenv_m14byte,
	Code::Fstenv_m28byte,
	Code::Fstsw_AX,
	Code::Fstsw_m2byte,
};

const Code NON_DECODED_CODE_VALUES1632[3] = {
	Code::Popw_CS,
	Code::Fstdw_AX,
	Code::Fstsg_AX,
};

void verify_constant_offsets(const ConstantOffsets& expected, const ConstantOffsets& actual) {
	CHECK_EQ(actual.immediate_offset(), expected.immediate_offset());
	CHECK_EQ(actual.immediate_size(), expected.immediate_size());
	CHECK_EQ(actual.immediate_offset2(), expected.immediate_offset2());
	CHECK_EQ(actual.immediate_size2(), expected.immediate_size2());
	CHECK_EQ(actual.displacement_offset(), expected.displacement_offset());
	CHECK_EQ(actual.displacement_size(), expected.displacement_size());
}

std::string tc_msg(const DecoderTestCase& tc) { return "line " + std::to_string(tc.line_number) + ": " + tc.hex_bytes; }

void decode_test(std::uint32_t bitness, const DecoderTestCase& tc) {
	auto bytes = to_vec_u8(tc.hex_bytes);
	auto created = create_decoder(bitness, bytes, tc.ip, tc.decoder_options);
	Decoder& decoder = created.decoder;
	std::size_t len = created.len;
	CHECK_EQ(decoder.position(), 0U);
	CHECK_EQ(decoder.max_position(), bytes.size());
	std::uint64_t rip = decoder.ip();
	Instruction instr = decoder.decode();
	std::size_t failures_before = 0;
	(void)failures_before;
	CHECK_MSG(decoder.last_error() == tc.decoder_error, tc_msg(tc));
	CHECK_EQ(decoder.position(), len);
	CHECK_EQ(decoder.can_decode(), created.can_read);
	CHECK_MSG(instr.code() == tc.code, tc_msg(tc) + " code: " + to_string(instr.code()) + " != " + to_string(tc.code));
	CHECK_EQ(instr.is_invalid(), tc.code == Code::INVALID);
	CHECK_EQ(instr.mnemonic(), tc.mnemonic);
	CHECK_EQ(code_ext::mnemonic(instr.code()), instr.mnemonic());
	CHECK_EQ(instr.len(), len);
	CHECK_EQ(instr.ip(), rip);
	CHECK_EQ(instr.next_ip(), decoder.ip());
	CHECK_EQ(instr.next_ip(), rip + len);
	switch (bitness) {
	case 16:
		CHECK_EQ(instr.code_size(), CodeSize::Code16);
		break;
	case 32:
		CHECK_EQ(instr.code_size(), CodeSize::Code32);
		break;
	case 64:
		CHECK_EQ(instr.code_size(), CodeSize::Code64);
		break;
	default:
		FAIL("Invalid bitness");
	}
	CHECK_MSG(instr.op_count() == tc.op_count, tc_msg(tc));
	CHECK_EQ(instr.zeroing_masking(), tc.zeroing_masking);
	CHECK_EQ(instr.merging_masking(), !tc.zeroing_masking);
	CHECK_EQ(instr.suppress_all_exceptions(), tc.suppress_all_exceptions);
	CHECK_EQ(instr.is_broadcast(), tc.is_broadcast);
	CHECK_EQ(instr.has_xacquire_prefix(), tc.has_xacquire_prefix);
	CHECK_EQ(instr.has_xrelease_prefix(), tc.has_xrelease_prefix);
	CHECK_EQ(instr.has_rep_prefix(), tc.has_repe_prefix);
	CHECK_EQ(instr.has_repe_prefix(), tc.has_repe_prefix);
	CHECK_EQ(instr.has_repne_prefix(), tc.has_repne_prefix);
	CHECK_EQ(instr.has_lock_prefix(), tc.has_lock_prefix);
	CHECK_EQ(instr.is_mvex_eviction_hint(), tc.mvex.eviction_hint);
	CHECK_EQ(instr.mvex_reg_mem_conv(), tc.mvex.reg_mem_conv);
	switch (tc.vsib_bitness) {
	case 0:
		CHECK(!instr.is_vsib());
		CHECK(!instr.is_vsib32());
		CHECK(!instr.is_vsib64());
		CHECK(!instr.vsib().has_value());
		break;
	case 32:
		CHECK(instr.is_vsib());
		CHECK(instr.is_vsib32());
		CHECK(!instr.is_vsib64());
		CHECK(instr.vsib() == std::optional<bool>(false));
		break;
	case 64:
		CHECK(instr.is_vsib());
		CHECK(!instr.is_vsib32());
		CHECK(instr.is_vsib64());
		CHECK(instr.vsib() == std::optional<bool>(true));
		break;
	default:
		FAIL("Invalid vsib bitness");
	}
	CHECK_EQ(instr.op_mask(), tc.op_mask);
	CHECK_EQ(instr.has_op_mask(), tc.op_mask != Register::None);
	CHECK_EQ(instr.rounding_control(), tc.rounding_control);
	CHECK_EQ(instr.segment_prefix(), tc.segment_prefix);
	if (instr.segment_prefix() == Register::None)
		CHECK(!instr.has_segment_prefix());
	else
		CHECK(instr.has_segment_prefix());
	auto op_kinds_iter = instr.op_kinds();
	CHECK_EQ(static_cast<std::uint32_t>(op_kinds_iter.size()), instr.op_count());
	std::vector<OpKind> op_kinds(op_kinds_iter.begin(), op_kinds_iter.end());
	CHECK_EQ(static_cast<std::uint32_t>(op_kinds.size()), instr.op_count());
	for (std::uint32_t i = 0; i < op_kinds.size(); i++) {
		CHECK_EQ(instr.op_kind(i), op_kinds[i]);
		CHECK_EQ(instr.try_op_kind(i).value(), op_kinds[i]);
	}
	for (std::uint32_t i = 0; i < tc.op_count && i < IcedConstants::MAX_OP_COUNT; i++) {
		OpKind op_kind = tc.op_kinds[i];
		CHECK_MSG(instr.op_kind(i) == op_kind, tc_msg(tc) + " op" + std::to_string(i));
		CHECK_EQ(instr.try_op_kind(i).value(), op_kind);
		switch (op_kind) {
		case OpKind::Register:
			CHECK_MSG(instr.op_register(i) == tc.op_registers[i], tc_msg(tc) + " op" + std::to_string(i));
			CHECK_EQ(instr.try_op_register(i).value(), tc.op_registers[i]);
			break;
		case OpKind::NearBranch16:
			CHECK_EQ(tc.near_branch, static_cast<std::uint64_t>(instr.near_branch16()));
			CHECK_EQ(tc.near_branch, instr.near_branch_target());
			break;
		case OpKind::NearBranch32:
			CHECK_EQ(tc.near_branch, static_cast<std::uint64_t>(instr.near_branch32()));
			CHECK_EQ(tc.near_branch, instr.near_branch_target());
			break;
		case OpKind::NearBranch64:
			CHECK_EQ(tc.near_branch, instr.near_branch64());
			CHECK_EQ(tc.near_branch, instr.near_branch_target());
			break;
		case OpKind::FarBranch16:
			CHECK_EQ(static_cast<std::uint32_t>(instr.far_branch16()), tc.far_branch);
			CHECK_EQ(instr.far_branch_selector(), tc.far_branch_selector);
			break;
		case OpKind::FarBranch32:
			CHECK_EQ(instr.far_branch32(), tc.far_branch);
			CHECK_EQ(instr.far_branch_selector(), tc.far_branch_selector);
			break;
		case OpKind::Immediate8:
			CHECK_EQ(static_cast<std::uint8_t>(tc.immediate), instr.immediate8());
			break;
		case OpKind::Immediate8_2nd:
			CHECK_EQ(tc.immediate_2nd, instr.immediate8_2nd());
			break;
		case OpKind::Immediate16:
			CHECK_EQ(static_cast<std::uint16_t>(tc.immediate), instr.immediate16());
			break;
		case OpKind::Immediate32:
			CHECK_EQ(static_cast<std::uint32_t>(tc.immediate), instr.immediate32());
			break;
		case OpKind::Immediate64:
			CHECK_EQ(tc.immediate, instr.immediate64());
			break;
		case OpKind::Immediate8to16:
			CHECK_EQ(static_cast<std::int16_t>(tc.immediate), instr.immediate8to16());
			break;
		case OpKind::Immediate8to32:
			CHECK_EQ(static_cast<std::int32_t>(tc.immediate), instr.immediate8to32());
			break;
		case OpKind::Immediate8to64:
			CHECK_EQ(static_cast<std::int64_t>(tc.immediate), instr.immediate8to64());
			break;
		case OpKind::Immediate32to64:
			CHECK_EQ(static_cast<std::int64_t>(tc.immediate), instr.immediate32to64());
			break;
		case OpKind::MemorySegSI:
		case OpKind::MemorySegESI:
		case OpKind::MemorySegRSI:
		case OpKind::MemorySegDI:
		case OpKind::MemorySegEDI:
		case OpKind::MemorySegRDI:
			CHECK_EQ(instr.memory_segment(), tc.memory_segment);
			CHECK_EQ(instr.memory_size(), tc.memory_size);
			break;
		case OpKind::MemoryESDI:
		case OpKind::MemoryESEDI:
		case OpKind::MemoryESRDI:
			CHECK_EQ(tc.memory_size, instr.memory_size());
			break;
		case OpKind::Memory:
			CHECK_MSG(instr.memory_segment() == tc.memory_segment, tc_msg(tc));
			CHECK_MSG(instr.memory_base() == tc.memory_base, tc_msg(tc));
			CHECK_MSG(instr.memory_index() == tc.memory_index, tc_msg(tc));
			CHECK_EQ(instr.memory_index_scale(), tc.memory_index_scale);
			CHECK_EQ(instr.memory_displacement32(), static_cast<std::uint32_t>(tc.memory_displacement));
			CHECK_MSG(instr.memory_displacement64() == tc.memory_displacement, tc_msg(tc));
			CHECK_EQ(instr.memory_displ_size(), tc.memory_displ_size);
			CHECK_MSG(instr.memory_size() == tc.memory_size, tc_msg(tc));
			break;
		}
	}
	if (tc.op_count >= 1) {
		CHECK_EQ(instr.op0_kind(), tc.op_kinds[0]);
		if (tc.op_kinds[0] == OpKind::Register)
			CHECK_EQ(instr.op0_register(), tc.op_registers[0]);
		if (tc.op_count >= 2) {
			CHECK_EQ(instr.op1_kind(), tc.op_kinds[1]);
			if (tc.op_kinds[1] == OpKind::Register)
				CHECK_EQ(instr.op1_register(), tc.op_registers[1]);
			if (tc.op_count >= 3) {
				CHECK_EQ(instr.op2_kind(), tc.op_kinds[2]);
				if (tc.op_kinds[2] == OpKind::Register)
					CHECK_EQ(instr.op2_register(), tc.op_registers[2]);
				if (tc.op_count >= 4) {
					CHECK_EQ(instr.op3_kind(), tc.op_kinds[3]);
					if (tc.op_kinds[3] == OpKind::Register)
						CHECK_EQ(instr.op3_register(), tc.op_registers[3]);
					if (tc.op_count >= 5) {
						CHECK_EQ(instr.op4_kind(), tc.op_kinds[4]);
						if (tc.op_kinds[4] == OpKind::Register)
							CHECK_EQ(instr.op4_register(), tc.op_registers[4]);
						static_assert(IcedConstants::MAX_OP_COUNT == 5, "");
						CHECK_EQ(tc.op_count, 5U);
					}
				}
			}
		}
	}
	verify_constant_offsets(tc.constant_offsets, decoder.get_constant_offsets(instr));
}

void decode(std::uint32_t bitness) {
	for (const auto& info : get_test_cases(bitness))
		decode_test(bitness, info);
}

void decode_misc(std::uint32_t bitness) {
	for (const auto& info : get_misc_test_cases(bitness))
		decode_test(bitness, info);
}

void decode_mem_test(std::uint32_t bitness, const DecoderMemoryTestCase& tc) {
	auto bytes = to_vec_u8(tc.hex_bytes);
	auto created = create_decoder(bitness, bytes, tc.ip, tc.decoder_options);
	Decoder& decoder = created.decoder;
	std::size_t len = created.len;
	CHECK_EQ(decoder.position(), 0U);
	CHECK_EQ(decoder.max_position(), bytes.size());
	Instruction instr = decoder.decode();
	CHECK_EQ(decoder.last_error(), DecoderError::None);
	CHECK_EQ(decoder.position(), len);
	CHECK_EQ(decoder.can_decode(), created.can_read);

	CHECK_MSG(instr.code() == tc.code, "line " + std::to_string(tc.line_number) + ": " + tc.hex_bytes);
	CHECK_EQ(instr.is_invalid(), tc.code == Code::INVALID);
	CHECK_EQ(instr.op_count(), 2U);
	CHECK_EQ(instr.len(), len);
	CHECK(!instr.has_rep_prefix());
	CHECK(!instr.has_repe_prefix());
	CHECK(!instr.has_repne_prefix());
	CHECK(!instr.has_lock_prefix());
	CHECK_EQ(instr.segment_prefix(), tc.prefix_segment);
	if (instr.segment_prefix() == Register::None)
		CHECK(!instr.has_segment_prefix());
	else
		CHECK(instr.has_segment_prefix());

	CHECK_EQ(instr.op0_kind(), OpKind::Memory);
	CHECK_EQ(instr.memory_segment(), tc.segment);
	CHECK_EQ(instr.memory_base(), tc.base_register);
	CHECK_EQ(instr.memory_index(), tc.index_register);
	CHECK_EQ(instr.memory_displacement32(), static_cast<std::uint32_t>(tc.displacement));
	CHECK_EQ(instr.memory_displacement64(), tc.displacement);
	CHECK_EQ(instr.memory_index_scale(), 1U << tc.scale);
	CHECK_EQ(instr.memory_displ_size(), tc.displ_size);

	CHECK_EQ(instr.op1_kind(), OpKind::Register);
	CHECK_EQ(instr.op1_register(), tc.register_);
	verify_constant_offsets(tc.constant_offsets, decoder.get_constant_offsets(instr));
}

void decode_mem(std::uint32_t bitness) {
	for (const auto& info : get_mem_test_cases(bitness))
		decode_mem_test(bitness, info);
}

} // namespace

TEST_CASE("decoder/decoder_new") {
	Decoder decoder(64, nullptr, 0, DecoderOptions::NONE);
	CHECK_EQ(decoder.ip(), 0U);
}

TEST_CASE("decoder/decoder_try_new") {
	auto decoder = Decoder::try_new(64, nullptr, 0, DecoderOptions::NONE);
	REQUIRE(decoder.is_ok());
	CHECK_EQ(decoder.value().ip(), 0U);
}

TEST_CASE("decoder/decoder_with_ip") {
	auto decoder = Decoder::with_ip(64, nullptr, 0, 0x1234'5678'9ABC'DEF1ULL, DecoderOptions::NONE);
	CHECK_EQ(decoder.ip(), 0x1234'5678'9ABC'DEF1ULL);
}

TEST_CASE("decoder/decoder_try_with_ip") {
	auto decoder = Decoder::try_with_ip(64, nullptr, 0, 0x1234'5678'9ABC'DEF1ULL, DecoderOptions::NONE);
	REQUIRE(decoder.is_ok());
	CHECK_EQ(decoder.value().ip(), 0x1234'5678'9ABC'DEF1ULL);
}

TEST_CASE("decoder/decode_16") { decode(16); }
TEST_CASE("decoder/decode_32") { decode(32); }
TEST_CASE("decoder/decode_64") { decode(64); }

TEST_CASE("decoder/decode_misc_16") { decode_misc(16); }
TEST_CASE("decoder/decode_misc_32") { decode_misc(32); }
TEST_CASE("decoder/decode_misc_64") { decode_misc(64); }

TEST_CASE("decoder/decode_mem_16") { decode_mem(16); }
TEST_CASE("decoder/decode_mem_32") { decode_mem(32); }
TEST_CASE("decoder/decode_mem_64") { decode_mem(64); }

TEST_CASE("decoder/make_sure_all_code_values_are_tested_in_16_32_64_bit_modes") {
	constexpr std::uint8_t T16 = 0x01;
	constexpr std::uint8_t T32 = 0x02;
	constexpr std::uint8_t T64 = 0x04;
	std::vector<std::uint8_t> tested(IcedConstants::CODE_ENUM_COUNT, 0);
	tested[static_cast<std::size_t>(Code::INVALID)] = T16 | T32 | T64;

	for (const auto& info : decoder_tests(false, false)) {
		CHECK(not_decoded().count(info.code()) == 0);

		std::uint8_t t;
		switch (info.bitness()) {
		case 16:
			t = T16;
			break;
		case 32:
			t = T32;
			break;
		case 64:
			t = T64;
			break;
		default:
			FAIL("Invalid bitness");
		}
		tested[static_cast<std::size_t>(info.code())] |= t;
	}

	// The Rust code uses the encoder's non-decoded tests if the encoder feature is enabled. It gives the same result.
	for (Code code : NON_DECODED_CODE_VALUES1632)
		tested[static_cast<std::size_t>(code)] |= T16 | T32;
	for (Code code : NON_DECODED_CODE_VALUES)
		tested[static_cast<std::size_t>(code)] |= T16 | T32 | T64;

	for (Code c : not_decoded()) {
		CHECK(code32_only().count(c) == 0);
		CHECK(code64_only().count(c) == 0);
	}

	for (Code c : not_decoded32_only())
		tested[static_cast<std::size_t>(c)] ^= T64;
	for (Code c : not_decoded64_only())
		tested[static_cast<std::size_t>(c)] ^= T16 | T32;

	for (Code c : code32_only()) {
		CHECK(code64_only().count(c) == 0);
		tested[static_cast<std::size_t>(c)] ^= T64;
	}

	for (Code c : code64_only()) {
		CHECK(code32_only().count(c) == 0);
		tested[static_cast<std::size_t>(c)] ^= T16 | T32;
	}

	std::string sb16;
	std::string sb32;
	std::string sb64;
	std::uint32_t missing16 = 0;
	std::uint32_t missing32 = 0;
	std::uint32_t missing64 = 0;
	auto names = code_names();
	for (std::size_t i = 0; i < tested.size(); i++) {
		if (tested[i] != (T16 | T32 | T64) && !is_ignored_code(names[i])) {
			if ((tested[i] & T16) == 0) {
				sb16 += std::string(names[i]) + " ";
				missing16++;
			}
			if ((tested[i] & T32) == 0) {
				sb32 += std::string(names[i]) + " ";
				missing32++;
			}
			if ((tested[i] & T64) == 0) {
				sb64 += std::string(names[i]) + " ";
				missing64++;
			}
		}
	}
	CHECK_EQ("16: " + std::to_string(missing16) + " ins " + sb16, std::string("16: 0 ins "));
	CHECK_EQ("32: " + std::to_string(missing32) + " ins " + sb32, std::string("32: 0 ins "));
	CHECK_EQ("64: " + std::to_string(missing64) + " ins " + sb64, std::string("64: 0 ins "));
}

} // namespace iced_x86::tests
