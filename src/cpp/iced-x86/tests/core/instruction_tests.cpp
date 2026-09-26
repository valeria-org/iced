// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Instruction tests that don't need the decoder/encoder

#include "test_framework.hpp"
#include "iced_x86/constant_offsets.hpp"
#include "iced_x86/iced_features.hpp"
#include "iced_x86/iced_x86.hpp"
#include "internal/code_internal.hpp"
#include "internal/data_reader.hpp"
#include "internal/instruction_internal.hpp"
#include "internal/mvex/mvex.hpp"
#include "internal/tuple_type_tbl.hpp"

#include <cstdint>
#include <optional>
#include <unordered_set>

using namespace iced_x86;
using internal::InstructionInternal;

namespace {
// add [rdi+r12*8-5AA5EDCCh],esi
Instruction create_add_mem_reg() {
	Instruction instr;
	instr.set_code(Code::Add_rm32_r32);
	instr.set_code_size(CodeSize::Code64);
	instr.set_op0_kind(OpKind::Memory);
	instr.set_memory_base(Register::RDI);
	instr.set_memory_index(Register::R12);
	instr.set_memory_index_scale(8);
	instr.set_memory_displacement64(static_cast<std::uint64_t>(-0x5AA5'EDCCLL));
	instr.set_memory_displ_size(4);
	instr.set_op1_kind(OpKind::Register);
	instr.set_op1_register(Register::ESI);
	instr.set_len(8);
	instr.set_next_ip(0x1000);
	return instr;
}
} // namespace

TEST_CASE("core/instruction/op_kinds") {
	const Instruction instr = create_add_mem_reg();
	CHECK_EQ(instr.op_count(), 2U);
	const auto op_kinds = instr.op_kinds();
	REQUIRE_EQ(op_kinds.size(), std::size_t{2});
	CHECK(op_kinds[0] == OpKind::Memory);
	CHECK(op_kinds[1] == OpKind::Register);
	std::size_t count = 0;
	for (OpKind op_kind : op_kinds) {
		CHECK(op_kind == instr.op_kind(static_cast<std::uint32_t>(count)));
		count++;
	}
	CHECK_EQ(count, std::size_t{2});
	CHECK(instr.mnemonic() == Mnemonic::Add);
	CHECK(instr.memory_size() == MemorySize::UInt32);
	CHECK(instr.memory_segment() == Register::DS);
	CHECK(!instr.is_vsib());
	CHECK(!instr.vsib().has_value());
	CHECK_EQ(instr.ip(), 0x1000ULL - 8);
}

TEST_CASE("core/instruction/eq_and_hash_ignore_some_fields") {
	Instruction instr1 = create_add_mem_reg();
	Instruction instr2 = instr1;
	CHECK(instr1.eq_all_bits(instr2));
	CHECK(instr1 == instr2);
	instr1.set_code_size(CodeSize::Code32);
	instr2.set_code_size(CodeSize::Code64);
	CHECK(!instr1.eq_all_bits(instr2));
	instr1.set_len(10);
	instr2.set_len(5);
	instr1.set_ip(0x9733'3795'FA7C'EAABULL);
	instr2.set_ip(0x9BE5'A3A0'7A66'FC05ULL);
	CHECK(instr1 == instr2);
	CHECK(!(instr1 != instr2));
	CHECK_EQ(std::hash<Instruction>()(instr1), std::hash<Instruction>()(instr2));
	instr2.set_op1_register(Register::EDI);
	CHECK(instr1 != instr2);
	std::unordered_set<Instruction> set;
	set.insert(instr1);
	set.insert(instr1);
	set.insert(instr2);
	CHECK_EQ(set.size(), std::size_t{2});
}

TEST_CASE("core/instruction/virtual_address") {
	const Instruction instr = create_add_mem_reg();
	auto get_reg = [](Register reg, std::size_t element_index, std::size_t element_size) -> std::optional<std::uint64_t> {
		CHECK_EQ(element_index, std::size_t{0});
		CHECK_EQ(element_size, std::size_t{0});
		switch (reg) {
		// The base address of ES, CS, SS and DS is always 0 in 64-bit mode
		case Register::DS:
			return 0x0000'0000'0000'0000ULL;
		case Register::RDI:
			return 0x0000'0000'1000'0000ULL;
		case Register::R12:
			return 0x0000'0004'0000'0000ULL;
		default:
			return std::nullopt;
		}
	};
	CHECK(instr.virtual_address(0, 0, get_reg) == std::optional<std::uint64_t>(0x0000'001F'B55A'1234ULL));
	CHECK(instr.try_virtual_address(0, 0, get_reg) == std::optional<std::uint64_t>(0x0000'001F'B55A'1234ULL));
	CHECK(instr.virtual_address(1, 0, get_reg) == std::optional<std::uint64_t>(0));
	CHECK(!instr.virtual_address(0, 0, [](Register, std::size_t, std::size_t) -> std::optional<std::uint64_t> { return std::nullopt; }).has_value());

	// Function pointer + context
	struct Ctx {
		std::uint64_t rdi;
		int calls;
	} ctx{0x1234, 0};
	const auto fn = [](void* context, Register reg, std::size_t, std::size_t) -> std::optional<std::uint64_t> {
		auto* c = static_cast<Ctx*>(context);
		c->calls++;
		if (reg == Register::RDI)
			return c->rdi;
		if (reg == Register::R12 || reg == Register::DS)
			return 0;
		return std::nullopt;
	};
	CHECK(instr.virtual_address(0, 0, fn, &ctx) == std::optional<std::uint64_t>(0x1234 + static_cast<std::uint64_t>(-0x5AA5'EDCCLL)));
	CHECK_EQ(ctx.calls, 3);

	// 16-bit address size masks the result
	Instruction instr16;
	instr16.set_code(Code::Add_rm16_r16);
	instr16.set_code_size(CodeSize::Code16);
	instr16.set_op0_kind(OpKind::Memory);
	instr16.set_memory_base(Register::BX);
	instr16.set_memory_displacement64(0xFFFF);
	instr16.set_memory_displ_size(2);
	auto va16 = instr16.virtual_address(0, 0, [](Register reg, std::size_t, std::size_t) -> std::optional<std::uint64_t> {
		if (reg == Register::BX)
			return 2;
		if (reg == Register::DS)
			return 0x10000;
		return std::nullopt;
	});
	CHECK(va16 == std::optional<std::uint64_t>(0x10001));

	// String instructions
	Instruction movs;
	movs.set_code(Code::Movsb_m8_m8);
	movs.set_op0_kind(OpKind::MemoryESRDI);
	movs.set_op1_kind(OpKind::MemorySegESI);
	auto get_str = [](Register reg, std::size_t, std::size_t) -> std::optional<std::uint64_t> {
		switch (reg) {
		case Register::ES:
			return 0x100;
		case Register::DS:
			return 0x200;
		case Register::RDI:
			return 0x1'0000'0010ULL;
		case Register::ESI:
			return 0xFFFF'FFFF'0000'0020ULL;
		default:
			return std::nullopt;
		}
	};
	CHECK(movs.virtual_address(0, 0, get_str) == std::optional<std::uint64_t>(0x1'0000'0110ULL));
	CHECK(movs.virtual_address(1, 0, get_str) == std::optional<std::uint64_t>(0x220));
}

TEST_CASE("core/instruction/vsib") {
	Instruction instr;
	instr.set_code(Code::VEX_Vpgatherdd_xmm_vm32x_xmm);
	CHECK(instr.is_vsib());
	CHECK(instr.is_vsib32());
	CHECK(!instr.is_vsib64());
	CHECK(instr.vsib() == std::optional<bool>(false));
	instr.set_code(Code::EVEX_Vscatterqpd_vm64z_k1_zmm);
	CHECK(instr.is_vsib());
	CHECK(!instr.is_vsib32());
	CHECK(instr.is_vsib64());
	instr.set_code(Code::MVEX_Vscatterpf1dps_mvt_k1);
	CHECK(instr.is_vsib32());
	instr.set_code(Code::Add_rm8_r8);
	CHECK(!instr.is_vsib());

	// vpgatherdq xmm1,[rax+xmm2*4+10h],xmm3
	instr.set_code(Code::VEX_Vpgatherdq_xmm_vm32x_xmm);
	instr.set_code_size(CodeSize::Code64);
	instr.set_op1_kind(OpKind::Memory);
	instr.set_memory_base(Register::RAX);
	instr.set_memory_index(Register::XMM2);
	instr.set_memory_index_scale(4);
	instr.set_memory_displacement64(0x10);
	instr.set_memory_displ_size(1);
	auto va = instr.virtual_address(1, 1, [](Register reg, std::size_t element_index, std::size_t element_size) -> std::optional<std::uint64_t> {
		if (reg == Register::RAX)
			return 0x1000;
		if (reg == Register::DS)
			return 0;
		if (reg == Register::XMM2 && element_index == 1 && element_size == 4)
			return 0xFFFF'FFFF; // -1 (sign extended)
		return std::nullopt;
	});
	CHECK(va == std::optional<std::uint64_t>(0x1000 + 0x10 - 4));
}

TEST_CASE("core/instruction/mvex") {
	Instruction instr;
	instr.set_code(Code::MVEX_Vmovaps_zmm_k1_zmmmt);
	CHECK(!instr.is_mvex_eviction_hint());
	instr.set_is_mvex_eviction_hint(true);
	CHECK(instr.is_mvex_eviction_hint());
	CHECK(instr.mvex_reg_mem_conv() == MvexRegMemConv::None);
	instr.set_mvex_reg_mem_conv(MvexRegMemConv::MemConvFloat16);
	CHECK(instr.mvex_reg_mem_conv() == MvexRegMemConv::MemConvFloat16);
	CHECK(instr.is_mvex_eviction_hint());
	instr.set_immediate8(0x5A);
	CHECK_EQ(instr.immediate8(), std::uint8_t{0x5A});
	CHECK(instr.mvex_reg_mem_conv() == MvexRegMemConv::MemConvFloat16);
	CHECK(instr.is_mvex_eviction_hint());
	CHECK(instr.memory_size() == MemorySize::Packed256_Float16);
	instr.set_mvex_reg_mem_conv(MvexRegMemConv::MemConvNone);
	CHECK(instr.memory_size() == MemorySize::Packed512_Float32);
	instr.set_mvex_reg_mem_conv(MvexRegMemConv::MemConvBroadcast1);
	CHECK(instr.memory_size() == MemorySize::Float32);
	instr.set_is_mvex_eviction_hint(false);
	CHECK(!instr.is_mvex_eviction_hint());

	// Not an MVEX instruction
	instr.set_code(Code::Add_rm8_r8);
	CHECK(!instr.is_mvex_eviction_hint());
	CHECK(instr.mvex_reg_mem_conv() == MvexRegMemConv::None);

	const internal::MvexInfo& info = internal::get_mvex_info(Code::MVEX_Vmovaps_zmm_k1_zmmmt);
	CHECK(info.tuple_type_lut_kind == MvexTupleTypeLutKind::Float32);
	CHECK(info.conv_fn == MvexConvFn::Sf32);
	CHECK(internal::MVEX_TUPLE_TYPE_LUT[static_cast<std::size_t>(MvexTupleTypeLutKind::Int32) * 8 + 0] == TupleType::N64);
	CHECK(internal::MVEX_TUPLE_TYPE_LUT[static_cast<std::size_t>(MvexTupleTypeLutKind::Int32) * 8 + 1] == TupleType::N4);
}

TEST_CASE("core/instruction/memory_size") {
	Instruction instr;
	instr.set_code(Code::EVEX_Vaddps_xmm_k1z_xmm_xmmm128b32);
	CHECK(instr.memory_size() == MemorySize::Packed128_Float32);
	instr.set_is_broadcast(true);
	CHECK(instr.memory_size() == MemorySize::Broadcast128_Float32);
}

TEST_CASE("core/instruction/declare_data") {
	Instruction instr;
	instr.set_code(Code::DeclareByte);
	instr.set_declare_data_len(16);
	for (std::size_t i = 0; i < 16; i++)
		instr.set_declare_byte_value(i, static_cast<std::uint8_t>(0xA0 + i));
	for (std::size_t i = 0; i < 16; i++) {
		CHECK_EQ(instr.get_declare_byte_value(i), static_cast<std::uint8_t>(0xA0 + i));
		CHECK_EQ(instr.try_get_declare_byte_value(i).value(), static_cast<std::uint8_t>(0xA0 + i));
	}
	CHECK(instr.try_get_declare_byte_value(16).is_err());
	CHECK(instr.try_set_declare_byte_value(16, 0).is_err());
	CHECK_EQ(instr.get_declare_word_value(0), std::uint16_t{0xA1A0});
	CHECK_EQ(instr.get_declare_word_value(7), std::uint16_t{0xAFAE});
	CHECK_EQ(instr.get_declare_dword_value(3), 0xAFAE'ADACU);
	CHECK_EQ(instr.get_declare_qword_value(0), 0xA7A6'A5A4'A3A2'A1A0ULL);
	CHECK_EQ(instr.get_declare_qword_value(1), 0xAFAE'ADAC'ABAA'A9A8ULL);

	instr.set_code(Code::DeclareWord);
	for (std::size_t i = 0; i < 8; i++)
		instr.set_declare_word_value_i16(i, static_cast<std::int16_t>(-1 - static_cast<int>(i)));
	for (std::size_t i = 0; i < 8; i++)
		CHECK_EQ(instr.get_declare_word_value(i), static_cast<std::uint16_t>(-1 - static_cast<int>(i)));
	CHECK(instr.try_get_declare_word_value(8).is_err());
	CHECK(instr.try_set_declare_word_value(8, 0).is_err());

	instr.set_code(Code::DeclareDword);
	for (std::size_t i = 0; i < 4; i++)
		CHECK(instr.try_set_declare_dword_value(i, 0x1234'5670U + static_cast<std::uint32_t>(i)).is_ok());
	for (std::size_t i = 0; i < 4; i++)
		CHECK_EQ(instr.try_get_declare_dword_value(i).value(), 0x1234'5670U + static_cast<std::uint32_t>(i));
	CHECK(instr.try_get_declare_dword_value(4).is_err());
	CHECK(instr.try_set_declare_dword_value_i32(4, 0).is_err());

	instr.set_code(Code::DeclareQword);
	instr.set_declare_qword_value_i64(0, -2);
	instr.set_declare_qword_value(1, 0x8000'0000'0000'0001ULL);
	CHECK_EQ(instr.get_declare_qword_value(0), 0xFFFF'FFFF'FFFF'FFFEULL);
	CHECK_EQ(instr.get_declare_qword_value(1), 0x8000'0000'0000'0001ULL);
	CHECK(instr.try_get_declare_qword_value(2).is_err());
	CHECK(instr.try_set_declare_qword_value_i64(2, 0).is_err());
}

TEST_CASE("core/instruction/prefixes") {
	Instruction instr = create_add_mem_reg();
	instr.set_has_lock_prefix(true);
	instr.set_has_xacquire_prefix(true);
	CHECK(instr.has_xacquire_prefix());
	CHECK(instr.has_repne_prefix());
	CHECK(!instr.has_xrelease_prefix());
	// Not a memory operand
	instr.set_op0_kind(OpKind::Register);
	CHECK(!instr.has_xacquire_prefix());
	// xchg [mem],reg doesn't need a LOCK prefix
	instr = Instruction();
	instr.set_code(Code::Xchg_rm32_r32);
	instr.set_op0_kind(OpKind::Memory);
	instr.set_has_xrelease_prefix(true);
	CHECK(instr.has_xrelease_prefix());
	instr.set_has_xacquire_prefix(true);
	CHECK(instr.has_xacquire_prefix());
	instr.set_code(Code::Mov_rm32_imm32);
	CHECK(instr.has_xrelease_prefix());
	CHECK(!instr.has_xacquire_prefix());
	instr.set_segment_prefix(Register::SS);
	CHECK(instr.memory_segment() == Register::SS);
	CHECK(instr.has_segment_prefix());
	instr.set_segment_prefix(Register::None);
	instr.set_memory_base(Register::RBP);
	CHECK(instr.memory_segment() == Register::SS);
}

TEST_CASE("core/instruction/branch_helpers") {
	Instruction instr;
	instr.set_code(Code::Jbe_rel32_64);
	CHECK(instr.is_jcc_near());
	CHECK(instr.is_jcc_short_or_near());
	CHECK(instr.condition_code() == ConditionCode::be);
	instr.as_short_branch();
	CHECK(instr.code() == Code::Jbe_rel8_64);
	CHECK(instr.is_jcc_short());
	instr.as_short_branch();
	CHECK(instr.code() == Code::Jbe_rel8_64);
	instr.as_near_branch();
	CHECK(instr.code() == Code::Jbe_rel32_64);
	instr.negate_condition_code();
	CHECK(instr.code() == Code::Ja_rel32_64);
	CHECK(instr.condition_code() == ConditionCode::a);
	instr.set_code(Code::Setbe_rm8);
	instr.negate_condition_code();
	CHECK(instr.code() == Code::Seta_rm8);
	instr.set_code(Code::Stosq_m64_RAX);
	CHECK(instr.is_string_instruction());
}

TEST_CASE("core/instruction/internal") {
	Instruction instr;
	InstructionInternal::internal_set_len(instr, 15);
	CHECK_EQ(instr.len(), std::size_t{15});
	InstructionInternal::internal_set_code_size(instr, CodeSize::Code32);
	CHECK(instr.code_size() == CodeSize::Code32);
	InstructionInternal::internal_set_has_repe_prefix(instr);
	CHECK(instr.has_repe_prefix());
	CHECK(InstructionInternal::internal_has_repe_or_repne_prefix(instr));
	InstructionInternal::internal_set_has_repne_prefix(instr);
	CHECK(instr.has_repne_prefix());
	CHECK(!instr.has_repe_prefix());
	InstructionInternal::internal_clear_has_repe_repne_prefix(instr);
	CHECK(!InstructionInternal::internal_has_repe_or_repne_prefix(instr));
	CHECK_EQ(InstructionInternal::internal_has_any_of_lock_rep_repne_prefix(instr), 0U);
	InstructionInternal::internal_set_op_mask(instr, 3);
	CHECK(instr.op_mask() == Register::K3);
	CHECK_EQ(InstructionInternal::internal_op_mask(instr), 3U);
	CHECK(InstructionInternal::internal_has_op_mask_or_zeroing_masking(instr));
	InstructionInternal::internal_set_rounding_control(instr, static_cast<std::uint32_t>(RoundingControl::RoundUp));
	CHECK(instr.rounding_control() == RoundingControl::RoundUp);
	CHECK(InstructionInternal::internal_has_rounding_control_or_sae(instr));
	InstructionInternal::internal_set_memory_displ_size(instr, 3);
	CHECK_EQ(instr.memory_displ_size(), 4U);
	InstructionInternal::internal_set_memory_index_scale(instr, internal::InstrScale::Scale4);
	CHECK_EQ(instr.memory_index_scale(), 4U);
	CHECK_EQ(InstructionInternal::internal_get_memory_index_scale(instr), 2U);
	InstructionInternal::internal_set_declare_data_len(instr, 7);
	CHECK_EQ(instr.declare_data_len(), std::size_t{7});
	CHECK(!InstructionInternal::internal_op0_is_not_reg_or_op1_is_not_reg(instr));
	instr.set_op1_kind(OpKind::Memory);
	CHECK(InstructionInternal::internal_op0_is_not_reg_or_op1_is_not_reg(instr));
	instr.set_segment_prefix(Register::FS);
	CHECK_EQ(InstructionInternal::internal_segment_prefix_raw(instr), 4U);
	instr.set_op2_register(Register::XMM3);
	CHECK(InstructionInternal::internal_op_register(instr, 2) == Register::XMM3);
	CHECK(InstructionInternal::internal_op_register(instr, 4) == Register::None);

	InstructionInternal::internal_set_immediate64_lo(instr, 0x9ABC'DEF0U);
	InstructionInternal::internal_set_immediate64_hi(instr, 0x1234'5678U);
	CHECK_EQ(instr.immediate64(), 0x1234'5678'9ABC'DEF0ULL);

	CHECK_EQ(InstructionInternal::get_address_size_in_bytes(Register::EAX, Register::None, 0, CodeSize::Code64), 4U);
	CHECK_EQ(InstructionInternal::get_address_size_in_bytes(Register::None, Register::R8, 0, CodeSize::Code32), 8U);
	CHECK_EQ(InstructionInternal::get_address_size_in_bytes(Register::BX, Register::SI, 0, CodeSize::Code32), 2U);
	CHECK_EQ(InstructionInternal::get_address_size_in_bytes(Register::RIP, Register::None, 0, CodeSize::Code16), 8U);
	CHECK_EQ(InstructionInternal::get_address_size_in_bytes(Register::None, Register::None, 4, CodeSize::Code64), 4U);
	CHECK_EQ(InstructionInternal::get_address_size_in_bytes(Register::None, Register::None, 1, CodeSize::Code16), 2U);
	CHECK_EQ(InstructionInternal::get_address_size_in_bytes(Register::None, Register::None, 0, CodeSize::Unknown), 8U);

	auto r = InstructionInternal::with_string_reg_segrsi(Code::Lodsb_AL_m8, 32, Register::AL, Register::FS, RepPrefixKind::Repe);
	REQUIRE(r.is_ok());
	CHECK(r.value().op1_kind() == OpKind::MemorySegESI);
	CHECK(r.value().segment_prefix() == Register::FS);
	CHECK(r.value().has_rep_prefix());
	CHECK(InstructionInternal::with_string_reg_segrsi(Code::Lodsb_AL_m8, 8, Register::AL, Register::None, RepPrefixKind::None).is_err());
	auto r2 = InstructionInternal::with_string_segrsi_esrdi(Code::Movsb_m8_m8, 16, Register::None, RepPrefixKind::Repne);
	REQUIRE(r2.is_ok());
	CHECK(r2.value().op0_kind() == OpKind::MemorySegSI);
	CHECK(r2.value().op1_kind() == OpKind::MemoryESDI);
	CHECK(r2.value().has_repne_prefix());
	auto r3 = InstructionInternal::with_maskmov(Code::Maskmovq_rDI_mm_mm, 64, Register::MM1, Register::MM2, Register::None);
	REQUIRE(r3.is_ok());
	CHECK(r3.value().op0_kind() == OpKind::MemorySegRDI);
	CHECK(r3.value().op2_register() == Register::MM2);
}

TEST_CASE("core/code_internal") {
	CHECK(internal::code_ignores_segment(Code::Lea_r64_m));
	CHECK(!internal::code_ignores_segment(Code::Mov_r64_rm64));
	CHECK(internal::code_ignores_index(Code::Bndldx_bnd_mib));
	CHECK(!internal::code_ignores_index(Code::Lea_r64_m));
	CHECK(internal::code_is_tile_stride_index(Code::VEX_Tileloadd_tmm_sibmem));
	CHECK(!internal::code_is_tile_stride_index(Code::Lea_r64_m));
}

TEST_CASE("core/misc_types") {
	ConstantOffsets co;
	CHECK(!co.has_displacement());
	CHECK(!co.has_immediate());
	CHECK(!co.has_immediate2());
	const ConstantOffsets co2(1, 4, 5, 2, 7, 1);
	CHECK_EQ(co2.displacement_offset(), std::size_t{1});
	CHECK_EQ(co2.displacement_size(), std::size_t{4});
	CHECK_EQ(co2.immediate_offset(), std::size_t{5});
	CHECK_EQ(co2.immediate_size(), std::size_t{2});
	CHECK_EQ(co2.immediate_offset2(), std::size_t{7});
	CHECK_EQ(co2.immediate_size2(), std::size_t{1});
	CHECK(co2.has_displacement() && co2.has_immediate() && co2.has_immediate2());
	CHECK(co != co2);
	CHECK(co2 == ConstantOffsets(1, 4, 5, 2, 7, 1));

	static_assert(IcedFeatures::has_decoder() && IcedFeatures::has_encoder() && IcedFeatures::has_instruction_info(), "");

	static const CpuidFeature features[] = {CpuidFeature::AVX, CpuidFeature::AVX2};
	const Slice<CpuidFeature> slice(features);
	CHECK_EQ(slice.size(), std::size_t{2});
	CHECK(slice[1] == CpuidFeature::AVX2);
	CHECK(Slice<CpuidFeature>().empty());

	constexpr FpuStackIncrementInfo fpu(-1, false, true);
	static_assert(fpu.increment() == -1 && !fpu.conditional() && fpu.writes_top(), "");
	CHECK(fpu == FpuStackIncrementInfo(-1, false, true));
	CHECK(FpuStackIncrementInfo() == FpuStackIncrementInfo(0, false, false));

	CHECK_EQ(internal::get_disp8n(TupleType::N8, false), 8U);
	CHECK_EQ(internal::get_disp8n(TupleType::N8, true), 8U);
	CHECK_EQ(internal::get_disp8n(TupleType::N8b2, true), 2U);
	CHECK_EQ(internal::get_disp8n(TupleType::N64b2, true), 2U);

	static const std::uint8_t data[] = {0x05, 0x80, 0x01, 0xFF, 0xFF, 0x03, 0x02, 'h', 'i'};
	internal::DataReader reader(data);
	CHECK(reader.can_read());
	CHECK_EQ(reader.read_u8(), std::size_t{5});
	CHECK_EQ(reader.read_compressed_u32(), 0x80U);
	CHECK_EQ(reader.read_compressed_u32(), 0xFFFFU);
	CHECK(reader.read_ascii_str() == "hi");
	CHECK(!reader.can_read());
	CHECK_EQ(reader.len_left(), std::size_t{0});
	reader.set_index(6);
	std::size_t len_data_size = 0;
	const std::uint8_t* len_data = reader.read_len_data(len_data_size);
	CHECK(len_data == data + 6);
	CHECK_EQ(len_data_size, std::size_t{3});
	CHECK_EQ(reader.index(), std::size_t{9});
}
