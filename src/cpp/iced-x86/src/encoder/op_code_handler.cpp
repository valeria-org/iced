// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "internal/encoder/op_code_handler.hpp"
#include "iced_x86/encoder.hpp"
#include "iced_x86/encoding_kind.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/mvex_eh_bit.hpp"
#include "iced_x86/mvex_reg_mem_conv.hpp"
#include "iced_x86/rounding_control.hpp"
#include "internal/encoder/enc_flags1.hpp"
#include "internal/encoder/enc_flags2.hpp"
#include "internal/encoder/enc_flags3.hpp"
#include "internal/encoder/encoder_data.hpp"
#include "internal/encoder/encoder_flags.hpp"
#include "internal/encoder/encoder_internal.hpp"
#include "internal/encoder/evex_op_code_table.hpp"
#include "internal/encoder/l_bit.hpp"
#include "internal/encoder/legacy_op_code_table.hpp"
#include "internal/encoder/mvex_op_code_table.hpp"
#include "internal/encoder/ops_tables.hpp"
#include "internal/encoder/vex_op_code_table.hpp"
#include "internal/encoder/xop_op_code_table.hpp"
#include "internal/iced_assert.hpp"
#include "internal/instruction_internal.hpp"
#include "internal/mandatory_prefix_byte.hpp"
#include "internal/mvex/mvex_info.hpp"
#include "internal/mvex/mvex_tt_lut.hpp"
#include "internal/tuple_type_tbl.hpp"
#include <limits>
#include <memory>

namespace iced_x86::internal {

namespace {

using E = EncoderInternal;

std::uint32_t get_op_code(std::uint32_t enc_flags2) noexcept { return static_cast<std::uint16_t>(enc_flags2 >> EncFlags2::OP_CODE_SHIFT); }

std::int32_t get_group_index(std::uint32_t enc_flags2) noexcept {
	return (enc_flags2 & EncFlags2::HAS_GROUP_INDEX) == 0 ? -1 : static_cast<std::int32_t>((enc_flags2 >> EncFlags2::GROUP_INDEX_SHIFT) & 7);
}

std::int32_t get_rm_group_index(std::uint32_t enc_flags2, std::uint32_t enc_flags3) noexcept {
	return (enc_flags3 & EncFlags3::HAS_RM_GROUP_INDEX) == 0 ? -1 : static_cast<std::int32_t>((enc_flags2 >> EncFlags2::GROUP_INDEX_SHIFT) & 7);
}

void init_base(OpCodeHandler& h, OpCodeHandler::EncodeFn encode, bool is_special_instr) noexcept {
	h.encode = encode;
	h.try_convert_to_disp8n = nullptr;
	for (auto& op : h.operands)
		op = nullptr;
	h.operands_len = 0;
	h.op_code = 0;
	h.group_index = -1;
	h.rm_group_index = -1;
	h.enc_flags3 = EncFlags3::NONE;
	h.op_size = CodeSize::Unknown;
	h.addr_size = CodeSize::Unknown;
	h.is_2byte_opcode = false;
	h.is_special_instr = is_special_instr;
}

void init_vec_base(OpCodeHandler& h, OpCodeHandler::EncodeFn encode, std::uint32_t enc_flags2, std::uint32_t enc_flags3) noexcept {
	init_base(h, encode, false);
	h.op_code = get_op_code(enc_flags2);
	h.group_index = get_group_index(enc_flags2);
	h.rm_group_index = get_rm_group_index(enc_flags2, enc_flags3);
	h.enc_flags3 = enc_flags3;
	h.is_2byte_opcode = (enc_flags2 & EncFlags2::OP_CODE_IS2_BYTES) != 0;
}

// Same as Rust: the operands are all ops until the last non-zero op index
void init_operands(OpCodeHandler& h, const Op* const* table, const std::uint32_t* ops, std::uint32_t count) noexcept {
	std::uint32_t len = 0;
	for (std::uint32_t i = count; i > 0; i--) {
		if (ops[i - 1] != 0) {
			len = i;
			break;
		}
	}
#ifndef NDEBUG
	for (std::uint32_t i = len; i < count; i++)
		ICED_DEBUG_ASSERT(ops[i] == 0);
#endif
	ICED_ASSERT(len <= OpCodeHandler::MAX_OPERANDS);
	for (std::uint32_t i = 0; i < len; i++)
		h.operands[i] = table[ops[i]];
	h.operands_len = len;
}

// ---------------------------------------------------------------------------
// InvalidHandler

void invalid_encode(const OpCodeHandler*, Encoder& encoder, const Instruction&) { E::set_error_message_str(encoder, INVALID_HANDLER_ERROR_MESSAGE); }

void init_invalid_handler(OpCodeHandler& h) noexcept { init_base(h, invalid_encode, false); }

// ---------------------------------------------------------------------------
// DeclareDataHandler

void declare_data_encode(const OpCodeHandler* self, Encoder& encoder, const Instruction& instruction) {
	const std::size_t length = instruction.declare_data_len() * self->u.declare_data.elem_size;
	for (std::size_t i = 0; i < length; i++) {
		auto value = instruction.try_get_declare_byte_value(i);
		if (value.is_ok())
			E::write_byte_internal(encoder, value.value());
		else {
			E::set_error_message_str(encoder, "Invalid db/dw/dd/dq data length");
			return;
		}
	}
}

void init_declare_data_handler(OpCodeHandler& h, Code code) noexcept {
	init_base(h, declare_data_encode, true);
	std::uint32_t elem_size;
	switch (code) {
	case Code::DeclareByte:
		elem_size = 1;
		break;
	case Code::DeclareWord:
		elem_size = 2;
		break;
	case Code::DeclareDword:
		elem_size = 4;
		break;
	case Code::DeclareQword:
		elem_size = 8;
		break;
	default:
		ICED_UNREACHABLE();
	}
	h.u.declare_data.elem_size = elem_size;
}

// ---------------------------------------------------------------------------
// ZeroBytesHandler

void zero_bytes_encode(const OpCodeHandler*, Encoder&, const Instruction&) {}

void init_zero_bytes_handler(OpCodeHandler& h) noexcept { init_base(h, zero_bytes_encode, true); }

// ---------------------------------------------------------------------------
// LegacyHandler

void legacy_encode(const OpCodeHandler* self, Encoder& encoder, const Instruction& instruction) {
	const OpCodeHandler::LegacyData& d = self->u.legacy;
	std::uint32_t b = d.mandatory_prefix;
	E::write_prefixes(encoder, instruction, b != 0xF3);
	if (b != 0)
		E::write_byte_internal(encoder, b);

	static_assert(EncoderFlags::B == 0x01, "");
	static_assert(EncoderFlags::X == 0x02, "");
	static_assert(EncoderFlags::R == 0x04, "");
	static_assert(EncoderFlags::W == 0x08, "");
	static_assert(EncoderFlags::REX == 0x40, "");
	b = E::encoder_flags(encoder);
	b &= 0x4F;
	if (b != 0) {
		if ((E::encoder_flags(encoder) & EncoderFlags::HIGH_LEGACY_8_BIT_REGS) != 0)
			E::set_error_message_str(encoder,
				"Registers AH, CH, DH, BH can't be used if there's a REX prefix. Use AL, CL, DL, BL, SPL, BPL, SIL, DIL, R8L-R15L instead.");
		b |= 0x40;
		E::write_byte_internal(encoder, b);
	}

	b = d.table_byte1;
	if (b != 0) {
		E::write_byte_internal(encoder, b);
		b = d.table_byte2;
		if (b != 0)
			E::write_byte_internal(encoder, b);
	}
}

void init_legacy_handler(OpCodeHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) noexcept {
	init_vec_base(h, legacy_encode, enc_flags2, enc_flags3);
	h.op_size = static_cast<CodeSize>((enc_flags3 >> EncFlags3::OPERAND_SIZE_SHIFT) & EncFlags3::OPERAND_SIZE_MASK);
	h.addr_size = static_cast<CodeSize>((enc_flags3 >> EncFlags3::ADDRESS_SIZE_SHIFT) & EncFlags3::ADDRESS_SIZE_MASK);

	OpCodeHandler::LegacyData& d = h.u.legacy;
	switch (static_cast<LegacyOpCodeTable>((enc_flags2 >> EncFlags2::TABLE_SHIFT) & EncFlags2::TABLE_MASK)) {
	case LegacyOpCodeTable::MAP0:
		d.table_byte1 = 0;
		d.table_byte2 = 0;
		break;
	case LegacyOpCodeTable::MAP0F:
		d.table_byte1 = 0x0F;
		d.table_byte2 = 0;
		break;
	case LegacyOpCodeTable::MAP0F38:
		d.table_byte1 = 0x0F;
		d.table_byte2 = 0x38;
		break;
	case LegacyOpCodeTable::MAP0F3A:
		d.table_byte1 = 0x0F;
		d.table_byte2 = 0x3A;
		break;
	default:
		ICED_UNREACHABLE();
	}
	switch (static_cast<MandatoryPrefixByte>((enc_flags2 >> EncFlags2::MANDATORY_PREFIX_SHIFT) & EncFlags2::MANDATORY_PREFIX_MASK)) {
	case MandatoryPrefixByte::None:
		d.mandatory_prefix = 0;
		break;
	case MandatoryPrefixByte::P66:
		d.mandatory_prefix = 0x66;
		break;
	case MandatoryPrefixByte::PF3:
		d.mandatory_prefix = 0xF3;
		break;
	case MandatoryPrefixByte::PF2:
		d.mandatory_prefix = 0xF2;
		break;
	default:
		ICED_UNREACHABLE();
	}

	const std::uint32_t ops[4] = {
		(enc_flags1 >> EncFlags1::LEGACY_OP0_SHIFT) & EncFlags1::LEGACY_OP_MASK,
		(enc_flags1 >> EncFlags1::LEGACY_OP1_SHIFT) & EncFlags1::LEGACY_OP_MASK,
		(enc_flags1 >> EncFlags1::LEGACY_OP2_SHIFT) & EncFlags1::LEGACY_OP_MASK,
		(enc_flags1 >> EncFlags1::LEGACY_OP3_SHIFT) & EncFlags1::LEGACY_OP_MASK,
	};
	init_operands(h, LEGACY_TABLE, ops, 4);
}

// ---------------------------------------------------------------------------
// VexHandler

void vex_encode(const OpCodeHandler* self, Encoder& encoder, const Instruction& instruction) {
	const OpCodeHandler::VexData& d = self->u.vex;
	E::write_prefixes(encoder, instruction, true);
	const std::uint32_t encoder_flags = E::encoder_flags(encoder);

	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::None) == 0, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::P66) == 1, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::PF3) == 2, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::PF2) == 3, "");
	std::uint32_t b = d.last_byte;
	b |= (~encoder_flags >> (EncoderFlags::VVVVV_SHIFT - 3)) & 0x78;

	if ((E::prevent_vex2(encoder) | d.w1 | (d.table - static_cast<std::uint32_t>(VexOpCodeTable::MAP0F)) |
			(encoder_flags & (EncoderFlags::X | EncoderFlags::B | EncoderFlags::W))) != 0) {
		E::write_byte_internal(encoder, 0xC4);
		static_assert(static_cast<std::uint32_t>(VexOpCodeTable::MAP0F) == 1, "");
		static_assert(static_cast<std::uint32_t>(VexOpCodeTable::MAP0F38) == 2, "");
		static_assert(static_cast<std::uint32_t>(VexOpCodeTable::MAP0F3A) == 3, "");
		std::uint32_t b2 = d.table;
		static_assert(EncoderFlags::B == 1, "");
		static_assert(EncoderFlags::X == 2, "");
		static_assert(EncoderFlags::R == 4, "");
		b2 |= (~encoder_flags & 7) << 5;
		E::write_byte_internal(encoder, b2);
		b |= d.mask_w_l & E::internal_vex_wig_lig(encoder);
		E::write_byte_internal(encoder, b);
	}
	else {
		E::write_byte_internal(encoder, 0xC5);
		static_assert(EncoderFlags::R == 4, "");
		b |= (~encoder_flags & 4) << 5;
		b |= d.mask_l & E::internal_vex_lig(encoder);
		E::write_byte_internal(encoder, b);
	}
}

void init_vex_handler(OpCodeHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) noexcept {
	init_vec_base(h, vex_encode, enc_flags2, enc_flags3);
	OpCodeHandler::VexData& d = h.u.vex;
	const WBit wbit = static_cast<WBit>((enc_flags2 >> EncFlags2::WBIT_SHIFT) & EncFlags2::WBIT_MASK);
	d.w1 = wbit == WBit::W1 ? 0xFFFF'FFFFU : 0;
	const LBit lbit = static_cast<LBit>((enc_flags2 >> EncFlags2::LBIT_SHIFT) & EncFlags2::LBIT_MASK);
	d.last_byte = lbit == LBit::L1 || lbit == LBit::L256 ? 4 : 0;
	if (d.w1 != 0)
		d.last_byte |= 0x80;
	d.last_byte |= (enc_flags2 >> EncFlags2::MANDATORY_PREFIX_SHIFT) & EncFlags2::MANDATORY_PREFIX_MASK;
	d.mask_w_l = wbit == WBit::WIG ? 0x80 : 0;
	if (lbit == LBit::LIG) {
		d.mask_w_l |= 4;
		d.mask_l = 4;
	}
	else
		d.mask_l = 0;
	d.table = (enc_flags2 >> EncFlags2::TABLE_SHIFT) & EncFlags2::TABLE_MASK;

	const std::uint32_t ops[5] = {
		(enc_flags1 >> EncFlags1::VEX_OP0_SHIFT) & EncFlags1::VEX_OP_MASK,
		(enc_flags1 >> EncFlags1::VEX_OP1_SHIFT) & EncFlags1::VEX_OP_MASK,
		(enc_flags1 >> EncFlags1::VEX_OP2_SHIFT) & EncFlags1::VEX_OP_MASK,
		(enc_flags1 >> EncFlags1::VEX_OP3_SHIFT) & EncFlags1::VEX_OP_MASK,
		(enc_flags1 >> EncFlags1::VEX_OP4_SHIFT) & EncFlags1::VEX_OP_MASK,
	};
	init_operands(h, VEX_TABLE, ops, 5);
}

// ---------------------------------------------------------------------------
// XopHandler

void xop_encode(const OpCodeHandler* self, Encoder& encoder, const Instruction& instruction) {
	const OpCodeHandler::XopData& d = self->u.xop;
	E::write_prefixes(encoder, instruction, true);
	E::write_byte_internal(encoder, 0x8F);

	const std::uint32_t encoder_flags = E::encoder_flags(encoder);
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::None) == 0, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::P66) == 1, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::PF3) == 2, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::PF2) == 3, "");

	std::uint32_t b = d.table;
	static_assert(EncoderFlags::B == 1, "");
	static_assert(EncoderFlags::X == 2, "");
	static_assert(EncoderFlags::R == 4, "");
	b |= (~encoder_flags & 7) << 5;
	E::write_byte_internal(encoder, b);
	b = d.last_byte;
	b |= (~encoder_flags >> (EncoderFlags::VVVVV_SHIFT - 3)) & 0x78;
	E::write_byte_internal(encoder, b);
}

void init_xop_handler(OpCodeHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) noexcept {
	static_assert(static_cast<std::uint32_t>(XopOpCodeTable::MAP8) == 0, "");
	static_assert(static_cast<std::uint32_t>(XopOpCodeTable::MAP9) == 1, "");
	static_assert(static_cast<std::uint32_t>(XopOpCodeTable::MAP10) == 2, "");
	init_vec_base(h, xop_encode, enc_flags2, enc_flags3);
	OpCodeHandler::XopData& d = h.u.xop;
	const LBit lbit = static_cast<LBit>((enc_flags2 >> EncFlags2::LBIT_SHIFT) & EncFlags2::LBIT_MASK);
	d.last_byte = lbit == LBit::L1 || lbit == LBit::L256 ? 4 : 0;
	const WBit wbit = static_cast<WBit>((enc_flags2 >> EncFlags2::WBIT_SHIFT) & EncFlags2::WBIT_MASK);
	if (wbit == WBit::W1)
		d.last_byte |= 0x80;
	d.last_byte |= (enc_flags2 >> EncFlags2::MANDATORY_PREFIX_SHIFT) & EncFlags2::MANDATORY_PREFIX_MASK;
	d.table = 8 + ((enc_flags2 >> EncFlags2::TABLE_SHIFT) & EncFlags2::TABLE_MASK);

	const std::uint32_t ops[4] = {
		(enc_flags1 >> EncFlags1::XOP_OP0_SHIFT) & EncFlags1::XOP_OP_MASK,
		(enc_flags1 >> EncFlags1::XOP_OP1_SHIFT) & EncFlags1::XOP_OP_MASK,
		(enc_flags1 >> EncFlags1::XOP_OP2_SHIFT) & EncFlags1::XOP_OP_MASK,
		(enc_flags1 >> EncFlags1::XOP_OP3_SHIFT) & EncFlags1::XOP_OP_MASK,
	};
	init_operands(h, XOP_TABLE, ops, 4);
}

// ---------------------------------------------------------------------------
// EvexHandler

std::optional<std::int8_t> evex_try_convert_to_disp8n(const OpCodeHandler* self, Encoder& encoder, const Instruction&, std::int32_t displ) {
	const std::int32_t n =
		static_cast<std::int32_t>(get_disp8n(self->u.evex.tuple_type, (E::encoder_flags(encoder) & EncoderFlags::BROADCAST) != 0));
	const std::int32_t res = displ / n;
	if (res * n == displ && std::numeric_limits<std::int8_t>::min() <= res && res <= std::numeric_limits<std::int8_t>::max())
		return static_cast<std::int8_t>(res);
	return std::nullopt;
}

void evex_encode(const OpCodeHandler* self, Encoder& encoder, const Instruction& instruction) {
	const OpCodeHandler::EvexData& d = self->u.evex;
	E::write_prefixes(encoder, instruction, true);
	const std::uint32_t encoder_flags = E::encoder_flags(encoder);

	E::write_byte_internal(encoder, 0x62);

	static_assert(static_cast<std::uint32_t>(EvexOpCodeTable::MAP0F) == 1, "");
	static_assert(static_cast<std::uint32_t>(EvexOpCodeTable::MAP0F38) == 2, "");
	static_assert(static_cast<std::uint32_t>(EvexOpCodeTable::MAP0F3A) == 3, "");
	static_assert(static_cast<std::uint32_t>(EvexOpCodeTable::MAP5) == 5, "");
	static_assert(static_cast<std::uint32_t>(EvexOpCodeTable::MAP6) == 6, "");
	std::uint32_t b = d.table;
	static_assert(EncoderFlags::B == 1, "");
	static_assert(EncoderFlags::X == 2, "");
	static_assert(EncoderFlags::R == 4, "");
	b |= (encoder_flags & 7) << 5;
	static_assert(EncoderFlags::R2 == 0x0000'0200, "");
	b |= (encoder_flags >> (9 - 4)) & 0x10;
	b ^= ~0xFU;
	E::write_byte_internal(encoder, b);

	b = d.p1_bits;
	b |= (~encoder_flags >> (EncoderFlags::VVVVV_SHIFT - 3)) & 0x78;
	b |= d.mask_w & E::internal_evex_wig(encoder);
	E::write_byte_internal(encoder, b);

	b = InstructionInternal::internal_op_mask(instruction);
	if (b != 0) {
		if ((self->enc_flags3 & EncFlags3::OP_MASK_REGISTER) == 0)
			E::set_error_message_str(encoder, "The instruction doesn't support opmask registers");
	}
	else {
		if ((self->enc_flags3 & EncFlags3::REQUIRE_OP_MASK_REGISTER) != 0)
			E::set_error_message_str(encoder, "The instruction must use an opmask register");
	}
	b |= (encoder_flags >> (EncoderFlags::VVVVV_SHIFT + 4 - 3)) & 8;
	if (instruction.suppress_all_exceptions()) {
		if ((self->enc_flags3 & EncFlags3::SUPPRESS_ALL_EXCEPTIONS) == 0)
			E::set_error_message_str(encoder, "The instruction doesn't support suppress-all-exceptions");
		b |= 0x10;
	}
	const RoundingControl rc = instruction.rounding_control();
	if (rc != RoundingControl::None) {
		if ((self->enc_flags3 & EncFlags3::ROUNDING_CONTROL) == 0)
			E::set_error_message_str(encoder, "The instruction doesn't support rounding control");
		b |= 0x10;
		static_assert(static_cast<std::uint32_t>(RoundingControl::RoundToNearest) == 1, "");
		static_assert(static_cast<std::uint32_t>(RoundingControl::RoundDown) == 2, "");
		static_assert(static_cast<std::uint32_t>(RoundingControl::RoundUp) == 3, "");
		static_assert(static_cast<std::uint32_t>(RoundingControl::RoundTowardZero) == 4, "");
		b |= (static_cast<std::uint32_t>(rc) - static_cast<std::uint32_t>(RoundingControl::RoundToNearest)) << 5;
	}
	else if ((self->enc_flags3 & EncFlags3::SUPPRESS_ALL_EXCEPTIONS) == 0 || !instruction.suppress_all_exceptions())
		b |= d.ll_bits;
	if ((encoder_flags & EncoderFlags::BROADCAST) != 0)
		b |= 0x10;
	else if (instruction.is_broadcast())
		E::set_error_message_str(encoder, "The instruction doesn't support broadcasting");
	if (instruction.zeroing_masking()) {
		if ((self->enc_flags3 & EncFlags3::ZEROING_MASKING) == 0)
			E::set_error_message_str(encoder, "The instruction doesn't support zeroing masking");
		b |= 0x80;
	}
	b ^= 8;
	b |= d.mask_ll & E::internal_evex_lig(encoder);
	E::write_byte_internal(encoder, b);
}

void init_evex_handler(OpCodeHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) noexcept {
	init_vec_base(h, evex_encode, enc_flags2, enc_flags3);
	h.try_convert_to_disp8n = evex_try_convert_to_disp8n;
	OpCodeHandler::EvexData& d = h.u.evex;
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::None) == 0, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::P66) == 1, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::PF3) == 2, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::PF2) == 3, "");
	d.p1_bits = 4 | ((enc_flags2 >> EncFlags2::MANDATORY_PREFIX_SHIFT) & EncFlags2::MANDATORY_PREFIX_MASK);
	const WBit wbit = static_cast<WBit>((enc_flags2 >> EncFlags2::WBIT_SHIFT) & EncFlags2::WBIT_MASK);
	if (wbit == WBit::W1)
		d.p1_bits |= 0x80;
	const LBit lbit = static_cast<LBit>((enc_flags2 >> EncFlags2::LBIT_SHIFT) & EncFlags2::LBIT_MASK);
	d.mask_ll = 0;
	switch (lbit) {
	case LBit::LIG:
		d.mask_ll = 3 << 5;
		d.ll_bits = 0 << 5;
		break;
	case LBit::L0:
	case LBit::LZ:
	case LBit::L128:
		d.ll_bits = 0 << 5;
		break;
	case LBit::L1:
	case LBit::L256:
		d.ll_bits = 1 << 5;
		break;
	case LBit::L512:
		d.ll_bits = 2 << 5;
		break;
	default:
		ICED_UNREACHABLE();
	}
	d.mask_w = wbit == WBit::WIG ? 0x80 : 0;
	d.table = (enc_flags2 >> EncFlags2::TABLE_SHIFT) & EncFlags2::TABLE_MASK;
	d.tuple_type = static_cast<TupleType>((enc_flags3 >> EncFlags3::TUPLE_TYPE_SHIFT) & EncFlags3::TUPLE_TYPE_MASK);
	d.wbit = wbit;

	const std::uint32_t ops[4] = {
		(enc_flags1 >> EncFlags1::EVEX_OP0_SHIFT) & EncFlags1::EVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::EVEX_OP1_SHIFT) & EncFlags1::EVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::EVEX_OP2_SHIFT) & EncFlags1::EVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::EVEX_OP3_SHIFT) & EncFlags1::EVEX_OP_MASK,
	};
	init_operands(h, EVEX_TABLE, ops, 4);
}

// ---------------------------------------------------------------------------
// MvexHandler

std::optional<std::int8_t> mvex_try_convert_to_disp8n(const OpCodeHandler*, Encoder&, const Instruction& instruction, std::int32_t displ) {
	const MvexInfo& mvex = get_mvex_info(instruction.code());
	const MvexRegMemConv conv = instruction.mvex_reg_mem_conv();
	const std::size_t sss = (static_cast<std::size_t>(conv) - static_cast<std::size_t>(MvexRegMemConv::MemConvNone)) & 7;
	const TupleType tuple_type = MVEX_TUPLE_TYPE_LUT[static_cast<std::size_t>(mvex.tuple_type_lut_kind) * 8 + sss];

	const std::int32_t n = static_cast<std::int32_t>(get_disp8n(tuple_type, false));
	const std::int32_t res = displ / n;
	if (res * n == displ && std::numeric_limits<std::int8_t>::min() <= res && res <= std::numeric_limits<std::int8_t>::max())
		return static_cast<std::int8_t>(res);
	return std::nullopt;
}

void mvex_encode(const OpCodeHandler* self, Encoder& encoder, const Instruction& instruction) {
	const OpCodeHandler::MvexData& d = self->u.mvex;
	E::write_prefixes(encoder, instruction, true);
	const std::uint32_t encoder_flags = E::encoder_flags(encoder);

	E::write_byte_internal(encoder, 0x62);

	static_assert(static_cast<std::uint32_t>(MvexOpCodeTable::MAP0F) == 1, "");
	static_assert(static_cast<std::uint32_t>(MvexOpCodeTable::MAP0F38) == 2, "");
	static_assert(static_cast<std::uint32_t>(MvexOpCodeTable::MAP0F3A) == 3, "");
	std::uint32_t b = d.table;
	static_assert(EncoderFlags::B == 1, "");
	static_assert(EncoderFlags::X == 2, "");
	static_assert(EncoderFlags::R == 4, "");
	b |= (encoder_flags & 7) << 5;
	static_assert(EncoderFlags::R2 == 0x0000'0200, "");
	b |= (encoder_flags >> (9 - 4)) & 0x10;
	b ^= ~0xFU;
	E::write_byte_internal(encoder, b);

	b = d.p1_bits;
	b |= (~encoder_flags >> (EncoderFlags::VVVVV_SHIFT - 3)) & 0x78;
	b |= d.mask_w & E::internal_mvex_wig(encoder);
	E::write_byte_internal(encoder, b);

	b = InstructionInternal::internal_op_mask(instruction);
	if (b != 0) {
		if ((self->enc_flags3 & EncFlags3::OP_MASK_REGISTER) == 0)
			E::set_error_message_str(encoder, "The instruction doesn't support opmask registers");
	}
	else {
		if ((self->enc_flags3 & EncFlags3::REQUIRE_OP_MASK_REGISTER) != 0)
			E::set_error_message_str(encoder, "The instruction must use an opmask register");
	}
	b |= (encoder_flags >> (EncoderFlags::VVVVV_SHIFT + 4 - 3)) & 8;
	const MvexInfo& mvex = get_mvex_info(instruction.code());
	const MvexRegMemConv conv = instruction.mvex_reg_mem_conv();
	// Memory ops can only be op0-op2, never op3 (imm8)
	if (instruction.op0_kind() == OpKind::Memory || instruction.op1_kind() == OpKind::Memory || instruction.op2_kind() == OpKind::Memory) {
		static_assert(static_cast<std::uint32_t>(MvexRegMemConv::MemConvNone) + 1 == static_cast<std::uint32_t>(MvexRegMemConv::MemConvBroadcast1), "");
		static_assert(static_cast<std::uint32_t>(MvexRegMemConv::MemConvNone) + 2 == static_cast<std::uint32_t>(MvexRegMemConv::MemConvBroadcast4), "");
		static_assert(static_cast<std::uint32_t>(MvexRegMemConv::MemConvNone) + 3 == static_cast<std::uint32_t>(MvexRegMemConv::MemConvFloat16), "");
		static_assert(static_cast<std::uint32_t>(MvexRegMemConv::MemConvNone) + 4 == static_cast<std::uint32_t>(MvexRegMemConv::MemConvUint8), "");
		static_assert(static_cast<std::uint32_t>(MvexRegMemConv::MemConvNone) + 5 == static_cast<std::uint32_t>(MvexRegMemConv::MemConvSint8), "");
		static_assert(static_cast<std::uint32_t>(MvexRegMemConv::MemConvNone) + 6 == static_cast<std::uint32_t>(MvexRegMemConv::MemConvUint16), "");
		static_assert(static_cast<std::uint32_t>(MvexRegMemConv::MemConvNone) + 7 == static_cast<std::uint32_t>(MvexRegMemConv::MemConvSint16), "");
		if (conv >= MvexRegMemConv::MemConvNone && conv <= MvexRegMemConv::MemConvSint16)
			b |= (static_cast<std::uint32_t>(conv) - static_cast<std::uint32_t>(MvexRegMemConv::MemConvNone)) << 4;
		else if (conv == MvexRegMemConv::None) {
			// Nothing, treat it as MvexRegMemConv::MemConvNone
		}
		else
			E::set_error_message_str(encoder, "Memory operands must use a valid MvexRegMemConv variant, eg. MvexRegMemConv::MemConvNone");
		if (instruction.is_mvex_eviction_hint()) {
			if (mvex.can_use_eviction_hint())
				b |= 0x80;
			else
				E::set_error_message_str(encoder, "This instruction doesn't support eviction hint (`{eh}`)");
		}
	}
	else {
		if (instruction.is_mvex_eviction_hint())
			E::set_error_message_str(encoder, "Only memory operands can enable eviction hint (`{eh}`)");
		if (conv == MvexRegMemConv::None) {
			b |= 0x80;
			if (instruction.suppress_all_exceptions()) {
				b |= 0x40;
				if ((self->enc_flags3 & EncFlags3::SUPPRESS_ALL_EXCEPTIONS) == 0)
					E::set_error_message_str(encoder, "The instruction doesn't support suppress-all-exceptions");
			}
			const RoundingControl rc = instruction.rounding_control();
			if (rc == RoundingControl::None) {
				// Nothing
			}
			else {
				if ((self->enc_flags3 & EncFlags3::ROUNDING_CONTROL) == 0)
					E::set_error_message_str(encoder, "The instruction doesn't support rounding control");
				else {
					static_assert(static_cast<std::uint32_t>(RoundingControl::RoundToNearest) == 1, "");
					static_assert(static_cast<std::uint32_t>(RoundingControl::RoundDown) == 2, "");
					static_assert(static_cast<std::uint32_t>(RoundingControl::RoundUp) == 3, "");
					static_assert(static_cast<std::uint32_t>(RoundingControl::RoundTowardZero) == 4, "");
					b |= (static_cast<std::uint32_t>(rc) - static_cast<std::uint32_t>(RoundingControl::RoundToNearest)) << 4;
				}
			}
		}
		else if (conv >= MvexRegMemConv::RegSwizzleNone && conv <= MvexRegMemConv::RegSwizzleDddd) {
			if (instruction.suppress_all_exceptions())
				E::set_error_message_str(encoder, "Can't use {sae} with register swizzles");
			else if (instruction.rounding_control() != RoundingControl::None)
				E::set_error_message_str(encoder, "Can't use rounding control with register swizzles");
			b |= ((static_cast<std::uint32_t>(conv) - static_cast<std::uint32_t>(MvexRegMemConv::RegSwizzleNone)) & 7) << 4;
		}
		else
			E::set_error_message_str(encoder, "Register operands can't use memory up/down conversions");
	}
	if (mvex.eh_bit == MvexEHBit::EH1)
		b |= 0x80;
	b ^= 8;
	E::write_byte_internal(encoder, b);
}

void init_mvex_handler(OpCodeHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) noexcept {
	init_vec_base(h, mvex_encode, enc_flags2, enc_flags3);
	h.try_convert_to_disp8n = mvex_try_convert_to_disp8n;
	OpCodeHandler::MvexData& d = h.u.mvex;
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::None) == 0, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::P66) == 1, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::PF3) == 2, "");
	static_assert(static_cast<std::uint32_t>(MandatoryPrefixByte::PF2) == 3, "");
	d.p1_bits = (enc_flags2 >> EncFlags2::MANDATORY_PREFIX_SHIFT) & EncFlags2::MANDATORY_PREFIX_MASK;
	const WBit wbit = static_cast<WBit>((enc_flags2 >> EncFlags2::WBIT_SHIFT) & EncFlags2::WBIT_MASK);
	if (wbit == WBit::W1)
		d.p1_bits |= 0x80;
	d.mask_w = wbit == WBit::WIG ? 0x80 : 0;
	d.table = (enc_flags2 >> EncFlags2::TABLE_SHIFT) & EncFlags2::TABLE_MASK;
	d.wbit = wbit;

	const std::uint32_t ops[4] = {
		(enc_flags1 >> EncFlags1::MVEX_OP0_SHIFT) & EncFlags1::MVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::MVEX_OP1_SHIFT) & EncFlags1::MVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::MVEX_OP2_SHIFT) & EncFlags1::MVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::MVEX_OP3_SHIFT) & EncFlags1::MVEX_OP_MASK,
	};
	init_operands(h, MVEX_TABLE, ops, 4);
}

// ---------------------------------------------------------------------------
// D3nowHandler

constexpr OpModRM_reg D3NOW_OP0{Register::MM0, Register::MM7};
constexpr OpModRM_rm D3NOW_OP1{Register::MM0, Register::MM7};

void d3now_encode(const OpCodeHandler* self, Encoder& encoder, const Instruction& instruction) {
	E::write_prefixes(encoder, instruction, true);
	E::write_byte_internal(encoder, 0x0F);
	E::imm_size(encoder) = ImmSize::Size1OpCode;
	E::immediate(encoder) = self->u.d3now.immediate;
}

void init_d3now_handler(OpCodeHandler& h, std::uint32_t enc_flags2, std::uint32_t enc_flags3) noexcept {
	init_base(h, d3now_encode, false);
	h.operands[0] = &D3NOW_OP0;
	h.operands[1] = &D3NOW_OP1;
	h.operands_len = 2;
	h.op_code = 0x0F;
	h.enc_flags3 = enc_flags3;
	h.is_2byte_opcode = (enc_flags2 & EncFlags2::OP_CODE_IS2_BYTES) != 0;
	h.u.d3now.immediate = get_op_code(enc_flags2);
}

// ---------------------------------------------------------------------------

struct HandlersTable {
	std::unique_ptr<OpCodeHandler[]> handlers;

	HandlersTable() : handlers(new OpCodeHandler[IcedConstants::CODE_ENUM_COUNT]) {
		for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
			const Code code = static_cast<Code>(i);
			OpCodeHandler& h = handlers[i];
			const std::uint32_t enc_flags1 = ENC_FLAGS1[i];
			const std::uint32_t enc_flags2 = ENC_FLAGS2[i];
			const std::uint32_t enc_flags3 = ENC_FLAGS3[i];
			const EncodingKind encoding = static_cast<EncodingKind>((enc_flags3 >> EncFlags3::ENCODING_SHIFT) & EncFlags3::ENCODING_MASK);
			switch (encoding) {
			case EncodingKind::Legacy:
				if (code == Code::INVALID)
					init_invalid_handler(h);
				else if (code <= Code::DeclareQword)
					init_declare_data_handler(h, code);
				else if (code == Code::Zero_bytes)
					init_zero_bytes_handler(h);
				else
					init_legacy_handler(h, enc_flags1, enc_flags2, enc_flags3);
				break;
			case EncodingKind::VEX:
				init_vex_handler(h, enc_flags1, enc_flags2, enc_flags3);
				break;
			case EncodingKind::EVEX:
				init_evex_handler(h, enc_flags1, enc_flags2, enc_flags3);
				break;
			case EncodingKind::XOP:
				init_xop_handler(h, enc_flags1, enc_flags2, enc_flags3);
				break;
			case EncodingKind::D3NOW:
				init_d3now_handler(h, enc_flags2, enc_flags3);
				break;
			case EncodingKind::MVEX:
				init_mvex_handler(h, enc_flags1, enc_flags2, enc_flags3);
				break;
			default:
				ICED_UNREACHABLE();
			}
		}
	}
};

} // namespace

const OpCodeHandler* get_handlers_table() noexcept {
	static const HandlersTable table;
	return table.handlers.get();
}

} // namespace iced_x86::internal
