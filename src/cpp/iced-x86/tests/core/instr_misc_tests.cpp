// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of src/rust/iced-x86/src/test/instr_misc.rs

#include "test_framework.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/instruction.hpp"

#include <cstdint>
#include <limits>

using namespace iced_x86;

namespace {
template <typename T>
constexpr T enum_value(std::size_t value) {
	return static_cast<T>(value);
}
} // namespace

TEST_CASE("core/instr_misc/invalid_code_value_is_zero") {
	// A 'default' Instruction should be an invalid instruction
	static_assert(static_cast<std::uint32_t>(Code::INVALID) == 0, "");
	Instruction instr1;
	CHECK(instr1.code() == Code::INVALID);
	Instruction instr2 = Instruction();
	CHECK(instr2.code() == Code::INVALID);
	CHECK(instr1.eq_all_bits(instr2));
}

TEST_CASE("core/instr_misc/write_all_properties") {
	Instruction instr;

	instr.set_ip(0x8A6B'D04A'9B68'3A92ULL);
	instr.set_ip16(std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.ip16(), std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.ip32(), static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::min()));
	CHECK_EQ(instr.ip(), static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::min()));
	instr.set_ip(0x8A6B'D04A'9B68'3A92ULL);
	instr.set_ip16(std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.ip16(), std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.ip32(), static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max()));
	CHECK_EQ(instr.ip(), static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()));

	instr.set_ip(0x8A6B'D04A'9B68'3A92ULL);
	instr.set_ip32(std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.ip16(), std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.ip32(), std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.ip(), static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::min()));
	instr.set_ip(0x8A6B'D04A'9B68'3A92ULL);
	instr.set_ip32(std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.ip16(), std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.ip32(), std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.ip(), static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));

	instr.set_ip(std::numeric_limits<std::uint64_t>::min());
	CHECK_EQ(instr.ip16(), std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.ip32(), std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.ip(), std::numeric_limits<std::uint64_t>::min());
	instr.set_ip(std::numeric_limits<std::uint64_t>::max());
	CHECK_EQ(instr.ip16(), std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.ip32(), std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.ip(), std::numeric_limits<std::uint64_t>::max());

	instr.set_next_ip(0x8A6B'D04A'9B68'3A92ULL);
	instr.set_next_ip16(std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.next_ip16(), std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.next_ip32(), static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::min()));
	CHECK_EQ(instr.next_ip(), static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::min()));
	instr.set_next_ip(0x8A6B'D04A'9B68'3A92ULL);
	instr.set_next_ip16(std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.next_ip16(), std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.next_ip32(), static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max()));
	CHECK_EQ(instr.next_ip(), static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()));

	instr.set_next_ip(0x8A6B'D04A'9B68'3A92ULL);
	instr.set_next_ip32(std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.next_ip16(), std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.next_ip32(), std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.next_ip(), static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::min()));
	instr.set_next_ip(0x8A6B'D04A'9B68'3A92ULL);
	instr.set_next_ip32(std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.next_ip16(), std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.next_ip32(), std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.next_ip(), static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));

	instr.set_next_ip(std::numeric_limits<std::uint64_t>::min());
	CHECK_EQ(instr.next_ip16(), std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.next_ip32(), std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.next_ip(), std::numeric_limits<std::uint64_t>::min());
	instr.set_next_ip(std::numeric_limits<std::uint64_t>::max());
	CHECK_EQ(instr.next_ip16(), std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.next_ip32(), std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.next_ip(), std::numeric_limits<std::uint64_t>::max());

	instr.set_memory_displacement32(std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.memory_displacement32(), std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.memory_displacement64(), static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::min()));
	instr.set_memory_displacement32(std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.memory_displacement32(), std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.memory_displacement64(), static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));

	instr.set_memory_displacement64(std::numeric_limits<std::uint64_t>::min());
	CHECK_EQ(instr.memory_displacement32(), std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.memory_displacement64(), std::numeric_limits<std::uint64_t>::min());
	instr.set_memory_displacement64(std::numeric_limits<std::uint64_t>::max());
	CHECK_EQ(instr.memory_displacement32(), std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.memory_displacement64(), std::numeric_limits<std::uint64_t>::max());

	instr.set_memory_displacement64(0x1234'5678'9ABC'DEF1ULL);
	instr.set_memory_displacement32(0x5AA5'4321U);
	CHECK_EQ(instr.memory_displacement32(), 0x5AA5'4321U);
	CHECK_EQ(instr.memory_displacement64(), 0x5AA5'4321ULL);

	instr.set_immediate8(std::numeric_limits<std::uint8_t>::min());
	CHECK_EQ(instr.immediate8(), std::numeric_limits<std::uint8_t>::min());
	instr.set_immediate8(std::numeric_limits<std::uint8_t>::max());
	CHECK_EQ(instr.immediate8(), std::numeric_limits<std::uint8_t>::max());

	instr.set_immediate8_2nd(std::numeric_limits<std::uint8_t>::min());
	CHECK_EQ(instr.immediate8_2nd(), std::numeric_limits<std::uint8_t>::min());
	instr.set_immediate8_2nd(std::numeric_limits<std::uint8_t>::max());
	CHECK_EQ(instr.immediate8_2nd(), std::numeric_limits<std::uint8_t>::max());

	instr.set_immediate16(std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.immediate16(), std::numeric_limits<std::uint16_t>::min());
	instr.set_immediate16(std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.immediate16(), std::numeric_limits<std::uint16_t>::max());

	instr.set_immediate32(std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.immediate32(), std::numeric_limits<std::uint32_t>::min());
	instr.set_immediate32(std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.immediate32(), std::numeric_limits<std::uint32_t>::max());

	instr.set_immediate64(std::numeric_limits<std::uint64_t>::min());
	CHECK_EQ(instr.immediate64(), std::numeric_limits<std::uint64_t>::min());
	instr.set_immediate64(std::numeric_limits<std::uint64_t>::max());
	CHECK_EQ(instr.immediate64(), std::numeric_limits<std::uint64_t>::max());

	instr.set_immediate8to16(std::numeric_limits<std::int8_t>::min());
	CHECK_EQ(instr.immediate8to16(), static_cast<std::int16_t>(std::numeric_limits<std::int8_t>::min()));
	instr.set_immediate8to16(std::numeric_limits<std::int8_t>::max());
	CHECK_EQ(instr.immediate8to16(), static_cast<std::int16_t>(std::numeric_limits<std::int8_t>::max()));

	instr.set_immediate8to32(std::numeric_limits<std::int8_t>::min());
	CHECK_EQ(instr.immediate8to32(), static_cast<std::int32_t>(std::numeric_limits<std::int8_t>::min()));
	instr.set_immediate8to32(std::numeric_limits<std::int8_t>::max());
	CHECK_EQ(instr.immediate8to32(), static_cast<std::int32_t>(std::numeric_limits<std::int8_t>::max()));

	instr.set_immediate8to64(std::numeric_limits<std::int8_t>::min());
	CHECK_EQ(instr.immediate8to64(), static_cast<std::int64_t>(std::numeric_limits<std::int8_t>::min()));
	instr.set_immediate8to64(std::numeric_limits<std::int8_t>::max());
	CHECK_EQ(instr.immediate8to64(), static_cast<std::int64_t>(std::numeric_limits<std::int8_t>::max()));

	instr.set_immediate32to64(std::numeric_limits<std::int32_t>::min());
	CHECK_EQ(instr.immediate32to64(), static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()));
	instr.set_immediate32to64(std::numeric_limits<std::int32_t>::max());
	CHECK_EQ(instr.immediate32to64(), static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()));

	instr.set_op0_kind(OpKind::NearBranch16);
	instr.set_near_branch16(std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.near_branch16(), std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.near_branch_target(), static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::min()));
	instr.set_near_branch16(std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.near_branch16(), std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.near_branch_target(), static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()));

	instr.set_op0_kind(OpKind::NearBranch32);
	instr.set_near_branch32(std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.near_branch32(), std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.near_branch_target(), static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::min()));
	instr.set_near_branch32(std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.near_branch32(), std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.near_branch_target(), static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));

	instr.set_op0_kind(OpKind::NearBranch64);
	instr.set_near_branch64(std::numeric_limits<std::uint64_t>::min());
	CHECK_EQ(instr.near_branch64(), std::numeric_limits<std::uint64_t>::min());
	CHECK_EQ(instr.near_branch_target(), std::numeric_limits<std::uint64_t>::min());
	instr.set_near_branch64(std::numeric_limits<std::uint64_t>::max());
	CHECK_EQ(instr.near_branch64(), std::numeric_limits<std::uint64_t>::max());
	CHECK_EQ(instr.near_branch_target(), std::numeric_limits<std::uint64_t>::max());

	instr.set_far_branch16(std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.far_branch16(), std::numeric_limits<std::uint16_t>::min());
	instr.set_far_branch16(std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.far_branch16(), std::numeric_limits<std::uint16_t>::max());

	instr.set_far_branch32(std::numeric_limits<std::uint32_t>::min());
	CHECK_EQ(instr.far_branch32(), std::numeric_limits<std::uint32_t>::min());
	instr.set_far_branch32(std::numeric_limits<std::uint32_t>::max());
	CHECK_EQ(instr.far_branch32(), std::numeric_limits<std::uint32_t>::max());

	instr.set_far_branch_selector(std::numeric_limits<std::uint16_t>::min());
	CHECK_EQ(instr.far_branch_selector(), std::numeric_limits<std::uint16_t>::min());
	instr.set_far_branch_selector(std::numeric_limits<std::uint16_t>::max());
	CHECK_EQ(instr.far_branch_selector(), std::numeric_limits<std::uint16_t>::max());

	{
		Instruction instr2 = instr;
		instr2.set_code(Code::Cmpxchg8b_m64);
		instr2.set_op0_kind(OpKind::Memory);
		instr2.set_has_lock_prefix(true);

		instr2.set_has_xacquire_prefix(false);
		CHECK(!instr2.has_xacquire_prefix());
		instr2.set_has_xacquire_prefix(true);
		CHECK(instr2.has_xacquire_prefix());

		instr2.set_has_xrelease_prefix(false);
		CHECK(!instr2.has_xrelease_prefix());
		instr2.set_has_xrelease_prefix(true);
		CHECK(instr2.has_xrelease_prefix());
	}

	instr.set_has_rep_prefix(false);
	CHECK(!instr.has_rep_prefix());
	CHECK(!instr.has_repe_prefix());
	instr.set_has_rep_prefix(true);
	CHECK(instr.has_rep_prefix());
	CHECK(instr.has_repe_prefix());

	instr.set_has_repe_prefix(false);
	CHECK(!instr.has_rep_prefix());
	CHECK(!instr.has_repe_prefix());
	instr.set_has_repe_prefix(true);
	CHECK(instr.has_rep_prefix());
	CHECK(instr.has_repe_prefix());

	instr.set_has_repne_prefix(false);
	CHECK(!instr.has_repne_prefix());
	instr.set_has_repne_prefix(true);
	CHECK(instr.has_repne_prefix());

	instr.set_has_lock_prefix(false);
	CHECK(!instr.has_lock_prefix());
	instr.set_has_lock_prefix(true);
	CHECK(instr.has_lock_prefix());

	instr.set_is_broadcast(false);
	CHECK(!instr.is_broadcast());
	instr.set_is_broadcast(true);
	CHECK(instr.is_broadcast());

	instr.set_suppress_all_exceptions(false);
	CHECK(!instr.suppress_all_exceptions());
	instr.set_suppress_all_exceptions(true);
	CHECK(instr.suppress_all_exceptions());

	for (std::size_t i = 0; i < IcedConstants::MAX_INSTRUCTION_LENGTH + 1; i++) {
		instr.set_len(i);
		CHECK_EQ(instr.len(), i);
	}

	for (std::size_t i = 0; i < IcedConstants::CODE_SIZE_ENUM_COUNT; i++) {
		const auto code_size = enum_value<CodeSize>(i);
		instr.set_code_size(code_size);
		CHECK(instr.code_size() == code_size);
	}

	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const auto code = enum_value<Code>(i);
		instr.set_code(code);
		CHECK(instr.code() == code);
	}

	static_assert(IcedConstants::MAX_OP_COUNT == 5, "");
	for (std::size_t i = 0; i < IcedConstants::OP_KIND_ENUM_COUNT; i++) {
		const auto op_kind = enum_value<OpKind>(i);
		instr.set_op0_kind(op_kind);
		CHECK(instr.op0_kind() == op_kind);
	}

	for (std::size_t i = 0; i < IcedConstants::OP_KIND_ENUM_COUNT; i++) {
		const auto op_kind = enum_value<OpKind>(i);
		instr.set_op1_kind(op_kind);
		CHECK(instr.op1_kind() == op_kind);
	}

	for (std::size_t i = 0; i < IcedConstants::OP_KIND_ENUM_COUNT; i++) {
		const auto op_kind = enum_value<OpKind>(i);
		instr.set_op2_kind(op_kind);
		CHECK(instr.op2_kind() == op_kind);
	}

	for (std::size_t i = 0; i < IcedConstants::OP_KIND_ENUM_COUNT; i++) {
		const auto op_kind = enum_value<OpKind>(i);
		instr.set_op3_kind(op_kind);
		CHECK(instr.op3_kind() == op_kind);
	}

	for (std::size_t i = 0; i < IcedConstants::OP_KIND_ENUM_COUNT; i++) {
		const auto op_kind = enum_value<OpKind>(i);
		if (op_kind == OpKind::Immediate8) {
			instr.set_op4_kind(op_kind);
			CHECK(instr.try_set_op4_kind(op_kind).is_ok());
			CHECK(instr.op4_kind() == op_kind);
		}
		else {
			Instruction instr2 = instr;
#ifdef NDEBUG
			// Debug builds abort (debug assert)
			instr2.set_op4_kind(op_kind);
#endif
			CHECK(instr2.try_set_op4_kind(op_kind).is_err());
		}
	}

	for (std::uint32_t operand = 0; operand < 4; operand++) {
		for (std::size_t i = 0; i < IcedConstants::OP_KIND_ENUM_COUNT; i++) {
			const auto op_kind = enum_value<OpKind>(i);
			instr.set_op_kind(operand, op_kind);
			CHECK(instr.try_set_op_kind(operand, op_kind).is_ok());
			switch (operand) {
			case 0:
				CHECK(instr.op0_kind() == op_kind);
				break;
			case 1:
				CHECK(instr.op1_kind() == op_kind);
				break;
			case 2:
				CHECK(instr.op2_kind() == op_kind);
				break;
			default:
				CHECK(instr.op3_kind() == op_kind);
				break;
			}
			CHECK(instr.op_kind(operand) == op_kind);
			CHECK(instr.try_op_kind(operand).value() == op_kind);
		}
	}

	for (std::size_t i = 0; i < IcedConstants::OP_KIND_ENUM_COUNT; i++) {
		const auto op_kind = enum_value<OpKind>(i);
		if (op_kind == OpKind::Immediate8) {
			instr.set_op_kind(4, op_kind);
			CHECK(instr.try_set_op_kind(4, op_kind).is_ok());
			CHECK(instr.op4_kind() == op_kind);
			CHECK(instr.op_kind(4) == op_kind);
			CHECK(instr.try_op_kind(4).value() == op_kind);
		}
		else {
			Instruction instr2 = instr;
			CHECK(instr2.try_set_op_kind(4, op_kind).is_err());
#ifdef NDEBUG
			instr2.set_op_kind(4, op_kind);
#endif
		}
	}
	CHECK(instr.try_op_kind(5).is_err());
	CHECK(instr.try_set_op_kind(5, OpKind::Register).is_err());

	const Register seg_values[] = {Register::ES, Register::CS, Register::SS, Register::DS, Register::FS, Register::GS, Register::None};
	for (Register seg : seg_values) {
		instr.set_segment_prefix(seg);
		CHECK(instr.segment_prefix() == seg);
		if (instr.segment_prefix() == Register::None)
			CHECK(!instr.has_segment_prefix());
		else
			CHECK(instr.has_segment_prefix());
	}

	const std::uint32_t displ_sizes[] = {8, 4, 2, 1, 0};
	for (std::uint32_t displ_size : displ_sizes) {
		instr.set_memory_displ_size(displ_size);
		CHECK_EQ(instr.memory_displ_size(), displ_size);
	}

	const std::uint32_t scale_values[] = {8, 4, 2, 1};
	for (std::uint32_t scale_value : scale_values) {
		instr.set_memory_index_scale(scale_value);
		CHECK_EQ(instr.memory_index_scale(), scale_value);
	}

	for (std::size_t i = 0; i < IcedConstants::REGISTER_ENUM_COUNT; i++) {
		const auto reg = enum_value<Register>(i);
		instr.set_memory_base(reg);
		CHECK(instr.memory_base() == reg);
	}

	for (std::size_t i = 0; i < IcedConstants::REGISTER_ENUM_COUNT; i++) {
		const auto reg = enum_value<Register>(i);
		instr.set_memory_index(reg);
		CHECK(instr.memory_index() == reg);
	}

	for (std::size_t i = 0; i < IcedConstants::REGISTER_ENUM_COUNT; i++) {
		const auto reg = enum_value<Register>(i);
		instr.set_op0_register(reg);
		CHECK(instr.op0_register() == reg);
		instr.set_op1_register(reg);
		CHECK(instr.op1_register() == reg);
		instr.set_op2_register(reg);
		CHECK(instr.op2_register() == reg);
		instr.set_op3_register(reg);
		CHECK(instr.op3_register() == reg);
	}

	for (std::size_t i = 0; i < IcedConstants::REGISTER_ENUM_COUNT; i++) {
		const auto reg = enum_value<Register>(i);
		if (reg == Register::None) {
			instr.set_op4_register(reg);
			CHECK(instr.try_set_op4_register(reg).is_ok());
			CHECK(instr.op4_register() == reg);
		}
		else {
			CHECK(instr.try_set_op4_register(reg).is_err());
#ifdef NDEBUG
			Instruction instr2 = instr;
			instr2.set_op4_register(reg);
#endif
		}
	}

	for (std::uint32_t operand = 0; operand < 4; operand++) {
		for (std::size_t i = 0; i < IcedConstants::REGISTER_ENUM_COUNT; i++) {
			const auto reg = enum_value<Register>(i);
			instr.set_op_register(operand, reg);
			CHECK(instr.try_set_op_register(operand, reg).is_ok());
			switch (operand) {
			case 0:
				CHECK(instr.op0_register() == reg);
				break;
			case 1:
				CHECK(instr.op1_register() == reg);
				break;
			case 2:
				CHECK(instr.op2_register() == reg);
				break;
			default:
				CHECK(instr.op3_register() == reg);
				break;
			}
			CHECK(instr.op_register(operand) == reg);
			CHECK(instr.try_op_register(operand).value() == reg);
		}
	}

	for (std::size_t i = 0; i < IcedConstants::REGISTER_ENUM_COUNT; i++) {
		const auto reg = enum_value<Register>(i);
		if (reg == Register::None) {
			instr.set_op_register(4, reg);
			CHECK(instr.try_set_op_register(4, reg).is_ok());
			CHECK(instr.op4_register() == reg);
			CHECK(instr.op_register(4) == reg);
			CHECK(instr.try_op_register(4).value() == reg);
		}
		else {
			CHECK(instr.try_set_op_register(4, reg).is_err());
#ifdef NDEBUG
			Instruction instr2 = instr;
			instr2.set_op_register(4, reg);
#endif
		}
	}
	CHECK(instr.try_op_register(5).is_err());
	CHECK(instr.try_set_op_register(5, Register::None).is_err());

	const Register op_masks[] = {Register::K1, Register::K2, Register::K3, Register::K4, Register::K5, Register::K6, Register::K7, Register::None};
	for (Register op_mask : op_masks) {
		instr.set_op_mask(op_mask);
		CHECK(instr.op_mask() == op_mask);
		CHECK_EQ(instr.has_op_mask(), op_mask != Register::None);
	}

	instr.set_zeroing_masking(false);
	CHECK(!instr.zeroing_masking());
	CHECK(instr.merging_masking());
	instr.set_zeroing_masking(true);
	CHECK(instr.zeroing_masking());
	CHECK(!instr.merging_masking());
	instr.set_merging_masking(false);
	CHECK(!instr.merging_masking());
	CHECK(instr.zeroing_masking());
	instr.set_merging_masking(true);
	CHECK(instr.merging_masking());
	CHECK(!instr.zeroing_masking());

	for (std::size_t i = 0; i < IcedConstants::ROUNDING_CONTROL_ENUM_COUNT; i++) {
		const auto rc = enum_value<RoundingControl>(i);
		instr.set_rounding_control(rc);
		CHECK(instr.rounding_control() == rc);
	}

	for (std::size_t i = 0; i < IcedConstants::REGISTER_ENUM_COUNT; i++) {
		const auto reg = enum_value<Register>(i);
		instr.set_memory_base(reg);
		CHECK_EQ(instr.is_ip_rel_memory_operand(), reg == Register::RIP || reg == Register::EIP);
	}

	instr.set_memory_base(Register::EIP);
	instr.set_next_ip(0x1234'5670'9EDC'BA98ULL);
	instr.set_memory_displacement64(0x8765'4321'9ABC'DEF5ULL);
	CHECK(instr.is_ip_rel_memory_operand());
	CHECK_EQ(instr.ip_rel_memory_address(), 0x9ABC'DEF5ULL);

	instr.set_memory_base(Register::RIP);
	instr.set_next_ip(0x1234'5670'9EDC'BA98ULL);
	instr.set_memory_displacement64(0x8765'4321'9ABC'DEF5ULL);
	CHECK(instr.is_ip_rel_memory_operand());
	CHECK_EQ(instr.ip_rel_memory_address(), 0x8765'4321'9ABC'DEF5ULL);

	instr.set_declare_data_len(1);
	CHECK_EQ(instr.declare_data_len(), std::size_t{1});
	instr.set_declare_data_len(15);
	CHECK_EQ(instr.declare_data_len(), std::size_t{15});
	instr.set_declare_data_len(16);
	CHECK_EQ(instr.declare_data_len(), std::size_t{16});
}

TEST_CASE("core/instr_misc/verify_get_set_immediate") {
	Instruction instr;

	instr.set_code(Code::Add_AL_imm8);
	instr.set_op1_kind(OpKind::Immediate8);
	instr.set_immediate_i32(1, 0x5A);
	CHECK(instr.try_set_immediate_i32(1, 0x5A).is_ok());
	CHECK_EQ(instr.immediate(1), 0x5AULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0x5AULL);
	instr.set_immediate_i32(1, 0xA5);
	CHECK(instr.try_set_immediate_i32(1, 0xA5).is_ok());
	CHECK_EQ(instr.immediate(1), 0xA5ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xA5ULL);

	instr.set_code(Code::Add_AX_imm16);
	instr.set_op1_kind(OpKind::Immediate16);
	instr.set_immediate_i32(1, 0x5AA5);
	CHECK(instr.try_set_immediate_i32(1, 0x5AA5).is_ok());
	CHECK_EQ(instr.immediate(1), 0x5AA5ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0x5AA5ULL);
	instr.set_immediate_i32(1, 0xA55A);
	CHECK(instr.try_set_immediate_i32(1, 0xA55A).is_ok());
	CHECK_EQ(instr.immediate(1), 0xA55AULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xA55AULL);

	instr.set_code(Code::Add_EAX_imm32);
	instr.set_op1_kind(OpKind::Immediate32);
	instr.set_immediate_i32(1, 0x5AA5'1234);
	CHECK(instr.try_set_immediate_i32(1, 0x5AA5'1234).is_ok());
	CHECK_EQ(instr.immediate(1), 0x5AA5'1234ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0x5AA5'1234ULL);
	instr.set_immediate_u32(1, 0xA54A'1234U);
	CHECK(instr.try_set_immediate_u32(1, 0xA54A'1234U).is_ok());
	CHECK_EQ(instr.immediate(1), 0xA54A'1234ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xA54A'1234ULL);

	instr.set_code(Code::Add_RAX_imm32);
	instr.set_op1_kind(OpKind::Immediate32to64);
	instr.set_immediate_i32(1, 0x5AA5'1234);
	CHECK(instr.try_set_immediate_i32(1, 0x5AA5'1234).is_ok());
	CHECK_EQ(instr.immediate(1), 0x5AA5'1234ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0x5AA5'1234ULL);
	instr.set_immediate_u32(1, 0xA54A'1234U);
	CHECK(instr.try_set_immediate_u32(1, 0xA54A'1234U).is_ok());
	CHECK_EQ(instr.immediate(1), 0xFFFF'FFFF'A54A'1234ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xFFFF'FFFF'A54A'1234ULL);

	instr.set_code(Code::Enterq_imm16_imm8);
	instr.set_op1_kind(OpKind::Immediate8_2nd);
	instr.set_immediate_i32(1, 0x5A);
	CHECK(instr.try_set_immediate_i32(1, 0x5A).is_ok());
	CHECK_EQ(instr.immediate(1), 0x5AULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0x5AULL);
	instr.set_immediate_i32(1, 0xA5);
	CHECK(instr.try_set_immediate_i32(1, 0xA5).is_ok());
	CHECK_EQ(instr.immediate(1), 0xA5ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xA5ULL);

	instr.set_code(Code::Adc_rm16_imm8);
	instr.set_op1_kind(OpKind::Immediate8to16);
	instr.set_immediate_i32(1, 0x5A);
	CHECK(instr.try_set_immediate_i32(1, 0x5A).is_ok());
	CHECK_EQ(instr.immediate(1), 0x5AULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0x5AULL);
	instr.set_immediate_i32(1, 0xA5);
	CHECK(instr.try_set_immediate_i32(1, 0xA5).is_ok());
	CHECK_EQ(instr.immediate(1), 0xFFFF'FFFF'FFFF'FFA5ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xFFFF'FFFF'FFFF'FFA5ULL);

	instr.set_code(Code::Adc_rm32_imm8);
	instr.set_op1_kind(OpKind::Immediate8to32);
	instr.set_immediate_i32(1, 0x5A);
	CHECK(instr.try_set_immediate_i32(1, 0x5A).is_ok());
	CHECK_EQ(instr.immediate(1), 0x5AULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0x5AULL);
	instr.set_immediate_i32(1, 0xA5);
	CHECK(instr.try_set_immediate_i32(1, 0xA5).is_ok());
	CHECK_EQ(instr.immediate(1), 0xFFFF'FFFF'FFFF'FFA5ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xFFFF'FFFF'FFFF'FFA5ULL);

	instr.set_code(Code::Adc_rm64_imm8);
	instr.set_op1_kind(OpKind::Immediate8to64);
	instr.set_immediate_i32(1, 0x5A);
	CHECK(instr.try_set_immediate_i32(1, 0x5A).is_ok());
	CHECK_EQ(instr.immediate(1), 0x5AULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0x5AULL);
	instr.set_immediate_i32(1, 0xA5);
	CHECK(instr.try_set_immediate_i32(1, 0xA5).is_ok());
	CHECK_EQ(instr.immediate(1), 0xFFFF'FFFF'FFFF'FFA5ULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xFFFF'FFFF'FFFF'FFA5ULL);

	instr.set_code(Code::Mov_r64_imm64);
	instr.set_op1_kind(OpKind::Immediate64);
	instr.set_immediate_i64(1, 0x5AA5'1234'5678'9ABCLL);
	CHECK(instr.try_set_immediate_i64(1, 0x5AA5'1234'5678'9ABCLL).is_ok());
	CHECK_EQ(instr.immediate(1), 0x5AA5'1234'5678'9ABCULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0x5AA5'1234'5678'9ABCULL);
	instr.set_immediate_u64(1, 0xA54A'1234'5678'9ABCULL);
	CHECK(instr.try_set_immediate_u64(1, 0xA54A'1234'5678'9ABCULL).is_ok());
	CHECK_EQ(instr.immediate(1), 0xA54A'1234'5678'9ABCULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xA54A'1234'5678'9ABCULL);
	instr.set_immediate_i64(1, -0x5AB5'EDCB'A987'6544LL);
	CHECK(instr.try_set_immediate_i64(1, -0x5AB5'EDCB'A987'6544LL).is_ok());
	CHECK_EQ(instr.immediate(1), 0xA54A'1234'5678'9ABCULL);
	CHECK_EQ(instr.try_immediate(1).value(), 0xA54A'1234'5678'9ABCULL);

	CHECK(instr.try_immediate(0).is_err());
	CHECK(instr.try_set_immediate_i32(0, 0).is_err());
	CHECK(instr.try_set_immediate_u32(0, 0).is_err());
	CHECK(instr.try_set_immediate_i64(0, 0).is_err());
	CHECK(instr.try_set_immediate_u64(0, 0).is_err());
	CHECK(instr.try_immediate(5).is_err());
	CHECK(instr.try_set_immediate_u64(5, 0).is_err());
#ifdef NDEBUG
	// Debug builds abort (debug assert)
	{
		Instruction instr2 = instr;
		CHECK_EQ(instr2.immediate(0), 0ULL);
		instr2.set_immediate_i32(0, 0);
		instr2.set_immediate_u32(0, 0);
		instr2.set_immediate_i64(0, 0);
		instr2.set_immediate_u64(0, 0);
		CHECK(instr2.eq_all_bits(instr));
	}
#endif
}

TEST_CASE("core/instr_misc/verify_instruction_size") {
	static_assert(sizeof(Instruction) == 40, "");
	static_assert(alignof(Instruction) == 8, "");
	CHECK_EQ(sizeof(Instruction), std::size_t{40});
}
