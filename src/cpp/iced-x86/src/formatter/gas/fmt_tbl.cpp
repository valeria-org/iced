// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/info.rs + formatter/gas/fmt_tbl.rs

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "iced_x86/code.hpp"
#include "iced_x86/code_size.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/rounding_control.hpp"
#include "internal/data_reader.hpp"
#include "internal/formatter/fmt_common.hpp"
#include "internal/formatter/fmt_utils.hpp"
#include "internal/formatter/gas/ctor_kind.hpp"
#include "internal/formatter/gas/fmt_data.hpp"
#include "internal/formatter/gas/info.hpp"
#include "internal/formatter/gas/instr_op_info_flags.hpp"
#include "internal/formatter/gas/mem_size_tbl.hpp"
#include "internal/formatter/pseudo_ops.hpp"
#include "internal/formatter/strings_tbl.hpp"
#include "internal/iced_assert.hpp"

namespace iced_x86::internal::gas {

static_assert(IcedConstants::MAX_OP_COUNT == 5, "");

InstrOpInfo::InstrOpInfo(const FormatterString& mnemonic_, const Instruction& instruction, std::uint32_t flags_) noexcept
	: InstrOpInfo(mnemonic_) {
	flags = static_cast<std::uint16_t>(flags_);
	const std::uint32_t instr_op_count = instruction.op_count();
	op_count = static_cast<std::uint8_t>(instr_op_count);
	if ((flags_ & InstrOpInfoFlags::KEEP_OPERAND_ORDER) != 0) {
		op_kinds[0] = to_instr_op_kind(instruction.op0_kind());
		op_kinds[1] = to_instr_op_kind(instruction.op1_kind());
		op_kinds[2] = to_instr_op_kind(instruction.op2_kind());
		op_kinds[3] = to_instr_op_kind(instruction.op3_kind());
		op_kinds[4] = to_instr_op_kind(instruction.op4_kind());
		op_registers[0] = instruction.op0_register();
		op_registers[1] = instruction.op1_register();
		op_registers[2] = instruction.op2_register();
		op_registers[3] = instruction.op3_register();
		op_registers[4] = instruction.op4_register();
	} else {
		switch (instr_op_count) {
		case 0:
			break;

		case 1:
			op_kinds[0] = to_instr_op_kind(instruction.op0_kind());
			op_registers[0] = instruction.op0_register();
			break;

		case 2:
			op_kinds[0] = to_instr_op_kind(instruction.op1_kind());
			op_kinds[1] = to_instr_op_kind(instruction.op0_kind());
			op_registers[0] = instruction.op1_register();
			op_registers[1] = instruction.op0_register();
			break;

		case 3:
			op_kinds[0] = to_instr_op_kind(instruction.op2_kind());
			op_kinds[1] = to_instr_op_kind(instruction.op1_kind());
			op_kinds[2] = to_instr_op_kind(instruction.op0_kind());
			op_registers[0] = instruction.op2_register();
			op_registers[1] = instruction.op1_register();
			op_registers[2] = instruction.op0_register();
			break;

		case 4:
			op_kinds[0] = to_instr_op_kind(instruction.op3_kind());
			op_kinds[1] = to_instr_op_kind(instruction.op2_kind());
			op_kinds[2] = to_instr_op_kind(instruction.op1_kind());
			op_kinds[3] = to_instr_op_kind(instruction.op0_kind());
			op_registers[0] = instruction.op3_register();
			op_registers[1] = instruction.op2_register();
			op_registers[2] = instruction.op1_register();
			op_registers[3] = instruction.op0_register();
			break;

		case 5:
			op_kinds[0] = to_instr_op_kind(instruction.op4_kind());
			op_kinds[1] = to_instr_op_kind(instruction.op3_kind());
			op_kinds[2] = to_instr_op_kind(instruction.op2_kind());
			op_kinds[3] = to_instr_op_kind(instruction.op1_kind());
			op_kinds[4] = to_instr_op_kind(instruction.op0_kind());
			op_registers[0] = instruction.op4_register();
			op_registers[1] = instruction.op3_register();
			op_registers[2] = instruction.op2_register();
			op_registers[3] = instruction.op1_register();
			op_registers[4] = instruction.op0_register();
			break;

		default:
			ICED_UNREACHABLE();
		}
	}
	switch (op_count) {
	case 0:
		op_indexes[0] = OP_ACCESS_INVALID;
		op_indexes[1] = OP_ACCESS_INVALID;
		op_indexes[2] = OP_ACCESS_INVALID;
		op_indexes[3] = OP_ACCESS_INVALID;
		op_indexes[4] = OP_ACCESS_INVALID;
		break;

	case 1:
		op_indexes[1] = OP_ACCESS_INVALID;
		op_indexes[2] = OP_ACCESS_INVALID;
		op_indexes[3] = OP_ACCESS_INVALID;
		op_indexes[4] = OP_ACCESS_INVALID;
		break;

	case 2:
		op_indexes[0] = 1;
		op_indexes[2] = OP_ACCESS_INVALID;
		op_indexes[3] = OP_ACCESS_INVALID;
		op_indexes[4] = OP_ACCESS_INVALID;
		break;

	case 3:
		op_indexes[0] = 2;
		op_indexes[1] = 1;
		op_indexes[3] = OP_ACCESS_INVALID;
		op_indexes[4] = OP_ACCESS_INVALID;
		break;

	case 4:
		op_indexes[0] = 3;
		op_indexes[1] = 2;
		op_indexes[2] = 1;
		op_indexes[4] = OP_ACCESS_INVALID;
		break;

	case 5:
		op_indexes[0] = 4;
		op_indexes[1] = 3;
		op_indexes[2] = 2;
		op_indexes[3] = 1;
		break;

	default:
		ICED_UNREACHABLE();
	}
}

namespace {

std::uint32_t get_bitness(CodeSize code_size) noexcept {
	static constexpr std::uint32_t CODESIZE_TO_BITNESS[4] = {0, 16, 32, 64};
	static_assert(static_cast<std::uint32_t>(CodeSize::Unknown) == 0, "");
	static_assert(static_cast<std::uint32_t>(CodeSize::Code16) == 1, "");
	static_assert(static_cast<std::uint32_t>(CodeSize::Code32) == 2, "");
	static_assert(static_cast<std::uint32_t>(CodeSize::Code64) == 3, "");
	return CODESIZE_TO_BITNESS[static_cast<std::size_t>(code_size) & 3];
}

const FormatterString& get_mnemonic(const FormatterOptions& options, const Instruction& instruction, const FormatterString& mnemonic,
									const FormatterString& mnemonic_suffix, std::uint32_t flags) noexcept {
	if (options.gas_show_mnemonic_size_suffix())
		return mnemonic_suffix;
	if ((flags & InstrOpInfoFlags::MNEMONIC_SUFFIX_IF_MEM) != 0 &&
		get_mem_size_tbl()[static_cast<std::size_t>(instruction.memory_size())]->is_default()) {
		if (instruction.op0_kind() == OpKind::Memory || instruction.op1_kind() == OpKind::Memory || instruction.op2_kind() == OpKind::Memory)
			return mnemonic_suffix;
	}
	return mnemonic;
}

std::vector<FormatterString> to_formatter_strings(std::vector<std::string> strings) {
	return FormatterString::with_strings(std::move(strings));
}

class SimpleInstrInfo final : public InstrInfo {
public:
	SimpleInstrInfo(std::string mnemonic, std::string mnemonic_suffix, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		return InstrOpInfo(get_mnemonic(options, instruction, mnemonic_, mnemonic_suffix_, flags_), instruction, flags_);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_cc final : public InstrInfo {
public:
	SimpleInstrInfo_cc(std::uint32_t cc_index, std::vector<std::string> mnemonics, std::vector<std::string> mnemonics_suffix)
		: mnemonics_(to_formatter_strings(std::move(mnemonics))), mnemonics_suffix_(to_formatter_strings(std::move(mnemonics_suffix))),
		  cc_index_(cc_index) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		const FormatterString& mnemonic = get_mnemonic_cc(options, cc_index_, mnemonics_);
		const FormatterString& mnemonic_suffix = get_mnemonic_cc(options, cc_index_, mnemonics_suffix_);
		return InstrOpInfo(get_mnemonic(options, instruction, mnemonic, mnemonic_suffix, FLAGS), instruction, FLAGS);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::vector<FormatterString> mnemonics_suffix_;
	std::uint32_t cc_index_;
};

class SimpleInstrInfo_AamAad final : public InstrInfo {
public:
	explicit SimpleInstrInfo_AamAad(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		if (instruction.immediate8() == 10)
			return InstrOpInfo(mnemonic_);
		return InstrOpInfo(mnemonic_, instruction, InstrOpInfoFlags::NONE);
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_nop final : public InstrInfo {
public:
	SimpleInstrInfo_nop(std::uint32_t bitness, std::string mnemonic, Register register_)
		: mnemonic_(std::move(mnemonic)), bitness_(bitness), register_(register_), str_xchg_("xchg"), str_xchgw_("xchgw"), str_xchgl_("xchgl"),
		  str_xchgq_("xchgq") {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (instr_bitness == 0 || (instr_bitness & bitness_) != 0)
			return InstrOpInfo(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		const FormatterString* mnemonic;
		if (!options.gas_show_mnemonic_size_suffix())
			mnemonic = &str_xchg_;
		else if (register_ == Register::AX)
			mnemonic = &str_xchgw_;
		else if (register_ == Register::EAX)
			mnemonic = &str_xchgl_;
		else if (register_ == Register::RAX)
			mnemonic = &str_xchgq_;
		else
			ICED_UNREACHABLE();
		InstrOpInfo info(*mnemonic);
		info.op_count = 2;
		static_assert(static_cast<std::uint32_t>(InstrOpKind::Register) == 0, "");
		// info.op_kinds[0] = InstrOpKind::Register;
		// info.op_kinds[1] = InstrOpKind::Register;
		info.op_registers[0] = register_;
		info.op_registers[1] = register_;
		info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_NONE;
		info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_NONE;
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
	Register register_;
	FormatterString str_xchg_;
	FormatterString str_xchgw_;
	FormatterString str_xchgl_;
	FormatterString str_xchgq_;
};

class SimpleInstrInfo_STIG1 final : public InstrInfo {
public:
	SimpleInstrInfo_STIG1(std::string mnemonic, bool pseudo_op) : mnemonic_(std::move(mnemonic)), pseudo_op_(pseudo_op) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		InstrOpInfo info(mnemonic_);
		ICED_DEBUG_ASSERT(instruction.op_count() == 2);
		ICED_DEBUG_ASSERT(instruction.op0_kind() == OpKind::Register && instruction.op0_register() == Register::ST0);
		if (!pseudo_op_ || !(options.use_pseudo_ops() && instruction.op1_register() == Register::ST1)) {
			info.op_count = 1;
			static_assert(static_cast<std::uint32_t>(InstrOpKind::Register) == 0, "");
			// info.op_kinds[0] = InstrOpKind::Register;
			info.op_registers[0] = instruction.op1_register();
			info.op_indexes[0] = 1;
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	bool pseudo_op_;
};

class SimpleInstrInfo_STi_ST final : public InstrInfo {
public:
	SimpleInstrInfo_STi_ST(std::string mnemonic, bool pseudo_op) : mnemonic_(std::move(mnemonic)), pseudo_op_(pseudo_op) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		if (pseudo_op_ && options.use_pseudo_ops() && (instruction.op0_register() == Register::ST1 || instruction.op1_register() == Register::ST1))
			return InstrOpInfo(mnemonic_);
		InstrOpInfo info(mnemonic_, instruction, FLAGS);
		ICED_DEBUG_ASSERT(info.op_registers[0] == Register::ST0);
		info.op_registers[0] = REGISTER_ST;
		return info;
	}

private:
	FormatterString mnemonic_;
	bool pseudo_op_;
};

class SimpleInstrInfo_ST_STi final : public InstrInfo {
public:
	explicit SimpleInstrInfo_ST_STi(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		ICED_DEBUG_ASSERT(info.op_registers[1] == Register::ST0);
		info.op_registers[1] = REGISTER_ST;
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_as final : public InstrInfo {
public:
	SimpleInstrInfo_as(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = 0;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::ADDR_SIZE16;
			else if (bitness_ == 32)
				flags |= InstrOpInfoFlags::ADDR_SIZE32;
			else
				flags |= InstrOpInfoFlags::ADDR_SIZE64;
		}
		return InstrOpInfo(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_maskmovq final : public InstrInfo {
public:
	explicit SimpleInstrInfo_maskmovq(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		ICED_DEBUG_ASSERT(instruction.op_count() == 3);

		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());

		std::uint32_t bitness;
		switch (instruction.op0_kind()) {
		case OpKind::MemorySegDI:
			bitness = 16;
			break;
		case OpKind::MemorySegEDI:
			bitness = 32;
			break;
		case OpKind::MemorySegRDI:
			bitness = 64;
			break;
		default:
			bitness = instr_bitness;
			break;
		}

		InstrOpInfo info(mnemonic_);
		info.op_count = 2;
		info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op2_kind());
		info.op_registers[0] = instruction.op2_register();
		info.op_indexes[0] = 2;
		info.op_kinds[1] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
		info.op_registers[1] = instruction.op1_register();
		info.op_indexes[1] = 1;
		if (instr_bitness != 0 && instr_bitness != bitness) {
			if (bitness == 16)
				info.flags |= static_cast<std::uint16_t>(InstrOpInfoFlags::ADDR_SIZE16);
			else if (bitness == 32)
				info.flags |= static_cast<std::uint16_t>(InstrOpInfoFlags::ADDR_SIZE32);
			else
				info.flags |= static_cast<std::uint16_t>(InstrOpInfoFlags::ADDR_SIZE64);
		}
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_pblendvb final : public InstrInfo {
public:
	explicit SimpleInstrInfo_pblendvb(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_);
		ICED_DEBUG_ASSERT(instruction.op_count() == 2);
		info.op_count = 3;
		static_assert(static_cast<std::uint32_t>(InstrOpKind::Register) == 0, "");
		// info.op_kinds[0] = InstrOpKind::Register;
		info.op_registers[0] = Register::XMM0;
		info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_kinds[1] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
		info.op_indexes[1] = 1;
		info.op_registers[1] = instruction.op1_register();
		info.op_kinds[2] = InstrOpInfo::to_instr_op_kind(instruction.op0_kind());
		info.op_registers[2] = instruction.op0_register();
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_OpSize final : public InstrInfo {
public:
	SimpleInstrInfo_OpSize(CodeSize code_size, std::string mnemonic, std::string mnemonic16, std::string mnemonic32, std::string mnemonic64)
		: mnemonics_{FormatterString(std::move(mnemonic)), FormatterString(std::move(mnemonic16)), FormatterString(std::move(mnemonic32)),
					 FormatterString(std::move(mnemonic64))},
		  code_size_(code_size) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		const FormatterString* mnemonic;
		if (instruction.code_size() == code_size_ && !options.gas_show_mnemonic_size_suffix())
			mnemonic = &mnemonics_[static_cast<std::size_t>(CodeSize::Unknown)];
		else
			mnemonic = &mnemonics_[static_cast<std::size_t>(code_size_)];
		return InstrOpInfo(*mnemonic, instruction, InstrOpInfoFlags::NONE);
	}

private:
	std::array<FormatterString, 4> mnemonics_;
	CodeSize code_size_;
};

class SimpleInstrInfo_OpSize2_bnd final : public InstrInfo {
public:
	SimpleInstrInfo_OpSize2_bnd(std::string mnemonic, std::string mnemonic16, std::string mnemonic32, std::string mnemonic64)
		: mnemonics_{FormatterString(std::move(mnemonic)), FormatterString(std::move(mnemonic16)), FormatterString(std::move(mnemonic32)),
					 FormatterString(std::move(mnemonic64))} {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		if (instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		const FormatterString* mnemonic;
		if (options.gas_show_mnemonic_size_suffix())
			mnemonic = &mnemonics_[static_cast<std::size_t>(CodeSize::Code64)];
		else
			mnemonic = &mnemonics_[static_cast<std::size_t>(instruction.code_size())];
		return InstrOpInfo(*mnemonic, instruction, flags);
	}

private:
	std::array<FormatterString, 4> mnemonics_;
};

class SimpleInstrInfo_OpSize3 final : public InstrInfo {
public:
	SimpleInstrInfo_OpSize3(std::uint32_t bitness, std::string mnemonic, std::string mnemonic_suffix)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		const FormatterString& mnemonic =
			!options.gas_show_mnemonic_size_suffix() && (instr_bitness == 0 || (instr_bitness & bitness_) != 0) ? mnemonic_ : mnemonic_suffix_;
		return InstrOpInfo(mnemonic, instruction, InstrOpInfoFlags::NONE);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_os2 final : public InstrInfo {
public:
	SimpleInstrInfo_os2(std::uint32_t bitness, std::string mnemonic, std::string mnemonic_suffix, bool can_use_bnd, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)), bitness_(bitness), flags_(flags), can_use_bnd_(can_use_bnd) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = flags_;
		if (can_use_bnd_ && instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		const FormatterString& mnemonic = instr_bitness != 0 && instr_bitness != bitness_
											  ? mnemonic_suffix_
											  : get_mnemonic(options, instruction, mnemonic_, mnemonic_suffix_, flags);
		return InstrOpInfo(mnemonic, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
	std::uint32_t bitness_;
	std::uint32_t flags_;
	bool can_use_bnd_;
};

class SimpleInstrInfo_os final : public InstrInfo {
public:
	SimpleInstrInfo_os(std::uint32_t bitness, std::string mnemonic, bool can_use_bnd, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), bitness_(bitness), flags_(flags), can_use_bnd_(can_use_bnd) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = flags_;
		if (can_use_bnd_ && instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::OP_SIZE16;
			else if (bitness_ == 32)
				flags |= InstrOpInfoFlags::OP_SIZE32;
			else
				flags |= InstrOpInfoFlags::OP_SIZE64;
		}
		return InstrOpInfo(get_mnemonic(options, instruction, mnemonic_, mnemonic_, flags), instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
	std::uint32_t flags_;
	bool can_use_bnd_;
};

class SimpleInstrInfo_os_mem final : public InstrInfo {
public:
	SimpleInstrInfo_os_mem(std::uint32_t bitness, std::string mnemonic, std::string mnemonic_suffix)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		const bool has_mem_op = instruction.op0_kind() == OpKind::Memory || instruction.op1_kind() == OpKind::Memory;
		if (has_mem_op && !(instr_bitness == 0 || (instr_bitness != 64 && instr_bitness == bitness_) || (instr_bitness == 64 && bitness_ == 32))) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::OP_SIZE16;
			else if (bitness_ == 32)
				flags |= InstrOpInfoFlags::OP_SIZE32;
			else
				flags |= InstrOpInfoFlags::OP_SIZE64;
		}
		const FormatterString& mnemonic = has_mem_op ? mnemonic_ : get_mnemonic(options, instruction, mnemonic_, mnemonic_suffix_, flags);
		return InstrOpInfo(mnemonic, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_os_mem2 final : public InstrInfo {
public:
	SimpleInstrInfo_os_mem2(std::uint32_t bitness, std::string mnemonic, std::string mnemonic_suffix)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		const FormatterString& mnemonic = instr_bitness != 0 && (instr_bitness & bitness_) == 0
											  ? mnemonic_suffix_
											  : get_mnemonic(options, instruction, mnemonic_, mnemonic_suffix_, FLAGS);
		return InstrOpInfo(mnemonic, instruction, InstrOpInfoFlags::NONE);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_Reg16 final : public InstrInfo {
public:
	SimpleInstrInfo_Reg16(std::string mnemonic, std::string mnemonic_suffix)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		InstrOpInfo info(get_mnemonic(options, instruction, mnemonic_, mnemonic_suffix_, FLAGS), instruction, FLAGS);
		info.op_registers[0] = r_to_r16(info.op_registers[0]);
		info.op_registers[1] = r_to_r16(info.op_registers[1]);
		info.op_registers[2] = r_to_r16(info.op_registers[2]);
		return info;
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
};

class SimpleInstrInfo_mem16 final : public InstrInfo {
public:
	SimpleInstrInfo_mem16(std::string mnemonic, std::string mnemonic_reg_suffix, std::string mnemonic_mem_suffix)
		: mnemonic_(std::move(mnemonic)), mnemonic_reg_suffix_(std::move(mnemonic_reg_suffix)), mnemonic_mem_suffix_(std::move(mnemonic_mem_suffix)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		const FormatterString& mnemonic_suffix =
			instruction.op0_kind() == OpKind::Memory || instruction.op1_kind() == OpKind::Memory ? mnemonic_mem_suffix_ : mnemonic_reg_suffix_;
		return InstrOpInfo(get_mnemonic(options, instruction, mnemonic_, mnemonic_suffix, FLAGS), instruction, FLAGS);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_reg_suffix_;
	FormatterString mnemonic_mem_suffix_;
};

constexpr std::uint32_t NO_CC_INDEX = 0xFFFF'FFFF;

class SimpleInstrInfo_os_loop final : public InstrInfo {
public:
	SimpleInstrInfo_os_loop(std::uint32_t bitness, std::uint32_t reg_size, std::uint32_t cc_index, std::vector<std::string> mnemonics,
							std::vector<std::string> mnemonics_suffix)
		: mnemonics_(to_formatter_strings(std::move(mnemonics))), mnemonics_suffix_(to_formatter_strings(std::move(mnemonics_suffix))),
		  bitness_(bitness), cc_index_(cc_index), reg_size_(reg_size) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		const std::vector<FormatterString>& mnemonics =
			(instr_bitness != 0 && instr_bitness != reg_size_) || options.gas_show_mnemonic_size_suffix() ? mnemonics_suffix_ : mnemonics_;
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::OP_SIZE16 | InstrOpInfoFlags::OP_SIZE_IS_BYTE_DIRECTIVE;
			else if (bitness_ == 32)
				flags |= InstrOpInfoFlags::OP_SIZE32 | InstrOpInfoFlags::OP_SIZE_IS_BYTE_DIRECTIVE;
			else
				flags |= InstrOpInfoFlags::OP_SIZE64;
		}
		const FormatterString& mnemonic = cc_index_ == NO_CC_INDEX ? mnemonics[0] : get_mnemonic_cc(options, cc_index_, mnemonics);
		return InstrOpInfo(mnemonic, instruction, flags);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::vector<FormatterString> mnemonics_suffix_;
	std::uint32_t bitness_;
	std::uint32_t cc_index_;
	std::uint32_t reg_size_;
};

class SimpleInstrInfo_os_jcc final : public InstrInfo {
public:
	SimpleInstrInfo_os_jcc(std::uint32_t bitness, std::uint32_t cc_index, std::vector<std::string> mnemonics)
		: mnemonics_(to_formatter_strings(std::move(mnemonics))), bitness_(bitness), cc_index_(cc_index) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::OP_SIZE16;
			else if (bitness_ == 32)
				flags |= InstrOpInfoFlags::OP_SIZE32;
			else
				flags |= InstrOpInfoFlags::OP_SIZE64;
		}
		const Register prefix_seg = instruction.segment_prefix();
		if (prefix_seg == Register::CS)
			flags |= InstrOpInfoFlags::JCC_NOT_TAKEN;
		else if (prefix_seg == Register::DS)
			flags |= InstrOpInfoFlags::JCC_TAKEN;
		if (instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		const FormatterString& mnemonic = get_mnemonic_cc(options, cc_index_, mnemonics_);
		return InstrOpInfo(get_mnemonic(options, instruction, mnemonic, mnemonic, flags), instruction, flags);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::uint32_t bitness_;
	std::uint32_t cc_index_;
};

class SimpleInstrInfo_movabs final : public InstrInfo {
public:
	SimpleInstrInfo_movabs(std::string mnemonic, std::string mnemonic_suffix, std::string mnemonic64, std::string mnemonic_suffix64)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)), mnemonic64_(std::move(mnemonic64)),
		  mnemonic_suffix64_(std::move(mnemonic_suffix64)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		std::uint32_t mem_size;
		const FormatterString* mnemonic;
		const FormatterString* mnemonic_suffix;
		switch (instruction.memory_displ_size()) {
		case 2:
			mem_size = 16;
			mnemonic = &mnemonic_;
			mnemonic_suffix = &mnemonic_suffix_;
			break;
		case 4:
			mem_size = 32;
			mnemonic = &mnemonic_;
			mnemonic_suffix = &mnemonic_suffix_;
			break;
		default:
			mem_size = 64;
			mnemonic = &mnemonic64_;
			mnemonic_suffix = &mnemonic_suffix64_;
			break;
		}
		if (instr_bitness == 0)
			instr_bitness = mem_size;
		if (instr_bitness == 64) {
			if (mem_size == 32)
				flags |= InstrOpInfoFlags::ADDR_SIZE32;
		} else if (instr_bitness != mem_size) {
			ICED_DEBUG_ASSERT(mem_size == 16 || mem_size == 32);
			if (mem_size == 16)
				flags |= InstrOpInfoFlags::ADDR_SIZE16;
			else
				flags |= InstrOpInfoFlags::ADDR_SIZE32;
		}
		return InstrOpInfo(get_mnemonic(options, instruction, *mnemonic, *mnemonic_suffix, flags), instruction, flags);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
	FormatterString mnemonic64_;
	FormatterString mnemonic_suffix64_;
};

void move_operands(InstrOpInfo& info, std::uint32_t index, InstrOpKind new_op_kind) noexcept {
	ICED_DEBUG_ASSERT(info.op_count <= 4);

	switch (index) {
	case 0:
		info.op_kinds[4] = info.op_kinds[3];
		info.op_registers[4] = info.op_registers[3];
		info.op_kinds[3] = info.op_kinds[2];
		info.op_registers[3] = info.op_registers[2];
		info.op_kinds[2] = info.op_kinds[1];
		info.op_registers[2] = info.op_registers[1];
		info.op_kinds[1] = info.op_kinds[0];
		info.op_registers[1] = info.op_registers[0];
		info.op_kinds[0] = new_op_kind;
		info.op_indexes[4] = info.op_indexes[3];
		info.op_indexes[3] = info.op_indexes[2];
		info.op_indexes[2] = info.op_indexes[1];
		info.op_indexes[1] = info.op_indexes[0];
		info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_NONE;
		info.op_count++;
		break;

	case 1:
		info.op_kinds[4] = info.op_kinds[3];
		info.op_registers[4] = info.op_registers[3];
		info.op_kinds[3] = info.op_kinds[2];
		info.op_registers[3] = info.op_registers[2];
		info.op_kinds[2] = info.op_kinds[1];
		info.op_registers[2] = info.op_registers[1];
		info.op_kinds[1] = new_op_kind;
		info.op_indexes[4] = info.op_indexes[3];
		info.op_indexes[3] = info.op_indexes[2];
		info.op_indexes[2] = info.op_indexes[1];
		info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_NONE;
		info.op_count++;
		break;

	default:
		ICED_UNREACHABLE();
	}
}

class SimpleInstrInfo_er final : public InstrInfo {
public:
	SimpleInstrInfo_er(std::uint32_t er_index, std::string mnemonic)
		: mnemonic_(mnemonic), mnemonic_suffix_(std::move(mnemonic)), er_index_(er_index), flags_(InstrOpInfoFlags::NONE) {}
	SimpleInstrInfo_er(std::uint32_t er_index, std::string mnemonic, std::string mnemonic_suffix, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)), er_index_(er_index), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		InstrOpInfo info(get_mnemonic(options, instruction, mnemonic_, mnemonic_suffix_, flags_), instruction, flags_);
		if (IcedConstants::is_mvex(instruction.code())) {
			const RoundingControl rc = instruction.rounding_control();
			if (rc != RoundingControl::None) {
				InstrOpKind rc_op_kind;
				if (instruction.suppress_all_exceptions()) {
					switch (rc) {
					case RoundingControl::RoundToNearest:
						rc_op_kind = InstrOpKind::RnSae;
						break;
					case RoundingControl::RoundDown:
						rc_op_kind = InstrOpKind::RdSae;
						break;
					case RoundingControl::RoundUp:
						rc_op_kind = InstrOpKind::RuSae;
						break;
					case RoundingControl::RoundTowardZero:
						rc_op_kind = InstrOpKind::RzSae;
						break;
					default:
						return info;
					}
				} else {
					switch (rc) {
					case RoundingControl::RoundToNearest:
						rc_op_kind = InstrOpKind::Rn;
						break;
					case RoundingControl::RoundDown:
						rc_op_kind = InstrOpKind::Rd;
						break;
					case RoundingControl::RoundUp:
						rc_op_kind = InstrOpKind::Ru;
						break;
					case RoundingControl::RoundTowardZero:
						rc_op_kind = InstrOpKind::Rz;
						break;
					default:
						return info;
					}
				}
				move_operands(info, er_index_, rc_op_kind);
			} else if (instruction.suppress_all_exceptions())
				move_operands(info, er_index_, InstrOpKind::Sae);
		} else {
			const RoundingControl rc = instruction.rounding_control();
			if (rc != RoundingControl::None && can_show_rounding_control(instruction, options)) {
				InstrOpKind rc_op_kind;
				switch (rc) {
				case RoundingControl::RoundToNearest:
					rc_op_kind = InstrOpKind::RnSae;
					break;
				case RoundingControl::RoundDown:
					rc_op_kind = InstrOpKind::RdSae;
					break;
				case RoundingControl::RoundUp:
					rc_op_kind = InstrOpKind::RuSae;
					break;
				case RoundingControl::RoundTowardZero:
					rc_op_kind = InstrOpKind::RzSae;
					break;
				default:
					return info;
				}
				move_operands(info, er_index_, rc_op_kind);
			}
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
	std::uint32_t er_index_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_sae final : public InstrInfo {
public:
	SimpleInstrInfo_sae(std::uint32_t sae_index, std::string mnemonic) : mnemonic_(std::move(mnemonic)), sae_index_(sae_index) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		if (instruction.suppress_all_exceptions())
			move_operands(info, sae_index_, InstrOpKind::Sae);
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t sae_index_;
};

class SimpleInstrInfo_far final : public InstrInfo {
public:
	SimpleInstrInfo_far(std::uint32_t bitness, std::string mnemonic, std::string mnemonic_suffix)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = InstrOpInfoFlags::INDIRECT_OPERAND;
		std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (instr_bitness == 0)
			instr_bitness = bitness_;
		const FormatterString* mnemonic;
		if (bitness_ == 64) {
			flags |= InstrOpInfoFlags::OP_SIZE64;
			ICED_DEBUG_ASSERT(mnemonic_.lower() == mnemonic_suffix_.lower());
			mnemonic = &mnemonic_;
		} else {
			if (bitness_ != instr_bitness || options.gas_show_mnemonic_size_suffix())
				mnemonic = &mnemonic_suffix_;
			else
				mnemonic = &mnemonic_;
		}
		return InstrOpInfo(*mnemonic, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_bnd final : public InstrInfo {
public:
	SimpleInstrInfo_bnd(std::string mnemonic, std::string mnemonic_suffix, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = flags_;
		if (instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		return InstrOpInfo(get_mnemonic(options, instruction, mnemonic_, mnemonic_suffix_, flags), instruction, flags);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
	std::uint32_t flags_;
};

void remove_first_imm8_operand(InstrOpInfo& info) noexcept {
	ICED_DEBUG_ASSERT(info.op_kinds[0] == InstrOpKind::Immediate8);
	info.op_count--;
	switch (info.op_count) {
	case 0:
		info.op_indexes[0] = OP_ACCESS_INVALID;
		break;

	case 1:
		info.op_kinds[0] = info.op_kinds[1];
		info.op_registers[0] = info.op_registers[1];
		info.op_indexes[0] = info.op_indexes[1];
		info.op_indexes[1] = OP_ACCESS_INVALID;
		break;

	case 2:
		info.op_kinds[0] = info.op_kinds[1];
		info.op_registers[0] = info.op_registers[1];
		info.op_kinds[1] = info.op_kinds[2];
		info.op_registers[1] = info.op_registers[2];
		info.op_indexes[0] = info.op_indexes[1];
		info.op_indexes[1] = info.op_indexes[2];
		info.op_indexes[2] = OP_ACCESS_INVALID;
		break;

	case 3:
		info.op_kinds[0] = info.op_kinds[1];
		info.op_registers[0] = info.op_registers[1];
		info.op_kinds[1] = info.op_kinds[2];
		info.op_registers[1] = info.op_registers[2];
		info.op_kinds[2] = info.op_kinds[3];
		info.op_registers[2] = info.op_registers[3];
		info.op_indexes[0] = info.op_indexes[1];
		info.op_indexes[1] = info.op_indexes[2];
		info.op_indexes[2] = info.op_indexes[3];
		info.op_indexes[3] = OP_ACCESS_INVALID;
		break;

	case 4:
		info.op_kinds[0] = info.op_kinds[1];
		info.op_registers[0] = info.op_registers[1];
		info.op_kinds[1] = info.op_kinds[2];
		info.op_registers[1] = info.op_registers[2];
		info.op_kinds[2] = info.op_kinds[3];
		info.op_registers[2] = info.op_registers[3];
		info.op_kinds[3] = info.op_kinds[4];
		info.op_registers[3] = info.op_registers[4];
		info.op_indexes[0] = info.op_indexes[1];
		info.op_indexes[1] = info.op_indexes[2];
		info.op_indexes[2] = info.op_indexes[3];
		info.op_indexes[3] = info.op_indexes[4];
		info.op_indexes[4] = OP_ACCESS_INVALID;
		break;

	default:
		ICED_UNREACHABLE();
	}
}

class SimpleInstrInfo_pops final : public InstrInfo {
public:
	SimpleInstrInfo_pops(std::string mnemonic, const std::vector<FormatterString>& pseudo_ops, bool can_use_sae)
		: mnemonic_(std::move(mnemonic)), pseudo_ops_(&pseudo_ops), can_use_sae_(can_use_sae) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		if (can_use_sae_ && instruction.suppress_all_exceptions())
			move_operands(info, 1, InstrOpKind::Sae);
		const std::size_t imm = instruction.immediate8();
		if (options.use_pseudo_ops() && imm < pseudo_ops_->size()) {
			remove_first_imm8_operand(info);
			info.mnemonic = &(*pseudo_ops_)[imm];
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	const std::vector<FormatterString>* pseudo_ops_;
	bool can_use_sae_;
};

class SimpleInstrInfo_pclmulqdq final : public InstrInfo {
public:
	SimpleInstrInfo_pclmulqdq(std::string mnemonic, const std::vector<FormatterString>& pseudo_ops)
		: mnemonic_(std::move(mnemonic)), pseudo_ops_(&pseudo_ops) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		if (options.use_pseudo_ops()) {
			std::int32_t index;
			switch (instruction.immediate8()) {
			case 0:
				index = 0;
				break;
			case 1:
				index = 1;
				break;
			case 0x10:
				index = 2;
				break;
			case 0x11:
				index = 3;
				break;
			default:
				index = -1;
				break;
			}
			if (index >= 0) {
				remove_first_imm8_operand(info);
				ICED_ASSERT(static_cast<std::size_t>(index) < pseudo_ops_->size());
				info.mnemonic = &(*pseudo_ops_)[static_cast<std::size_t>(index)];
			}
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	const std::vector<FormatterString>* pseudo_ops_;
};

class SimpleInstrInfo_imul final : public InstrInfo {
public:
	SimpleInstrInfo_imul(std::string mnemonic, std::string mnemonic_suffix) : mnemonic_(std::move(mnemonic)), mnemonic_suffix_(std::move(mnemonic_suffix)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		InstrOpInfo info(get_mnemonic(options, instruction, mnemonic_, mnemonic_suffix_, FLAGS), instruction, FLAGS);
		ICED_DEBUG_ASSERT(info.op_count == 3);
		if (options.use_pseudo_ops() && info.op_kinds[1] == InstrOpKind::Register && info.op_kinds[2] == InstrOpKind::Register &&
			info.op_registers[1] == info.op_registers[2]) {
			info.op_count--;
			info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_READ_WRITE;
			info.op_indexes[2] = OP_ACCESS_INVALID;
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic_suffix_;
};

class SimpleInstrInfo_Reg32 final : public InstrInfo {
public:
	explicit SimpleInstrInfo_Reg32(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		InstrOpInfo info(mnemonic_, instruction, FLAGS);
		info.op_registers[0] = r64_to_r32(info.op_registers[0]);
		info.op_registers[1] = r64_to_r32(info.op_registers[1]);
		info.op_registers[2] = r64_to_r32(info.op_registers[2]);
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_DeclareData final : public InstrInfo {
public:
	SimpleInstrInfo_DeclareData(Code code, std::string mnemonic) : mnemonic_(std::move(mnemonic)), op_kind_(get_op_kind(code)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::KEEP_OPERAND_ORDER | InstrOpInfoFlags::MNEMONIC_IS_DIRECTIVE);
		info.op_count = static_cast<std::uint8_t>(instruction.declare_data_len());
		info.op_kinds[0] = op_kind_;
		info.op_kinds[1] = op_kind_;
		info.op_kinds[2] = op_kind_;
		info.op_kinds[3] = op_kind_;
		info.op_kinds[4] = op_kind_;
		info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[2] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[3] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[4] = InstrInfoConstants::OP_ACCESS_READ;
		return info;
	}

private:
	static InstrOpKind get_op_kind(Code code) noexcept {
		switch (code) {
		case Code::DeclareByte:
			return InstrOpKind::DeclareByte;
		case Code::DeclareWord:
			return InstrOpKind::DeclareWord;
		case Code::DeclareDword:
			return InstrOpKind::DeclareDword;
		case Code::DeclareQword:
			return InstrOpKind::DeclareQword;
		default:
			ICED_UNREACHABLE();
		}
	}

	FormatterString mnemonic_;
	InstrOpKind op_kind_;
};

// Not inlined so the (big) table reader function's stack frame stays small
template <typename T, typename... Args>
ICED_NOINLINE std::unique_ptr<InstrInfo> make_info(Args&&... args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}

// dst = s + c (c is ignored if it's 0)
void add_suffix(std::string& dst, const std::string& s, char c) {
	dst.assign(s);
	if (c != '\0')
		dst.push_back(c);
}

struct InstrInfosHolder {
	InstrInfos infos;

	InstrInfosHolder() {
		DataReader reader(FORMATTER_TBL_DATA, FORMATTER_TBL_DATA_SIZE);
		const std::vector<std::string_view> strings = get_strings_table_ref();
		const auto read_string = [&reader, &strings]() {
			const std::size_t index = reader.read_compressed_u32();
			ICED_ASSERT(index < strings.size());
			return strings[index];
		};
		const auto read_char = [&reader]() { return static_cast<char>(static_cast<std::uint8_t>(reader.read_u8())); };
		// Reused by all iterations (keeps the stack frame small)
		std::string s, s2, s3, s4, s5, s6;
		std::vector<std::string> mnemonics;
		std::vector<std::string> mnemonics_suffix;
		std::size_t prev_index = 0;
		bool has_prev_index = false;
		for (std::size_t i = 0; i < infos.size(); i++) {
			const std::size_t f = reader.read_u8();
			auto ctor_kind = static_cast<CtorKind>(f & 0x7F);
			std::size_t current_index = 0;
			bool restore_index = false;
			if (ctor_kind == CtorKind::Previous) {
				ICED_ASSERT(has_prev_index);
				current_index = reader.index();
				restore_index = true;
				reader.set_index(prev_index);
				ctor_kind = static_cast<CtorKind>(reader.read_u8() & 0x7F);
			} else {
				prev_index = reader.index() - 1;
				has_prev_index = true;
			}
			if ((f & 0x80) != 0) {
				s.assign(1, 'v');
				s.append(read_string());
			} else
				s.assign(read_string());

			char c;
			std::uint32_t v;
			std::uint32_t v2;
			std::uint32_t v3;
			std::unique_ptr<InstrInfo> info;
			switch (ctor_kind) {
			case CtorKind::Normal_1:
				info = make_info<SimpleInstrInfo>(std::string(s), std::move(s), InstrOpInfoFlags::NONE);
				break;

			case CtorKind::Normal_2a: {
				c = read_char();
				add_suffix(s2, s, c);
				info = make_info<SimpleInstrInfo>(std::move(s), std::move(s2), InstrOpInfoFlags::NONE);
				break;
			}

			case CtorKind::Normal_2b:
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo>(std::string(s), std::move(s), v);
				break;

			case CtorKind::Normal_2c:
				c = read_char();
				if (c != '\0')
					s.push_back(c);
				info = make_info<SimpleInstrInfo>(std::string(s), std::move(s), InstrOpInfoFlags::NONE);
				break;

			case CtorKind::Normal_3: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo>(std::move(s), std::move(s2), v);
				break;
			}

			case CtorKind::AamAad:
				info = make_info<SimpleInstrInfo_AamAad>(std::move(s));
				break;

			case CtorKind::asz:
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_as>(v, std::move(s));
				break;

			case CtorKind::bnd: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_bnd>(std::move(s), std::move(s2), v);
				break;
			}

			case CtorKind::DeclareData:
				info = make_info<SimpleInstrInfo_DeclareData>(static_cast<Code>(i), std::move(s));
				break;

			case CtorKind::er_2:
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_er>(v, std::move(s));
				break;

			case CtorKind::er_4: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				v2 = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_er>(v, std::move(s), std::move(s2), v2);
				break;
			}

			case CtorKind::far: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_far>(v, std::move(s), std::move(s2));
				break;
			}

			case CtorKind::imul: {
				c = read_char();
				add_suffix(s2, s, c);
				info = make_info<SimpleInstrInfo_imul>(std::move(s), std::move(s2));
				break;
			}

			case CtorKind::maskmovq:
				info = make_info<SimpleInstrInfo_maskmovq>(std::move(s));
				break;

			case CtorKind::movabs: {
				c = read_char();
				add_suffix(s2, s, c);
				s3.assign(read_string());
				add_suffix(s4, s3, c);
				info = make_info<SimpleInstrInfo_movabs>(std::move(s), std::move(s2), std::move(s3), std::move(s4));
				break;
			}

			case CtorKind::nop:
				v = reader.read_compressed_u32();
				v2 = static_cast<std::uint32_t>(reader.read_u8());
				info = make_info<SimpleInstrInfo_nop>(v, std::move(s), static_cast<Register>(v2));
				break;

			case CtorKind::OpSize: {
				v = static_cast<std::uint32_t>(reader.read_u8());
				add_suffix(s2, s, 'w');
				add_suffix(s3, s, 'l');
				add_suffix(s4, s, 'q');
				info = make_info<SimpleInstrInfo_OpSize>(static_cast<CodeSize>(v), std::move(s), std::move(s2), std::move(s3), std::move(s4));
				break;
			}

			case CtorKind::OpSize2_bnd: {
				s2.assign(read_string());
				s3.assign(read_string());
				s4.assign(read_string());
				info = make_info<SimpleInstrInfo_OpSize2_bnd>(std::move(s), std::move(s2), std::move(s3), std::move(s4));
				break;
			}

			case CtorKind::OpSize3: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_OpSize3>(v, std::move(s), std::move(s2));
				break;
			}

			case CtorKind::os:
				v = reader.read_compressed_u32();
				v2 = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v2 <= 1);
				v3 = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_os>(v, std::move(s), v2 != 0, v3);
				break;

			case CtorKind::CC_1: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				mnemonics.clear();
				mnemonics.push_back(std::move(s));
				mnemonics_suffix.clear();
				mnemonics_suffix.push_back(std::move(s2));
				info = make_info<SimpleInstrInfo_cc>(v, std::move(mnemonics), std::move(mnemonics_suffix));
				break;
			}

			case CtorKind::CC_2: {
				s2.assign(read_string());
				c = read_char();
				add_suffix(s3, s, c);
				add_suffix(s4, s2, c);
				v = reader.read_compressed_u32();
				mnemonics.clear();
				mnemonics.push_back(std::move(s));
				mnemonics.push_back(std::move(s2));
				mnemonics_suffix.clear();
				mnemonics_suffix.push_back(std::move(s3));
				mnemonics_suffix.push_back(std::move(s4));
				info = make_info<SimpleInstrInfo_cc>(v, std::move(mnemonics), std::move(mnemonics_suffix));
				break;
			}

			case CtorKind::CC_3: {
				s2.assign(read_string());
				s3.assign(read_string());
				c = read_char();
				add_suffix(s4, s, c);
				add_suffix(s5, s2, c);
				add_suffix(s6, s3, c);
				v = reader.read_compressed_u32();
				mnemonics.clear();
				mnemonics.push_back(std::move(s));
				mnemonics.push_back(std::move(s2));
				mnemonics.push_back(std::move(s3));
				mnemonics_suffix.clear();
				mnemonics_suffix.push_back(std::move(s4));
				mnemonics_suffix.push_back(std::move(s5));
				mnemonics_suffix.push_back(std::move(s6));
				info = make_info<SimpleInstrInfo_cc>(v, std::move(mnemonics), std::move(mnemonics_suffix));
				break;
			}

			case CtorKind::os_jcc_1: {
				v2 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				mnemonics.clear();
				mnemonics.push_back(std::move(s));
				info = make_info<SimpleInstrInfo_os_jcc>(v, v2, std::move(mnemonics));
				break;
			}

			case CtorKind::os_jcc_2: {
				s2.assign(read_string());
				v2 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				mnemonics.clear();
				mnemonics.push_back(std::move(s));
				mnemonics.push_back(std::move(s2));
				info = make_info<SimpleInstrInfo_os_jcc>(v, v2, std::move(mnemonics));
				break;
			}

			case CtorKind::os_jcc_3: {
				s2.assign(read_string());
				s3.assign(read_string());
				v2 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				mnemonics.clear();
				mnemonics.push_back(std::move(s));
				mnemonics.push_back(std::move(s2));
				mnemonics.push_back(std::move(s3));
				info = make_info<SimpleInstrInfo_os_jcc>(v, v2, std::move(mnemonics));
				break;
			}

			case CtorKind::os_loopcc: {
				s2.assign(read_string());
				c = read_char();
				add_suffix(s3, s, c);
				add_suffix(s4, s2, c);
				v3 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				v2 = reader.read_compressed_u32();
				mnemonics.clear();
				mnemonics.push_back(std::move(s));
				mnemonics.push_back(std::move(s2));
				mnemonics_suffix.clear();
				mnemonics_suffix.push_back(std::move(s3));
				mnemonics_suffix.push_back(std::move(s4));
				info = make_info<SimpleInstrInfo_os_loop>(v, v2, v3, std::move(mnemonics), std::move(mnemonics_suffix));
				break;
			}

			case CtorKind::os_loop: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				v2 = reader.read_compressed_u32();
				mnemonics.clear();
				mnemonics.push_back(std::move(s));
				mnemonics_suffix.clear();
				mnemonics_suffix.push_back(std::move(s2));
				info = make_info<SimpleInstrInfo_os_loop>(v, v2, NO_CC_INDEX, std::move(mnemonics), std::move(mnemonics_suffix));
				break;
			}

			case CtorKind::os_mem: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_os_mem>(v, std::move(s), std::move(s2));
				break;
			}

			case CtorKind::Reg16: {
				add_suffix(s2, s, 'w');
				info = make_info<SimpleInstrInfo_Reg16>(std::move(s), std::move(s2));
				break;
			}

			case CtorKind::os_mem2: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_os_mem2>(v, std::move(s), std::move(s2));
				break;
			}

			case CtorKind::os2_3: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				v2 = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v2 <= 1);
				info = make_info<SimpleInstrInfo_os2>(v, std::move(s), std::move(s2), v2 != 0, 0);
				break;
			}

			case CtorKind::os2_4: {
				c = read_char();
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				v2 = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v2 <= 1);
				v3 = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_os2>(v, std::move(s), std::move(s2), v2 != 0, v3);
				break;
			}

			case CtorKind::pblendvb:
				info = make_info<SimpleInstrInfo_pblendvb>(std::move(s));
				break;

			case CtorKind::pclmulqdq:
				v = static_cast<std::uint32_t>(reader.read_u8());
				info = make_info<SimpleInstrInfo_pclmulqdq>(std::move(s), get_pseudo_ops(static_cast<PseudoOpsKind>(v)));
				break;

			case CtorKind::pops:
				v = static_cast<std::uint32_t>(reader.read_u8());
				v2 = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v2 <= 1);
				info = make_info<SimpleInstrInfo_pops>(std::move(s), get_pseudo_ops(static_cast<PseudoOpsKind>(v)), v2 != 0);
				break;

			case CtorKind::mem16: {
				c = read_char();
				add_suffix(s2, s, c);
				add_suffix(s3, s, 'w');
				info = make_info<SimpleInstrInfo_mem16>(std::move(s), std::move(s2), std::move(s3));
				break;
			}

			case CtorKind::Reg32:
				info = make_info<SimpleInstrInfo_Reg32>(std::move(s));
				break;

			case CtorKind::sae:
				v = reader.read_compressed_u32();
				info = make_info<SimpleInstrInfo_sae>(v, std::move(s));
				break;

			case CtorKind::ST_STi:
				info = make_info<SimpleInstrInfo_ST_STi>(std::move(s));
				break;

			case CtorKind::STi_ST:
				v = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v <= 1);
				info = make_info<SimpleInstrInfo_STi_ST>(std::move(s), v != 0);
				break;

			case CtorKind::STIG1:
				v = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v <= 1);
				info = make_info<SimpleInstrInfo_STIG1>(std::move(s), v != 0);
				break;

			case CtorKind::Previous:
			default:
				ICED_UNREACHABLE();
			}

			infos[i] = std::move(info);
			if (restore_index)
				reader.set_index(current_index);
		}
		ICED_DEBUG_ASSERT(!reader.can_read());
	}
};

} // namespace

const InstrInfos& get_all_infos() {
	static const InstrInfosHolder holder;
	return holder.infos;
}

} // namespace iced_x86::internal::gas
