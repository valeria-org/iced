// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "iced_x86/op_code_info.hpp"
#include "iced_x86/code_ext.hpp"
#include "iced_x86/code_size.hpp"
#include "iced_x86/instruction.hpp"
#include "internal/encoder/dec_option_value.hpp"
#include "internal/encoder/enc_flags1.hpp"
#include "internal/encoder/enc_flags2.hpp"
#include "internal/encoder/enc_flags3.hpp"
#include "internal/encoder/encoder_data.hpp"
#include "internal/encoder/evex_op_code_table.hpp"
#include "internal/encoder/instruction_fmt.hpp"
#include "internal/encoder/l_bit.hpp"
#include "internal/encoder/l_kind.hpp"
#include "internal/encoder/legacy_op_code_table.hpp"
#include "internal/encoder/mvex_op_code_table.hpp"
#include "internal/encoder/op_code_data.hpp"
#include "internal/encoder/op_code_fmt.hpp"
#include "internal/encoder/op_code_info_flags1.hpp"
#include "internal/encoder/op_code_info_flags2.hpp"
#include "internal/encoder/op_kind_tables.hpp"
#include "internal/encoder/to_decoder_options.hpp"
#include "internal/encoder/vex_op_code_table.hpp"
#include "internal/encoder/w_bit.hpp"
#include "internal/encoder/xop_op_code_table.hpp"
#include "internal/iced_assert.hpp"
#include "internal/mandatory_prefix_byte.hpp"
#include "internal/mvex/mvex.hpp"
#include <memory>

namespace iced_x86 {

using internal::EncFlags1;
using internal::EncFlags2;
using internal::EncFlags3;
using internal::OpCodeInfoFlags1;
using internal::OpCodeInfoFlags2;

namespace internal {

struct OpCodeInfoInternal {
	static void init(OpCodeInfo& result, Code code, std::uint32_t enc_flags1, std::uint32_t enc_flags2, std::uint32_t enc_flags3,
		std::uint32_t opc_flags1, std::uint32_t opc_flags2, std::string& sb) {
		using Flags = OpCodeInfo::Flags;
		std::uint16_t flags = Flags::NONE;
		const std::uint16_t op_code = static_cast<std::uint16_t>(enc_flags2 >> EncFlags2::OP_CODE_SHIFT);

		if ((enc_flags1 & EncFlags1::IGNORES_ROUNDING_CONTROL) != 0)
			flags |= Flags::IGNORES_ROUNDING_CONTROL;
		if ((enc_flags1 & EncFlags1::AMD_LOCK_REG_BIT) != 0)
			flags |= Flags::AMD_LOCK_REG_BIT;
		switch (opc_flags1 & (OpCodeInfoFlags1::CPL0_ONLY | OpCodeInfoFlags1::CPL3_ONLY)) {
		case OpCodeInfoFlags1::CPL0_ONLY:
			flags |= Flags::CPL0;
			break;
		case OpCodeInfoFlags1::CPL3_ONLY:
			flags |= Flags::CPL3;
			break;
		default:
			flags |= Flags::CPL0 | Flags::CPL1 | Flags::CPL2 | Flags::CPL3;
			break;
		}

		OpCodeOperandKind op0_kind;
		OpCodeOperandKind op1_kind;
		OpCodeOperandKind op2_kind;
		OpCodeOperandKind op3_kind;
		OpCodeOperandKind op4_kind;
		std::uint8_t l;
		MandatoryPrefix mandatory_prefix;
		OpCodeTableKind table;
		LKind lkind;

		const EncodingKind encoding = static_cast<EncodingKind>((enc_flags3 >> EncFlags3::ENCODING_SHIFT) & EncFlags3::ENCODING_MASK);
		switch (static_cast<MandatoryPrefixByte>((enc_flags2 >> EncFlags2::MANDATORY_PREFIX_SHIFT) & EncFlags2::MANDATORY_PREFIX_MASK)) {
		case MandatoryPrefixByte::None:
			mandatory_prefix = (enc_flags2 & EncFlags2::HAS_MANDATORY_PREFIX) != 0 ? MandatoryPrefix::PNP : MandatoryPrefix::None;
			break;
		case MandatoryPrefixByte::P66:
			mandatory_prefix = MandatoryPrefix::P66;
			break;
		case MandatoryPrefixByte::PF3:
			mandatory_prefix = MandatoryPrefix::PF3;
			break;
		case MandatoryPrefixByte::PF2:
			mandatory_prefix = MandatoryPrefix::PF2;
			break;
		default:
			ICED_UNREACHABLE();
		}
		const std::uint8_t operand_size = code_size_to_bits(static_cast<CodeSize>((enc_flags3 >> EncFlags3::OPERAND_SIZE_SHIFT) & EncFlags3::OPERAND_SIZE_MASK));
		const std::uint8_t address_size = code_size_to_bits(static_cast<CodeSize>((enc_flags3 >> EncFlags3::ADDRESS_SIZE_SHIFT) & EncFlags3::ADDRESS_SIZE_MASK));
		const std::int8_t group_index =
			(enc_flags2 & EncFlags2::HAS_GROUP_INDEX) == 0 ? -1 : static_cast<std::int8_t>((enc_flags2 >> EncFlags2::GROUP_INDEX_SHIFT) & 7);
		const std::int8_t rm_group_index =
			(enc_flags3 & EncFlags3::HAS_RM_GROUP_INDEX) == 0 ? -1 : static_cast<std::int8_t>((enc_flags2 >> EncFlags2::GROUP_INDEX_SHIFT) & 7);
		const TupleType tuple_type = static_cast<TupleType>((enc_flags3 >> EncFlags3::TUPLE_TYPE_SHIFT) & EncFlags3::TUPLE_TYPE_MASK);

		switch (static_cast<LBit>((enc_flags2 >> EncFlags2::LBIT_SHIFT) & EncFlags2::LBIT_MASK)) {
		case LBit::LZ:
			lkind = LKind::LZ;
			l = 0;
			break;
		case LBit::L0:
			lkind = LKind::L0;
			l = 0;
			break;
		case LBit::L1:
			lkind = LKind::L0;
			l = 1;
			break;
		case LBit::L128:
			lkind = LKind::L128;
			l = 0;
			break;
		case LBit::L256:
			lkind = LKind::L128;
			l = 1;
			break;
		case LBit::L512:
			lkind = LKind::L128;
			l = 2;
			break;
		case LBit::LIG:
			lkind = LKind::None;
			l = 0;
			flags |= Flags::LIG;
			break;
		default:
			ICED_UNREACHABLE();
		}

		switch (static_cast<WBit>((enc_flags2 >> EncFlags2::WBIT_SHIFT) & EncFlags2::WBIT_MASK)) {
		case WBit::W0:
			break;
		case WBit::W1:
			flags |= Flags::W;
			break;
		case WBit::WIG:
			flags |= Flags::WIG;
			break;
		case WBit::WIG32:
			flags |= Flags::WIG32;
			break;
		default:
			ICED_UNREACHABLE();
		}

		const std::uint32_t table_index = (enc_flags2 >> EncFlags2::TABLE_SHIFT) & EncFlags2::TABLE_MASK;
		switch (encoding) {
		case EncodingKind::Legacy:
			op0_kind = LEGACY_OP_KINDS[(enc_flags1 >> EncFlags1::LEGACY_OP0_SHIFT) & EncFlags1::LEGACY_OP_MASK];
			op1_kind = LEGACY_OP_KINDS[(enc_flags1 >> EncFlags1::LEGACY_OP1_SHIFT) & EncFlags1::LEGACY_OP_MASK];
			op2_kind = LEGACY_OP_KINDS[(enc_flags1 >> EncFlags1::LEGACY_OP2_SHIFT) & EncFlags1::LEGACY_OP_MASK];
			op3_kind = LEGACY_OP_KINDS[(enc_flags1 >> EncFlags1::LEGACY_OP3_SHIFT) & EncFlags1::LEGACY_OP_MASK];
			op4_kind = OpCodeOperandKind::None;

			switch (static_cast<LegacyOpCodeTable>(table_index)) {
			case LegacyOpCodeTable::MAP0:
				table = OpCodeTableKind::Normal;
				break;
			case LegacyOpCodeTable::MAP0F:
				table = OpCodeTableKind::T0F;
				break;
			case LegacyOpCodeTable::MAP0F38:
				table = OpCodeTableKind::T0F38;
				break;
			case LegacyOpCodeTable::MAP0F3A:
				table = OpCodeTableKind::T0F3A;
				break;
			default:
				ICED_UNREACHABLE();
			}
			break;

		case EncodingKind::VEX:
			op0_kind = VEX_OP_KINDS[(enc_flags1 >> EncFlags1::VEX_OP0_SHIFT) & EncFlags1::VEX_OP_MASK];
			op1_kind = VEX_OP_KINDS[(enc_flags1 >> EncFlags1::VEX_OP1_SHIFT) & EncFlags1::VEX_OP_MASK];
			op2_kind = VEX_OP_KINDS[(enc_flags1 >> EncFlags1::VEX_OP2_SHIFT) & EncFlags1::VEX_OP_MASK];
			op3_kind = VEX_OP_KINDS[(enc_flags1 >> EncFlags1::VEX_OP3_SHIFT) & EncFlags1::VEX_OP_MASK];
			op4_kind = VEX_OP_KINDS[(enc_flags1 >> EncFlags1::VEX_OP4_SHIFT) & EncFlags1::VEX_OP_MASK];

			switch (static_cast<VexOpCodeTable>(table_index)) {
			case VexOpCodeTable::MAP0:
				table = OpCodeTableKind::Normal;
				break;
			case VexOpCodeTable::MAP0F:
				table = OpCodeTableKind::T0F;
				break;
			case VexOpCodeTable::MAP0F38:
				table = OpCodeTableKind::T0F38;
				break;
			case VexOpCodeTable::MAP0F3A:
				table = OpCodeTableKind::T0F3A;
				break;
			default:
				ICED_UNREACHABLE();
			}
			break;

		case EncodingKind::EVEX:
			op0_kind = EVEX_OP_KINDS[(enc_flags1 >> EncFlags1::EVEX_OP0_SHIFT) & EncFlags1::EVEX_OP_MASK];
			op1_kind = EVEX_OP_KINDS[(enc_flags1 >> EncFlags1::EVEX_OP1_SHIFT) & EncFlags1::EVEX_OP_MASK];
			op2_kind = EVEX_OP_KINDS[(enc_flags1 >> EncFlags1::EVEX_OP2_SHIFT) & EncFlags1::EVEX_OP_MASK];
			op3_kind = EVEX_OP_KINDS[(enc_flags1 >> EncFlags1::EVEX_OP3_SHIFT) & EncFlags1::EVEX_OP_MASK];
			op4_kind = OpCodeOperandKind::None;

			switch (static_cast<EvexOpCodeTable>(table_index)) {
			case EvexOpCodeTable::MAP0F:
				table = OpCodeTableKind::T0F;
				break;
			case EvexOpCodeTable::MAP0F38:
				table = OpCodeTableKind::T0F38;
				break;
			case EvexOpCodeTable::MAP0F3A:
				table = OpCodeTableKind::T0F3A;
				break;
			case EvexOpCodeTable::MAP5:
				table = OpCodeTableKind::MAP5;
				break;
			case EvexOpCodeTable::MAP6:
				table = OpCodeTableKind::MAP6;
				break;
			default:
				ICED_UNREACHABLE();
			}
			break;

		case EncodingKind::XOP:
			op0_kind = XOP_OP_KINDS[(enc_flags1 >> EncFlags1::XOP_OP0_SHIFT) & EncFlags1::XOP_OP_MASK];
			op1_kind = XOP_OP_KINDS[(enc_flags1 >> EncFlags1::XOP_OP1_SHIFT) & EncFlags1::XOP_OP_MASK];
			op2_kind = XOP_OP_KINDS[(enc_flags1 >> EncFlags1::XOP_OP2_SHIFT) & EncFlags1::XOP_OP_MASK];
			op3_kind = XOP_OP_KINDS[(enc_flags1 >> EncFlags1::XOP_OP3_SHIFT) & EncFlags1::XOP_OP_MASK];
			op4_kind = OpCodeOperandKind::None;

			switch (static_cast<XopOpCodeTable>(table_index)) {
			case XopOpCodeTable::MAP8:
				table = OpCodeTableKind::MAP8;
				break;
			case XopOpCodeTable::MAP9:
				table = OpCodeTableKind::MAP9;
				break;
			case XopOpCodeTable::MAP10:
				table = OpCodeTableKind::MAP10;
				break;
			default:
				ICED_UNREACHABLE();
			}
			break;

		case EncodingKind::D3NOW:
			op0_kind = OpCodeOperandKind::mm_reg;
			op1_kind = OpCodeOperandKind::mm_or_mem;
			op2_kind = OpCodeOperandKind::None;
			op3_kind = OpCodeOperandKind::None;
			op4_kind = OpCodeOperandKind::None;
			table = OpCodeTableKind::T0F;
			break;

		case EncodingKind::MVEX:
			op0_kind = MVEX_OP_KINDS[(enc_flags1 >> EncFlags1::MVEX_OP0_SHIFT) & EncFlags1::MVEX_OP_MASK];
			op1_kind = MVEX_OP_KINDS[(enc_flags1 >> EncFlags1::MVEX_OP1_SHIFT) & EncFlags1::MVEX_OP_MASK];
			op2_kind = MVEX_OP_KINDS[(enc_flags1 >> EncFlags1::MVEX_OP2_SHIFT) & EncFlags1::MVEX_OP_MASK];
			op3_kind = MVEX_OP_KINDS[(enc_flags1 >> EncFlags1::MVEX_OP3_SHIFT) & EncFlags1::MVEX_OP_MASK];
			op4_kind = OpCodeOperandKind::None;

			switch (static_cast<MvexOpCodeTable>(table_index)) {
			case MvexOpCodeTable::MAP0F:
				table = OpCodeTableKind::T0F;
				break;
			case MvexOpCodeTable::MAP0F38:
				table = OpCodeTableKind::T0F38;
				break;
			case MvexOpCodeTable::MAP0F3A:
				table = OpCodeTableKind::T0F3A;
				break;
			default:
				ICED_UNREACHABLE();
			}
			break;

		default:
			ICED_UNREACHABLE();
		}

		result.enc_flags2_ = enc_flags2;
		result.enc_flags3_ = enc_flags3;
		result.opc_flags1_ = opc_flags1;
		result.opc_flags2_ = opc_flags2;
		result.code_ = code;
		result.op_code_ = op_code;
		result.flags_ = flags;
		result.encoding_ = encoding;
		result.operand_size_ = operand_size;
		result.address_size_ = address_size;
		result.l_ = l;
		result.tuple_type_ = tuple_type;
		result.table_ = table;
		result.mandatory_prefix_ = mandatory_prefix;
		result.group_index_ = group_index;
		result.rm_group_index_ = rm_group_index;
		result.op_kinds_[0] = op0_kind;
		result.op_kinds_[1] = op1_kind;
		result.op_kinds_[2] = op2_kind;
		result.op_kinds_[3] = op3_kind;
		result.op_kinds_[4] = op4_kind;

		result.op_code_string_ = OpCodeFormatter(result, sb, lkind, (opc_flags1 & OpCodeInfoFlags1::MOD_REG_RM_STRING) != 0).format();
		const InstrStrFmtOption fmt_opt =
			static_cast<InstrStrFmtOption>((opc_flags2 >> OpCodeInfoFlags2::INSTR_STR_FMT_OPTION_SHIFT) & OpCodeInfoFlags2::INSTR_STR_FMT_OPTION_MASK);
		result.instruction_string_ = InstructionFormatter(result, fmt_opt, sb).format();
	}

	static std::uint8_t code_size_to_bits(CodeSize code_size) noexcept {
		switch (code_size) {
		case CodeSize::Unknown:
			return 0;
		case CodeSize::Code16:
			return 16;
		case CodeSize::Code32:
			return 32;
		case CodeSize::Code64:
			return 64;
		default:
			ICED_UNREACHABLE();
		}
	}

	struct Table {
		std::unique_ptr<OpCodeInfo[]> infos;

		Table() : infos(new OpCodeInfo[IcedConstants::CODE_ENUM_COUNT]) {
			std::string sb;
			for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++)
				init(infos[i], static_cast<Code>(i), ENC_FLAGS1[i], ENC_FLAGS2[i], ENC_FLAGS3[i], OPC_FLAGS1[i], OPC_FLAGS2[i], sb);
		}
	};

	static const OpCodeInfo* get_table() noexcept {
		static const Table table;
		return table.infos.get();
	}
};

} // namespace internal

OpCodeOperandKind OpCodeInfo::invalid_operand_op_kind() noexcept {
	ICED_DEBUG_ASSERT(false); // Invalid operand
	return OpCodeOperandKind::None;
}

Mnemonic OpCodeInfo::mnemonic() const noexcept { return code_ext::mnemonic(code_); }

MvexEHBit OpCodeInfo::mvex_eh_bit() const noexcept {
	if (encoding() == EncodingKind::MVEX)
		return internal::get_mvex_info(code()).eh_bit;
	return MvexEHBit::None;
}

bool OpCodeInfo::mvex_can_use_eviction_hint() const noexcept {
	if (encoding() == EncodingKind::MVEX)
		return internal::get_mvex_info(code()).can_use_eviction_hint();
	return false;
}

bool OpCodeInfo::mvex_can_use_imm_rounding_control() const noexcept {
	if (encoding() == EncodingKind::MVEX)
		return internal::get_mvex_info(code()).can_use_imm_rounding_control();
	return false;
}

bool OpCodeInfo::mvex_ignores_op_mask_register() const noexcept {
	if (encoding() == EncodingKind::MVEX)
		return internal::get_mvex_info(code()).ignores_op_mask_register();
	return false;
}

bool OpCodeInfo::mvex_no_sae_rc() const noexcept {
	if (encoding() == EncodingKind::MVEX)
		return internal::get_mvex_info(code()).no_sae_rc();
	return false;
}

MvexTupleTypeLutKind OpCodeInfo::mvex_tuple_type_lut_kind() const noexcept {
	if (encoding() == EncodingKind::MVEX)
		return internal::get_mvex_info(code()).tuple_type_lut_kind;
	return static_cast<MvexTupleTypeLutKind>(0);
}

MvexConvFn OpCodeInfo::mvex_conversion_func() const noexcept {
	if (encoding() == EncodingKind::MVEX)
		return internal::get_mvex_info(code()).conv_fn;
	return MvexConvFn::None;
}

std::uint8_t OpCodeInfo::mvex_valid_conversion_funcs_mask() const noexcept {
	if (encoding() == EncodingKind::MVEX)
		return static_cast<std::uint8_t>(~internal::get_mvex_info(code()).invalid_conv_fns);
	return 0;
}

std::uint8_t OpCodeInfo::mvex_valid_swizzle_funcs_mask() const noexcept {
	if (encoding() == EncodingKind::MVEX)
		return static_cast<std::uint8_t>(~internal::get_mvex_info(code()).invalid_swizzle_fns);
	return 0;
}

MemorySize OpCodeInfo::memory_size() const noexcept { return internal::SIZES_NORMAL[static_cast<std::size_t>(code())]; }

MemorySize OpCodeInfo::broadcast_memory_size() const noexcept { return internal::SIZES_BCST[static_cast<std::size_t>(code())]; }

std::uint32_t OpCodeInfo::decoder_option() const noexcept {
	const std::uint32_t dec_opt_value = (opc_flags1_ >> OpCodeInfoFlags1::DEC_OPTION_VALUE_SHIFT) & OpCodeInfoFlags1::DEC_OPTION_VALUE_MASK;
	return internal::TO_DECODER_OPTIONS[dec_opt_value];
}

std::uint32_t OpCodeInfo::op_code_len() const noexcept { return (enc_flags2_ & EncFlags2::OP_CODE_IS2_BYTES) != 0 ? 2 : 1; }

std::uint32_t OpCodeInfo::op_count() const noexcept { return internal::OP_COUNT[static_cast<std::size_t>(code())]; }

Result<OpCodeOperandKind> OpCodeInfo::try_op_kind(std::uint32_t operand) const {
	if (operand < MAX_OP_COUNT)
		return op_kinds_[operand];
	return IcedError("Invalid operand");
}

bool OpCodeInfo::is_available_in_mode(std::uint32_t bitness) const noexcept {
	switch (bitness) {
	case 16:
		return mode16();
	case 32:
		return mode32();
	case 64:
		return mode64();
	default:
		return false;
	}
}

bool OpCodeInfo::mode16() const noexcept { return (enc_flags3_ & EncFlags3::BIT16OR32) != 0; }
bool OpCodeInfo::mode32() const noexcept { return (enc_flags3_ & EncFlags3::BIT16OR32) != 0; }
bool OpCodeInfo::mode64() const noexcept { return (enc_flags3_ & EncFlags3::BIT64) != 0; }
bool OpCodeInfo::fwait() const noexcept { return (enc_flags3_ & EncFlags3::FWAIT) != 0; }
bool OpCodeInfo::can_broadcast() const noexcept { return (enc_flags3_ & EncFlags3::BROADCAST) != 0; }
bool OpCodeInfo::can_use_rounding_control() const noexcept { return (enc_flags3_ & EncFlags3::ROUNDING_CONTROL) != 0; }
bool OpCodeInfo::can_suppress_all_exceptions() const noexcept { return (enc_flags3_ & EncFlags3::SUPPRESS_ALL_EXCEPTIONS) != 0; }
bool OpCodeInfo::can_use_op_mask_register() const noexcept { return (enc_flags3_ & EncFlags3::OP_MASK_REGISTER) != 0; }
bool OpCodeInfo::require_op_mask_register() const noexcept { return (enc_flags3_ & EncFlags3::REQUIRE_OP_MASK_REGISTER) != 0; }
bool OpCodeInfo::can_use_zeroing_masking() const noexcept { return (enc_flags3_ & EncFlags3::ZEROING_MASKING) != 0; }
bool OpCodeInfo::can_use_lock_prefix() const noexcept { return (enc_flags3_ & EncFlags3::LOCK) != 0; }
bool OpCodeInfo::can_use_xacquire_prefix() const noexcept { return (enc_flags3_ & EncFlags3::XACQUIRE) != 0; }
bool OpCodeInfo::can_use_xrelease_prefix() const noexcept { return (enc_flags3_ & EncFlags3::XRELEASE) != 0; }
bool OpCodeInfo::can_use_rep_prefix() const noexcept { return (enc_flags3_ & EncFlags3::REP) != 0; }
bool OpCodeInfo::can_use_repne_prefix() const noexcept { return (enc_flags3_ & EncFlags3::REPNE) != 0; }
bool OpCodeInfo::can_use_bnd_prefix() const noexcept { return (enc_flags3_ & EncFlags3::BND) != 0; }
bool OpCodeInfo::can_use_hint_taken_prefix() const noexcept { return (enc_flags3_ & EncFlags3::HINT_TAKEN) != 0; }
bool OpCodeInfo::can_use_notrack_prefix() const noexcept { return (enc_flags3_ & EncFlags3::NOTRACK) != 0; }
bool OpCodeInfo::default_op_size64() const noexcept { return (enc_flags3_ & EncFlags3::DEFAULT_OP_SIZE64) != 0; }
bool OpCodeInfo::force_op_size64() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::FORCE_OP_SIZE64) != 0; }
bool OpCodeInfo::intel_force_op_size64() const noexcept { return (enc_flags3_ & EncFlags3::INTEL_FORCE_OP_SIZE64) != 0; }
bool OpCodeInfo::is_input_output() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::INPUT_OUTPUT) != 0; }
bool OpCodeInfo::is_nop() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::NOP) != 0; }
bool OpCodeInfo::is_reserved_nop() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::RESERVED_NOP) != 0; }
bool OpCodeInfo::is_serializing_intel() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::SERIALIZING_INTEL) != 0; }
bool OpCodeInfo::is_serializing_amd() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::SERIALIZING_AMD) != 0; }
bool OpCodeInfo::may_require_cpl0() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::MAY_REQUIRE_CPL0) != 0; }
bool OpCodeInfo::is_cet_tracked() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::CET_TRACKED) != 0; }
bool OpCodeInfo::is_non_temporal() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::NON_TEMPORAL) != 0; }
bool OpCodeInfo::is_fpu_no_wait() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::FPU_NO_WAIT) != 0; }
bool OpCodeInfo::ignores_mod_bits() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::IGNORES_MOD_BITS) != 0; }
bool OpCodeInfo::no66() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::NO66) != 0; }
bool OpCodeInfo::nfx() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::NFX) != 0; }
bool OpCodeInfo::requires_unique_reg_nums() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::REQUIRES_UNIQUE_REG_NUMS) != 0; }
bool OpCodeInfo::requires_unique_dest_reg_num() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::REQUIRES_UNIQUE_DEST_REG_NUM) != 0; }
bool OpCodeInfo::is_privileged() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::PRIVILEGED) != 0; }
bool OpCodeInfo::is_save_restore() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::SAVE_RESTORE) != 0; }
bool OpCodeInfo::is_stack_instruction() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::STACK_INSTRUCTION) != 0; }
bool OpCodeInfo::ignores_segment() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::IGNORES_SEGMENT) != 0; }
bool OpCodeInfo::is_op_mask_read_write() const noexcept { return (opc_flags1_ & OpCodeInfoFlags1::OP_MASK_READ_WRITE) != 0; }
bool OpCodeInfo::real_mode() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::REAL_MODE) != 0; }
bool OpCodeInfo::protected_mode() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::PROTECTED_MODE) != 0; }
bool OpCodeInfo::virtual8086_mode() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::VIRTUAL8086_MODE) != 0; }
bool OpCodeInfo::compatibility_mode() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::COMPATIBILITY_MODE) != 0; }
bool OpCodeInfo::long_mode() const noexcept { return (enc_flags3_ & EncFlags3::BIT64) != 0; }
bool OpCodeInfo::use_outside_smm() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_OUTSIDE_SMM) != 0; }
bool OpCodeInfo::use_in_smm() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_IN_SMM) != 0; }
bool OpCodeInfo::use_outside_enclave_sgx() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_OUTSIDE_ENCLAVE_SGX) != 0; }
bool OpCodeInfo::use_in_enclave_sgx1() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_IN_ENCLAVE_SGX1) != 0; }
bool OpCodeInfo::use_in_enclave_sgx2() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_IN_ENCLAVE_SGX2) != 0; }
bool OpCodeInfo::use_outside_vmx_op() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_OUTSIDE_VMX_OP) != 0; }
bool OpCodeInfo::use_in_vmx_root_op() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_IN_VMX_ROOT_OP) != 0; }
bool OpCodeInfo::use_in_vmx_non_root_op() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_IN_VMX_NON_ROOT_OP) != 0; }
bool OpCodeInfo::use_outside_seam() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_OUTSIDE_SEAM) != 0; }
bool OpCodeInfo::use_in_seam() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::USE_IN_SEAM) != 0; }
bool OpCodeInfo::tdx_non_root_gen_ud() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::TDX_NON_ROOT_GEN_UD) != 0; }
bool OpCodeInfo::tdx_non_root_gen_ve() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::TDX_NON_ROOT_GEN_VE) != 0; }
bool OpCodeInfo::tdx_non_root_may_gen_ex() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::TDX_NON_ROOT_MAY_GEN_EX) != 0; }
bool OpCodeInfo::intel_vm_exit() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::INTEL_VM_EXIT) != 0; }
bool OpCodeInfo::intel_may_vm_exit() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::INTEL_MAY_VM_EXIT) != 0; }
bool OpCodeInfo::intel_smm_vm_exit() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::INTEL_SMM_VM_EXIT) != 0; }
bool OpCodeInfo::amd_vm_exit() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::AMD_VM_EXIT) != 0; }
bool OpCodeInfo::amd_may_vm_exit() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::AMD_MAY_VM_EXIT) != 0; }
bool OpCodeInfo::tsx_abort() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::TSX_ABORT) != 0; }
bool OpCodeInfo::tsx_impl_abort() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::TSX_IMPL_ABORT) != 0; }
bool OpCodeInfo::tsx_may_abort() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::TSX_MAY_ABORT) != 0; }
bool OpCodeInfo::intel_decoder16() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::INTEL_DECODER16OR32) != 0; }
bool OpCodeInfo::intel_decoder32() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::INTEL_DECODER16OR32) != 0; }
bool OpCodeInfo::intel_decoder64() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::INTEL_DECODER64) != 0; }
bool OpCodeInfo::amd_decoder16() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::AMD_DECODER16OR32) != 0; }
bool OpCodeInfo::amd_decoder32() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::AMD_DECODER16OR32) != 0; }
bool OpCodeInfo::amd_decoder64() const noexcept { return (opc_flags2_ & OpCodeInfoFlags2::AMD_DECODER64) != 0; }

namespace code_ext {
const OpCodeInfo& op_code(Code code) noexcept { return internal::OpCodeInfoInternal::get_table()[static_cast<std::size_t>(code)]; }
} // namespace code_ext

} // namespace iced_x86
