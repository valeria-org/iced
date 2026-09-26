// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Verifies the generated op code handlers table (src/encoder/op_code_handlers_table.cpp). Rust creates the
// handlers at runtime from the ENC_FLAGS1..3 tables. This test creates the handlers the same way (same code
// as the Rust handler constructors in src/rust/iced-x86/src/encoder/op_code_handler.rs) and compares them
// with the generated handlers.

#include "test_framework.hpp"
#include "iced_x86/code.hpp"
#include "iced_x86/code_size.hpp"
#include "iced_x86/encoder.hpp"
#include "iced_x86/encoding_kind.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/memory_operand.hpp"
#include "iced_x86/tuple_type.hpp"
#include "internal/encoder/enc_flags1.hpp"
#include "internal/encoder/enc_flags2.hpp"
#include "internal/encoder/enc_flags3.hpp"
#include "internal/encoder/encoder_data.hpp"
#include "internal/encoder/l_bit.hpp"
#include "internal/encoder/legacy_op_code_table.hpp"
#include "internal/encoder/op_code_handler.hpp"
#include "internal/encoder/w_bit.hpp"
#include "internal/mandatory_prefix_byte.hpp"

#include <cstdint>
#include <vector>

using namespace iced_x86;
using namespace iced_x86::internal;

namespace {

struct ExpectedHandler {
	std::uint32_t enc_flags3 = 0;
	std::uint32_t op_code = 0;
	OpCodeHandlerKind kind = OpCodeHandlerKind::Invalid;
	std::uint32_t operands_len = 0;
	std::uint32_t operands[OpCodeHandler::MAX_OPERANDS] = {};
	std::int32_t group_index = -1;
	std::int32_t rm_group_index = -1;
	CodeSize op_size = CodeSize::Unknown;
	CodeSize addr_size = CodeSize::Unknown;
	bool is_2byte_opcode = false;
	bool is_special_instr = false;
	// Handler kind specific data
	std::uint32_t data[6] = {};
};

std::uint32_t get_op_code(std::uint32_t enc_flags2) { return static_cast<std::uint16_t>(enc_flags2 >> EncFlags2::OP_CODE_SHIFT); }

void init_vec_base(ExpectedHandler& h, OpCodeHandlerKind kind, std::uint32_t enc_flags2, std::uint32_t enc_flags3) {
	h.kind = kind;
	h.op_code = get_op_code(enc_flags2);
	h.group_index = (enc_flags2 & EncFlags2::HAS_GROUP_INDEX) == 0 ? -1 : static_cast<std::int32_t>((enc_flags2 >> EncFlags2::GROUP_INDEX_SHIFT) & 7);
	h.rm_group_index = (enc_flags3 & EncFlags3::HAS_RM_GROUP_INDEX) == 0 ? -1 : static_cast<std::int32_t>((enc_flags2 >> EncFlags2::GROUP_INDEX_SHIFT) & 7);
	h.enc_flags3 = enc_flags3;
	h.is_2byte_opcode = (enc_flags2 & EncFlags2::OP_CODE_IS2_BYTES) != 0;
}

void init_operands(ExpectedHandler& h, std::uint32_t offset, const std::uint32_t* ops, std::uint32_t count) {
	std::uint32_t len = 0;
	for (std::uint32_t i = count; i > 0; i--) {
		if (ops[i - 1] != 0) {
			len = i;
			break;
		}
	}
	for (std::uint32_t i = len; i < count; i++)
		CHECK_EQ(ops[i], 0U);
	REQUIRE(len <= OpCodeHandler::MAX_OPERANDS);
	for (std::uint32_t i = 0; i < len; i++)
		h.operands[i] = offset + ops[i];
	h.operands_len = len;
}

WBit get_wbit(std::uint32_t enc_flags2) { return static_cast<WBit>((enc_flags2 >> EncFlags2::WBIT_SHIFT) & EncFlags2::WBIT_MASK); }
LBit get_lbit(std::uint32_t enc_flags2) { return static_cast<LBit>((enc_flags2 >> EncFlags2::LBIT_SHIFT) & EncFlags2::LBIT_MASK); }
std::uint32_t get_table(std::uint32_t enc_flags2) { return (enc_flags2 >> EncFlags2::TABLE_SHIFT) & EncFlags2::TABLE_MASK; }
std::uint32_t get_mandatory_prefix_byte(std::uint32_t enc_flags2) {
	return (enc_flags2 >> EncFlags2::MANDATORY_PREFIX_SHIFT) & EncFlags2::MANDATORY_PREFIX_MASK;
}

void init_legacy(ExpectedHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) {
	init_vec_base(h, OpCodeHandlerKind::Legacy, enc_flags2, enc_flags3);
	h.op_size = static_cast<CodeSize>((enc_flags3 >> EncFlags3::OPERAND_SIZE_SHIFT) & EncFlags3::OPERAND_SIZE_MASK);
	h.addr_size = static_cast<CodeSize>((enc_flags3 >> EncFlags3::ADDRESS_SIZE_SHIFT) & EncFlags3::ADDRESS_SIZE_MASK);
	switch (static_cast<LegacyOpCodeTable>(get_table(enc_flags2))) {
	case LegacyOpCodeTable::MAP0:
		h.data[0] = 0;
		h.data[1] = 0;
		break;
	case LegacyOpCodeTable::MAP0F:
		h.data[0] = 0x0F;
		h.data[1] = 0;
		break;
	case LegacyOpCodeTable::MAP0F38:
		h.data[0] = 0x0F;
		h.data[1] = 0x38;
		break;
	case LegacyOpCodeTable::MAP0F3A:
		h.data[0] = 0x0F;
		h.data[1] = 0x3A;
		break;
	default:
		CHECK(false);
		break;
	}
	switch (static_cast<MandatoryPrefixByte>(get_mandatory_prefix_byte(enc_flags2))) {
	case MandatoryPrefixByte::None:
		h.data[2] = 0;
		break;
	case MandatoryPrefixByte::P66:
		h.data[2] = 0x66;
		break;
	case MandatoryPrefixByte::PF3:
		h.data[2] = 0xF3;
		break;
	case MandatoryPrefixByte::PF2:
		h.data[2] = 0xF2;
		break;
	default:
		CHECK(false);
		break;
	}
	const std::uint32_t ops[4] = {
		(enc_flags1 >> EncFlags1::LEGACY_OP0_SHIFT) & EncFlags1::LEGACY_OP_MASK,
		(enc_flags1 >> EncFlags1::LEGACY_OP1_SHIFT) & EncFlags1::LEGACY_OP_MASK,
		(enc_flags1 >> EncFlags1::LEGACY_OP2_SHIFT) & EncFlags1::LEGACY_OP_MASK,
		(enc_flags1 >> EncFlags1::LEGACY_OP3_SHIFT) & EncFlags1::LEGACY_OP_MASK,
	};
	init_operands(h, LEGACY_OPS_OFFSET, ops, 4);
}

void init_vex(ExpectedHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) {
	init_vec_base(h, OpCodeHandlerKind::VEX, enc_flags2, enc_flags3);
	const WBit wbit = get_wbit(enc_flags2);
	const LBit lbit = get_lbit(enc_flags2);
	const bool w1 = wbit == WBit::W1;
	std::uint32_t last_byte = lbit == LBit::L1 || lbit == LBit::L256 ? 4 : 0;
	if (w1)
		last_byte |= 0x80;
	last_byte |= get_mandatory_prefix_byte(enc_flags2);
	std::uint32_t mask_w_l = wbit == WBit::WIG ? 0x80 : 0;
	std::uint32_t mask_l = 0;
	if (lbit == LBit::LIG) {
		mask_w_l |= 4;
		mask_l = 4;
	}
	h.data[0] = get_table(enc_flags2);
	h.data[1] = last_byte;
	h.data[2] = mask_w_l;
	h.data[3] = mask_l;
	h.data[4] = w1 ? 1 : 0;
	const std::uint32_t ops[5] = {
		(enc_flags1 >> EncFlags1::VEX_OP0_SHIFT) & EncFlags1::VEX_OP_MASK,
		(enc_flags1 >> EncFlags1::VEX_OP1_SHIFT) & EncFlags1::VEX_OP_MASK,
		(enc_flags1 >> EncFlags1::VEX_OP2_SHIFT) & EncFlags1::VEX_OP_MASK,
		(enc_flags1 >> EncFlags1::VEX_OP3_SHIFT) & EncFlags1::VEX_OP_MASK,
		(enc_flags1 >> EncFlags1::VEX_OP4_SHIFT) & EncFlags1::VEX_OP_MASK,
	};
	init_operands(h, VEX_OPS_OFFSET, ops, 5);
}

void init_xop(ExpectedHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) {
	init_vec_base(h, OpCodeHandlerKind::XOP, enc_flags2, enc_flags3);
	const LBit lbit = get_lbit(enc_flags2);
	std::uint32_t last_byte = lbit == LBit::L1 || lbit == LBit::L256 ? 4 : 0;
	if (get_wbit(enc_flags2) == WBit::W1)
		last_byte |= 0x80;
	last_byte |= get_mandatory_prefix_byte(enc_flags2);
	h.data[0] = 8 + get_table(enc_flags2);
	h.data[1] = last_byte;
	const std::uint32_t ops[4] = {
		(enc_flags1 >> EncFlags1::XOP_OP0_SHIFT) & EncFlags1::XOP_OP_MASK,
		(enc_flags1 >> EncFlags1::XOP_OP1_SHIFT) & EncFlags1::XOP_OP_MASK,
		(enc_flags1 >> EncFlags1::XOP_OP2_SHIFT) & EncFlags1::XOP_OP_MASK,
		(enc_flags1 >> EncFlags1::XOP_OP3_SHIFT) & EncFlags1::XOP_OP_MASK,
	};
	init_operands(h, XOP_OPS_OFFSET, ops, 4);
}

void init_evex(ExpectedHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) {
	init_vec_base(h, OpCodeHandlerKind::EVEX, enc_flags2, enc_flags3);
	const WBit wbit = get_wbit(enc_flags2);
	std::uint32_t p1_bits = 4 | get_mandatory_prefix_byte(enc_flags2);
	if (wbit == WBit::W1)
		p1_bits |= 0x80;
	std::uint32_t ll_bits = 0;
	std::uint32_t mask_ll = 0;
	switch (get_lbit(enc_flags2)) {
	case LBit::LIG:
		mask_ll = 3 << 5;
		ll_bits = 0 << 5;
		break;
	case LBit::L0:
	case LBit::LZ:
	case LBit::L128:
		ll_bits = 0 << 5;
		break;
	case LBit::L1:
	case LBit::L256:
		ll_bits = 1 << 5;
		break;
	case LBit::L512:
		ll_bits = 2 << 5;
		break;
	default:
		CHECK(false);
		break;
	}
	h.data[0] = get_table(enc_flags2);
	h.data[1] = p1_bits;
	h.data[2] = ll_bits;
	h.data[3] = wbit == WBit::WIG ? 0x80 : 0;
	h.data[4] = mask_ll;
	h.data[5] = (enc_flags3 >> EncFlags3::TUPLE_TYPE_SHIFT) & EncFlags3::TUPLE_TYPE_MASK;
	const std::uint32_t ops[4] = {
		(enc_flags1 >> EncFlags1::EVEX_OP0_SHIFT) & EncFlags1::EVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::EVEX_OP1_SHIFT) & EncFlags1::EVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::EVEX_OP2_SHIFT) & EncFlags1::EVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::EVEX_OP3_SHIFT) & EncFlags1::EVEX_OP_MASK,
	};
	init_operands(h, EVEX_OPS_OFFSET, ops, 4);
}

void init_mvex(ExpectedHandler& h, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3) {
	init_vec_base(h, OpCodeHandlerKind::MVEX, enc_flags2, enc_flags3);
	const WBit wbit = get_wbit(enc_flags2);
	std::uint32_t p1_bits = get_mandatory_prefix_byte(enc_flags2);
	if (wbit == WBit::W1)
		p1_bits |= 0x80;
	h.data[0] = get_table(enc_flags2);
	h.data[1] = p1_bits;
	h.data[2] = wbit == WBit::WIG ? 0x80 : 0;
	const std::uint32_t ops[4] = {
		(enc_flags1 >> EncFlags1::MVEX_OP0_SHIFT) & EncFlags1::MVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::MVEX_OP1_SHIFT) & EncFlags1::MVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::MVEX_OP2_SHIFT) & EncFlags1::MVEX_OP_MASK,
		(enc_flags1 >> EncFlags1::MVEX_OP3_SHIFT) & EncFlags1::MVEX_OP_MASK,
	};
	init_operands(h, MVEX_OPS_OFFSET, ops, 4);
}

void init_d3now(ExpectedHandler& h, std::uint32_t enc_flags2, std::uint32_t enc_flags3) {
	h.kind = OpCodeHandlerKind::D3NOW;
	// The operands are OpModRM_reg(MM0, MM7) and OpModRM_rm(MM0, MM7). They're verified by the d3now_operands test.
	h.operands_len = 2;
	h.op_code = 0x0F;
	h.enc_flags3 = enc_flags3;
	h.is_2byte_opcode = (enc_flags2 & EncFlags2::OP_CODE_IS2_BYTES) != 0;
	h.data[0] = get_op_code(enc_flags2);
}

ExpectedHandler create_handler(Code code) {
	const std::size_t i = static_cast<std::size_t>(code);
	const std::uint32_t enc_flags1 = ENC_FLAGS1[i];
	const std::uint32_t enc_flags2 = ENC_FLAGS2[i];
	const std::uint32_t enc_flags3 = ENC_FLAGS3[i];
	ExpectedHandler h;
	switch (static_cast<EncodingKind>((enc_flags3 >> EncFlags3::ENCODING_SHIFT) & EncFlags3::ENCODING_MASK)) {
	case EncodingKind::Legacy:
		if (code == Code::INVALID)
			h.kind = OpCodeHandlerKind::Invalid;
		else if (code <= Code::DeclareQword) {
			h.kind = OpCodeHandlerKind::DeclareData;
			h.is_special_instr = true;
			switch (code) {
			case Code::DeclareByte:
				h.data[0] = 1;
				break;
			case Code::DeclareWord:
				h.data[0] = 2;
				break;
			case Code::DeclareDword:
				h.data[0] = 4;
				break;
			case Code::DeclareQword:
				h.data[0] = 8;
				break;
			default:
				CHECK(false);
				break;
			}
		}
		else if (code == Code::Zero_bytes) {
			h.kind = OpCodeHandlerKind::ZeroBytes;
			h.is_special_instr = true;
		}
		else
			init_legacy(h, enc_flags1, enc_flags2, enc_flags3);
		break;
	case EncodingKind::VEX:
		init_vex(h, enc_flags1, enc_flags2, enc_flags3);
		break;
	case EncodingKind::EVEX:
		init_evex(h, enc_flags1, enc_flags2, enc_flags3);
		break;
	case EncodingKind::XOP:
		init_xop(h, enc_flags1, enc_flags2, enc_flags3);
		break;
	case EncodingKind::D3NOW:
		init_d3now(h, enc_flags2, enc_flags3);
		break;
	case EncodingKind::MVEX:
		init_mvex(h, enc_flags1, enc_flags2, enc_flags3);
		break;
	default:
		CHECK(false);
		break;
	}
	return h;
}

void get_data(const OpCodeHandler& h, std::uint32_t (&data)[6]) {
	for (auto& d : data)
		d = 0;
	switch (h.kind) {
	case OpCodeHandlerKind::Invalid:
	case OpCodeHandlerKind::ZeroBytes:
		break;
	case OpCodeHandlerKind::DeclareData:
		data[0] = h.u.declare_data.elem_size;
		break;
	case OpCodeHandlerKind::Legacy:
		data[0] = h.u.legacy.table_byte1;
		data[1] = h.u.legacy.table_byte2;
		data[2] = h.u.legacy.mandatory_prefix;
		break;
	case OpCodeHandlerKind::VEX:
		data[0] = h.u.vex.table;
		data[1] = h.u.vex.last_byte;
		data[2] = h.u.vex.mask_w_l;
		data[3] = h.u.vex.mask_l;
		data[4] = h.u.vex.w1 ? 1 : 0;
		break;
	case OpCodeHandlerKind::XOP:
		data[0] = h.u.xop.table;
		data[1] = h.u.xop.last_byte;
		break;
	case OpCodeHandlerKind::EVEX:
		data[0] = h.u.evex.table;
		data[1] = h.u.evex.p1_bits;
		data[2] = h.u.evex.ll_bits;
		data[3] = h.u.evex.mask_w;
		data[4] = h.u.evex.mask_ll;
		data[5] = static_cast<std::uint32_t>(h.u.evex.tuple_type);
		break;
	case OpCodeHandlerKind::MVEX:
		data[0] = h.u.mvex.table;
		data[1] = h.u.mvex.p1_bits;
		data[2] = h.u.mvex.mask_w;
		break;
	case OpCodeHandlerKind::D3NOW:
		data[0] = h.u.d3now.immediate;
		break;
	}
}

} // namespace

TEST_CASE("encoder/op_code_handlers_table/matches_enc_flags") {
	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const Code code = static_cast<Code>(i);
		const ExpectedHandler expected = create_handler(code);
		const OpCodeHandler& h = OP_CODE_HANDLERS[i];
		std::uint32_t data[6];
		get_data(h, data);
		const bool ok = h.enc_flags3 == expected.enc_flags3 && h.op_code == expected.op_code && h.kind == expected.kind &&
			h.operands_len == expected.operands_len && h.group_index == expected.group_index && h.rm_group_index == expected.rm_group_index &&
			h.op_size == expected.op_size && h.addr_size == expected.addr_size && h.is_2byte_opcode == expected.is_2byte_opcode &&
			h.is_special_instr == expected.is_special_instr;
		CHECK_EQ(ok ? std::size_t(0) : i, std::size_t(0));
		if (h.kind != OpCodeHandlerKind::D3NOW) {
			for (std::uint32_t j = 0; j < OpCodeHandler::MAX_OPERANDS; j++) {
				const std::uint32_t op = j < h.operands_len ? h.operands[j] : 0;
				CHECK_EQ(op, expected.operands[j]);
			}
		}
		for (std::size_t j = 0; j < 6; j++)
			CHECK_EQ(data[j], expected.data[j]);
	}
}

TEST_CASE("encoder/op_code_handlers_table/d3now_operands") {
	const OpCodeHandler& pfadd = OP_CODE_HANDLERS[static_cast<std::size_t>(Code::D3NOW_Pfadd_mm_mmm64)];
	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const OpCodeHandler& h = OP_CODE_HANDLERS[i];
		if (h.kind != OpCodeHandlerKind::D3NOW)
			continue;
		CHECK_EQ(static_cast<std::uint32_t>(h.operands_len), 2U);
		CHECK_EQ(static_cast<std::uint32_t>(h.operands[0]), static_cast<std::uint32_t>(pfadd.operands[0]));
		CHECK_EQ(static_cast<std::uint32_t>(h.operands[1]), static_cast<std::uint32_t>(pfadd.operands[1]));
	}

	Encoder encoder(64);
	REQUIRE(encoder.encode(Instruction::with2(Code::D3NOW_Pfadd_mm_mmm64, Register::MM1, Register::MM7).value(), 0).is_ok());
	REQUIRE(encoder.encode(Instruction::with2(Code::D3NOW_Pfadd_mm_mmm64, Register::MM7, MemoryOperand::with_base(Register::RAX)).value(), 0).is_ok());
	const std::vector<std::uint8_t> expected = {0x0F, 0x0F, 0xCF, 0x9E, 0x0F, 0x0F, 0x38, 0x9E};
	CHECK(encoder.buffer() == expected);
	// Only MM0-MM7 are valid
	CHECK(encoder.encode(Instruction::with2(Code::D3NOW_Pfadd_mm_mmm64, Register::XMM1, Register::MM7).value(), 0).is_err());
	CHECK(encoder.encode(Instruction::with2(Code::D3NOW_Pfadd_mm_mmm64, Register::MM1, Register::XMM7).value(), 0).is_err());
}
