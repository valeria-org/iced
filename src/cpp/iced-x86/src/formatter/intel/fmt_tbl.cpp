// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/intel/info.rs + formatter/intel/fmt_tbl.rs

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
#include "internal/data_reader.hpp"
#include "internal/formatter/fmt_common.hpp"
#include "internal/formatter/fmt_utils.hpp"
#include "internal/formatter/intel/ctor_kind.hpp"
#include "internal/formatter/intel/fmt_data.hpp"
#include "internal/formatter/intel/info.hpp"
#include "internal/formatter/intel/instr_op_info_flags.hpp"
#include "internal/formatter/intel/mem_size_tbl.hpp"
#include "internal/formatter/pseudo_ops.hpp"
#include "internal/formatter/strings_tbl.hpp"
#include "internal/iced_assert.hpp"

namespace iced_x86::internal::intel {

static_assert(IcedConstants::MAX_OP_COUNT == 5, "");

InstrOpInfo::InstrOpInfo(const FormatterString& mnemonic_, const Instruction& instruction, std::uint32_t flags_) noexcept
	: InstrOpInfo(mnemonic_) {
	flags = static_cast<std::uint16_t>(flags_);
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
	const std::uint32_t instr_op_count = instruction.op_count();
	op_count = static_cast<std::uint8_t>(instr_op_count);
	switch (instr_op_count) {
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
		op_indexes[1] = 1;
		op_indexes[2] = OP_ACCESS_INVALID;
		op_indexes[3] = OP_ACCESS_INVALID;
		op_indexes[4] = OP_ACCESS_INVALID;
		break;

	case 3:
		op_indexes[1] = 1;
		op_indexes[2] = 2;
		op_indexes[3] = OP_ACCESS_INVALID;
		op_indexes[4] = OP_ACCESS_INVALID;
		break;

	case 4:
		op_indexes[1] = 1;
		op_indexes[2] = 2;
		op_indexes[3] = 3;
		op_indexes[4] = OP_ACCESS_INVALID;
		break;

	case 5:
		op_indexes[1] = 1;
		op_indexes[2] = 2;
		op_indexes[3] = 3;
		op_indexes[4] = 4;
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

class SimpleInstrInfo final : public InstrInfo {
public:
	SimpleInstrInfo(std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		return InstrOpInfo(mnemonic_, instruction, flags_);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_cc final : public InstrInfo {
public:
	SimpleInstrInfo_cc(std::uint32_t cc_index, std::vector<std::string> mnemonics)
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics))), cc_index_(cc_index) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		const FormatterString& mnemonic = get_mnemonic_cc(options, cc_index_, mnemonics_);
		return InstrOpInfo(mnemonic, instruction, FLAGS);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::uint32_t cc_index_;
};

class SimpleInstrInfo_memsize final : public InstrInfo {
public:
	SimpleInstrInfo_memsize(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		const std::uint32_t flags = instr_bitness == 0 || (instr_bitness & bitness_) != 0
										? InstrOpInfoFlags::MEM_SIZE_NOTHING
										: InstrOpInfoFlags::SHOW_NO_MEM_SIZE_FORCE_SIZE | InstrOpInfoFlags::SHOW_MIN_MEM_SIZE_FORCE_SIZE;
		return InstrOpInfo(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_StringIg1 final : public InstrInfo {
public:
	explicit SimpleInstrInfo_StringIg1(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_);
		info.op_count = 1;
		info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op0_kind());
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_StringIg0 final : public InstrInfo {
public:
	explicit SimpleInstrInfo_StringIg0(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_);
		info.op_count = 1;
		info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
		info.op_indexes[0] = info.op_indexes[1];
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_nop final : public InstrInfo {
public:
	SimpleInstrInfo_nop(std::uint32_t bitness, std::string mnemonic, Register register_)
		: mnemonic_(std::move(mnemonic)), bitness_(bitness), register_(register_), str_xchg_("xchg") {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (instr_bitness == 0 || (instr_bitness & bitness_) != 0)
			return InstrOpInfo(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		InstrOpInfo info(str_xchg_);
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
};

class SimpleInstrInfo_ST1 final : public InstrInfo {
public:
	SimpleInstrInfo_ST1(std::string mnemonic, std::uint32_t flags, bool is_load)
		: mnemonic_(std::move(mnemonic)), flags_(flags),
		  op0_access_(is_load ? InstrInfoConstants::OP_ACCESS_WRITE : InstrInfoConstants::OP_ACCESS_READ_WRITE) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_, instruction, flags_);
		ICED_DEBUG_ASSERT(instruction.op_count() == 1);
		info.op_count = 2;
		info.op_kinds[1] = info.op_kinds[0];
		info.op_registers[1] = info.op_registers[0];
		info.op_kinds[0] = InstrOpKind::Register;
		info.op_registers[0] = REGISTER_ST;
		info.op_indexes[1] = info.op_indexes[0];
		info.op_indexes[0] = op0_access_;
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
	std::int8_t op0_access_;
};

class SimpleInstrInfo_ST2 final : public InstrInfo {
public:
	SimpleInstrInfo_ST2(std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_, instruction, flags_);
		ICED_DEBUG_ASSERT(instruction.op_count() == 1);
		info.op_count = 2;
		info.op_kinds[1] = InstrOpKind::Register;
		info.op_registers[1] = REGISTER_ST;
		info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_READ;
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_maskmovq final : public InstrInfo {
public:
	explicit SimpleInstrInfo_maskmovq(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		ICED_DEBUG_ASSERT(instruction.op_count() == 3);

		const OpKind op_kind = instruction.op0_kind();
		OpKind short_form_op_kind;
		switch (instruction.code_size()) {
		case CodeSize::Code16:
			short_form_op_kind = OpKind::MemorySegDI;
			break;
		case CodeSize::Code32:
			short_form_op_kind = OpKind::MemorySegEDI;
			break;
		case CodeSize::Code64:
			short_form_op_kind = OpKind::MemorySegRDI;
			break;
		case CodeSize::Unknown:
		default:
			short_form_op_kind = op_kind;
			break;
		}
		std::uint32_t flags = InstrOpInfoFlags::IGNORE_SEGMENT_PREFIX;
		if (op_kind != short_form_op_kind) {
			if (op_kind == OpKind::MemorySegDI)
				flags |= InstrOpInfoFlags::ADDR_SIZE16;
			else if (op_kind == OpKind::MemorySegEDI)
				flags |= InstrOpInfoFlags::ADDR_SIZE32;
			else if (op_kind == OpKind::MemorySegRDI)
				flags |= InstrOpInfoFlags::ADDR_SIZE64;
		}
		InstrOpInfo info(mnemonic_);
		info.flags = static_cast<std::uint16_t>(flags);
		info.op_count = 2;
		info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
		info.op_indexes[0] = 1;
		info.op_registers[0] = instruction.op1_register();
		info.op_kinds[1] = InstrOpInfo::to_instr_op_kind(instruction.op2_kind());
		info.op_indexes[1] = 2;
		info.op_registers[1] = instruction.op2_register();
		const Register seg_reg = instruction.segment_prefix();
		if (seg_reg != Register::None && show_segment_prefix(Register::None, instruction, options)) {
			info.op_count = 3;
			info.op_kinds[2] = InstrOpKind::Register;
			info.op_registers[2] = seg_reg;
			info.op_indexes[2] = InstrInfoConstants::OP_ACCESS_READ;
		}
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_os final : public InstrInfo {
public:
	SimpleInstrInfo_os(std::uint32_t bitness, std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), bitness_(bitness), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = flags_;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::OP_SIZE16;
			else if (bitness_ == 32) {
				if (instr_bitness != 64)
					flags |= InstrOpInfoFlags::OP_SIZE32;
			} else
				flags |= InstrOpInfoFlags::OP_SIZE64;
		}
		return InstrOpInfo(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_os_bnd final : public InstrInfo {
public:
	SimpleInstrInfo_os_bnd(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		if (instruction.has_repne_prefix())
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
		return InstrOpInfo(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_as final : public InstrInfo {
public:
	SimpleInstrInfo_as(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;
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

class SimpleInstrInfo_os_jcc final : public InstrInfo {
public:
	SimpleInstrInfo_os_jcc(std::uint32_t bitness, std::uint32_t cc_index, std::vector<std::string> mnemonics, std::uint32_t flags)
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics))), bitness_(bitness), cc_index_(cc_index), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = flags_;
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
			flags |= InstrOpInfoFlags::IGNORE_SEGMENT_PREFIX | InstrOpInfoFlags::JCC_NOT_TAKEN;
		else if (prefix_seg == Register::DS)
			flags |= InstrOpInfoFlags::IGNORE_SEGMENT_PREFIX | InstrOpInfoFlags::JCC_TAKEN;
		if (instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		const FormatterString& mnemonic = get_mnemonic_cc(options, cc_index_, mnemonics_);
		return InstrOpInfo(mnemonic, instruction, flags);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::uint32_t bitness_;
	std::uint32_t cc_index_;
	std::uint32_t flags_;
};

constexpr std::uint32_t NO_CC_INDEX = 0xFFFF'FFFF;

class SimpleInstrInfo_os_loop final : public InstrInfo {
public:
	SimpleInstrInfo_os_loop(std::uint32_t bitness, std::uint32_t cc_index, Register register_, std::vector<std::string> mnemonics)
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics))), bitness_(bitness), cc_index_(cc_index), register_(register_) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		Register expected_reg;
		switch (instr_bitness) {
		case 0:
			expected_reg = register_;
			break;
		case 16:
			expected_reg = Register::CX;
			break;
		case 32:
			expected_reg = Register::ECX;
			break;
		case 64:
			expected_reg = Register::RCX;
			break;
		default:
			ICED_UNREACHABLE();
		}
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::OP_SIZE16;
			else if (bitness_ == 32)
				flags |= InstrOpInfoFlags::OP_SIZE32;
			else
				flags |= InstrOpInfoFlags::OP_SIZE64;
		}
		if (expected_reg != register_) {
			if (register_ == Register::CX)
				flags |= InstrOpInfoFlags::ADDR_SIZE16;
			else if (register_ == Register::ECX)
				flags |= InstrOpInfoFlags::ADDR_SIZE32;
			else
				flags |= InstrOpInfoFlags::ADDR_SIZE64;
		}
		const FormatterString& mnemonic = cc_index_ == NO_CC_INDEX ? mnemonics_[0] : get_mnemonic_cc(options, cc_index_, mnemonics_);
		return InstrOpInfo(mnemonic, instruction, flags);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::uint32_t bitness_;
	std::uint32_t cc_index_;
	Register register_;
};

class SimpleInstrInfo_movabs final : public InstrInfo {
public:
	explicit SimpleInstrInfo_movabs(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		std::uint32_t mem_size;
		switch (instruction.memory_displ_size()) {
		case 2:
			mem_size = 16;
			break;
		case 4:
			mem_size = 32;
			break;
		default:
			mem_size = 64;
			break;
		}
		if (instr_bitness == 0)
			instr_bitness = mem_size;
		if (instr_bitness != mem_size) {
			if (mem_size == 16)
				flags |= InstrOpInfoFlags::ADDR_SIZE16;
			else if (mem_size == 32)
				flags |= InstrOpInfoFlags::ADDR_SIZE32;
			else
				flags |= InstrOpInfoFlags::ADDR_SIZE64;
		}
		return InstrOpInfo(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_opmask_op final : public InstrInfo {
public:
	explicit SimpleInstrInfo_opmask_op(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		ICED_DEBUG_ASSERT(instruction.op_count() <= 2);
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		const Register kreg = instruction.op_mask();
		if (kreg != Register::None) {
			info.op_count++;
			info.op_kinds[2] = info.op_kinds[1];
			info.op_registers[2] = info.op_registers[1];
			info.op_indexes[2] = 1;
			info.op_kinds[1] = InstrOpKind::Register;
			info.op_registers[1] = kreg;
			info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_READ;
			info.flags |= static_cast<std::uint16_t>(InstrOpInfoFlags::IGNORE_OP_MASK);
		}
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_bnd final : public InstrInfo {
public:
	SimpleInstrInfo_bnd(std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = flags_;
		if (instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		return InstrOpInfo(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_ST_STi final : public InstrInfo {
public:
	SimpleInstrInfo_ST_STi(std::string mnemonic, bool pseudo_op) : mnemonic_(std::move(mnemonic)), pseudo_op_(pseudo_op) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = 0;
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

class SimpleInstrInfo_STi_ST final : public InstrInfo {
public:
	SimpleInstrInfo_STi_ST(std::string mnemonic, bool pseudo_op) : mnemonic_(std::move(mnemonic)), pseudo_op_(pseudo_op) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = 0;
		if (pseudo_op_ && options.use_pseudo_ops() && (instruction.op0_register() == Register::ST1 || instruction.op1_register() == Register::ST1))
			return InstrOpInfo(mnemonic_);
		InstrOpInfo info(mnemonic_, instruction, FLAGS);
		ICED_DEBUG_ASSERT(info.op_registers[1] == Register::ST0);
		info.op_registers[1] = REGISTER_ST;
		return info;
	}

private:
	FormatterString mnemonic_;
	bool pseudo_op_;
};

void remove_last_op(InstrOpInfo& info) noexcept {
	switch (info.op_count) {
	case 4:
		info.op_indexes[3] = OP_ACCESS_INVALID;
		break;
	case 3:
		info.op_indexes[2] = OP_ACCESS_INVALID;
		break;
	default:
		ICED_UNREACHABLE();
	}
	info.op_count--;
}

class SimpleInstrInfo_pops final : public InstrInfo {
public:
	SimpleInstrInfo_pops(std::string mnemonic, const std::vector<FormatterString>& pseudo_ops)
		: mnemonic_(std::move(mnemonic)), pseudo_ops_(&pseudo_ops) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		const std::size_t imm = instruction.immediate8();
		if (options.use_pseudo_ops() && imm < pseudo_ops_->size()) {
			info.mnemonic = &(*pseudo_ops_)[imm];
			remove_last_op(info);
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	const std::vector<FormatterString>* pseudo_ops_;
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
				ICED_ASSERT(static_cast<std::size_t>(index) < pseudo_ops_->size());
				info.mnemonic = &(*pseudo_ops_)[static_cast<std::size_t>(index)];
				remove_last_op(info);
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
	explicit SimpleInstrInfo_imul(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		ICED_DEBUG_ASSERT(info.op_count == 3);
		if (options.use_pseudo_ops() && info.op_kinds[0] == InstrOpKind::Register && info.op_kinds[1] == InstrOpKind::Register &&
			info.op_registers[0] == info.op_registers[1]) {
			info.op_count--;
			info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ_WRITE;
			info.op_kinds[1] = info.op_kinds[2];
			info.op_indexes[1] = 2;
			info.op_indexes[2] = OP_ACCESS_INVALID;
		}
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_Reg16 final : public InstrInfo {
public:
	explicit SimpleInstrInfo_Reg16(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		InstrOpInfo info(mnemonic_, instruction, FLAGS);
		info.op_registers[0] = r_to_r16(info.op_registers[0]);
		info.op_registers[1] = r_to_r16(info.op_registers[1]);
		info.op_registers[2] = r_to_r16(info.op_registers[2]);
		return info;
	}

private:
	FormatterString mnemonic_;
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

class SimpleInstrInfo_reg final : public InstrInfo {
public:
	SimpleInstrInfo_reg(std::string mnemonic, Register register_) : mnemonic_(std::move(mnemonic)), register_(register_) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		ICED_DEBUG_ASSERT(instruction.op_count() == 0);
		info.op_count = 1;
		info.op_kinds[0] = InstrOpKind::Register;
		info.op_registers[0] = register_;
		if (instruction.code() == Code::Skinit)
			info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ_WRITE;
		else
			info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ;
		return info;
	}

private:
	FormatterString mnemonic_;
	Register register_;
};

class SimpleInstrInfo_invlpga final : public InstrInfo {
public:
	SimpleInstrInfo_invlpga(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		static_cast<void>(instruction);
		InstrOpInfo info(mnemonic_);
		info.op_count = 2;
		info.op_kinds[0] = InstrOpKind::Register;
		info.op_kinds[1] = InstrOpKind::Register;
		info.op_registers[1] = Register::ECX;
		info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_READ;
		switch (bitness_) {
		case 16:
			info.op_registers[0] = Register::AX;
			break;
		case 32:
			info.op_registers[0] = Register::EAX;
			break;
		case 64:
			info.op_registers[0] = Register::RAX;
			break;
		default:
			ICED_UNREACHABLE();
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_DeclareData final : public InstrInfo {
public:
	SimpleInstrInfo_DeclareData(Code code, std::string mnemonic) : mnemonic_(std::move(mnemonic)), op_kind_(get_op_kind(code)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		InstrOpInfo info(mnemonic_, instruction, InstrOpInfoFlags::MNEMONIC_IS_DIRECTIVE);
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

class SimpleInstrInfo_bcst final : public InstrInfo {
public:
	SimpleInstrInfo_bcst(std::string mnemonic, std::uint32_t flags_no_broadcast)
		: mnemonic_(std::move(mnemonic)), flags_no_broadcast_(flags_no_broadcast) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		const FormatterString& bcst_to = *get_mem_size_tbl()[static_cast<std::size_t>(instruction.memory_size())].bcst_to;
		const std::uint32_t flags = !bcst_to.is_default() ? InstrOpInfoFlags::NONE : flags_no_broadcast_;
		return InstrOpInfo(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_no_broadcast_;
};

// Not inlined so the (big) table reader function's stack frame stays small
template <typename T, typename... Args>
ICED_NOINLINE std::unique_ptr<InstrInfo> make_info(Args&&... args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}

// Table reader state (heap allocated: it's only used once)
struct TblReader {
	DataReader reader;
	std::vector<std::string_view> strings;
	// Reused by all iterations
	std::string s;
	std::vector<std::string> mnemonics;

	TblReader() : reader(FORMATTER_TBL_DATA, FORMATTER_TBL_DATA_SIZE), strings(get_strings_table_ref()) {}

	std::string_view read_string() noexcept {
		const std::size_t index = reader.read_compressed_u32();
		ICED_ASSERT(index < strings.size());
		return strings[index];
	}
};

// Creates the InstrInfo or returns nullptr if it's created by create_info_b() (the switch is split in two functions to keep the stack frames small)
std::unique_ptr<InstrInfo> create_info_a(TblReader& r, CtorKind ctor_kind, std::size_t i) {
	DataReader& reader = r.reader;
	std::string& s = r.s;
	std::uint32_t v;
	std::uint32_t v2;
	std::unique_ptr<InstrInfo> info;
	switch (ctor_kind) {
		case CtorKind::Normal_1:
			info = make_info<SimpleInstrInfo>(std::move(s), InstrOpInfoFlags::NONE);
			break;

		case CtorKind::Normal_2:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo>(std::move(s), v);
			break;

		case CtorKind::asz:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_as>(v, std::move(s));
			break;

		case CtorKind::StringIg0:
			info = make_info<SimpleInstrInfo_StringIg0>(std::move(s));
			break;

		case CtorKind::StringIg1:
			info = make_info<SimpleInstrInfo_StringIg1>(std::move(s));
			break;

		case CtorKind::bcst:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_bcst>(std::move(s), v);
			break;

		case CtorKind::bnd:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_bnd>(std::move(s), v);
			break;

		case CtorKind::DeclareData:
			info = make_info<SimpleInstrInfo_DeclareData>(static_cast<Code>(i), std::move(s));
			break;

		case CtorKind::imul:
			info = make_info<SimpleInstrInfo_imul>(std::move(s));
			break;

		case CtorKind::opmask_op:
			info = make_info<SimpleInstrInfo_opmask_op>(std::move(s));
			break;

		case CtorKind::ST_STi:
			v = static_cast<std::uint32_t>(reader.read_u8());
			ICED_DEBUG_ASSERT(v <= 1);
			info = make_info<SimpleInstrInfo_ST_STi>(std::move(s), v != 0);
			break;

		case CtorKind::STi_ST:
			v = static_cast<std::uint32_t>(reader.read_u8());
			ICED_DEBUG_ASSERT(v <= 1);
			info = make_info<SimpleInstrInfo_STi_ST>(std::move(s), v != 0);
			break;

		case CtorKind::maskmovq:
			info = make_info<SimpleInstrInfo_maskmovq>(std::move(s));
			break;

		case CtorKind::memsize:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_memsize>(v, std::move(s));
			break;

		case CtorKind::movabs:
			info = make_info<SimpleInstrInfo_movabs>(std::move(s));
			break;

		case CtorKind::nop:
			v = reader.read_compressed_u32();
			v2 = static_cast<std::uint32_t>(reader.read_u8());
			info = make_info<SimpleInstrInfo_nop>(v, std::move(s), static_cast<Register>(v2));
			break;

		case CtorKind::os2:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_os>(v, std::move(s), InstrOpInfoFlags::NONE);
			break;

		case CtorKind::os3:
			v = reader.read_compressed_u32();
			v2 = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_os>(v, std::move(s), v2);
			break;

		case CtorKind::os_bnd:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_os_bnd>(v, std::move(s));
			break;

	default:
		break;
	}
	return info;
}

// Creates the InstrInfo (second half of the ctor kinds)
std::unique_ptr<InstrInfo> create_info_b(TblReader& r, CtorKind ctor_kind) {
	DataReader& reader = r.reader;
	std::string& s = r.s;
	std::vector<std::string>& mnemonics = r.mnemonics;
	const auto read_string = [&r]() { return r.read_string(); };
	std::uint32_t v;
	std::uint32_t v2;
	std::uint32_t v3;
	std::unique_ptr<InstrInfo> info;
	switch (ctor_kind) {
		case CtorKind::CC_1:
			v = reader.read_compressed_u32();
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			info = make_info<SimpleInstrInfo_cc>(v, std::move(mnemonics));
			break;

		case CtorKind::CC_2:
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			mnemonics.emplace_back(read_string());
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_cc>(v, std::move(mnemonics));
			break;

		case CtorKind::CC_3:
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			mnemonics.emplace_back(read_string());
			mnemonics.emplace_back(read_string());
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_cc>(v, std::move(mnemonics));
			break;

		case CtorKind::os_jcc_a_1:
			v2 = reader.read_compressed_u32();
			v = reader.read_compressed_u32();
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			info = make_info<SimpleInstrInfo_os_jcc>(v, v2, std::move(mnemonics), InstrOpInfoFlags::NONE);
			break;

		case CtorKind::os_jcc_a_2:
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			mnemonics.emplace_back(read_string());
			v2 = reader.read_compressed_u32();
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_os_jcc>(v, v2, std::move(mnemonics), InstrOpInfoFlags::NONE);
			break;

		case CtorKind::os_jcc_a_3:
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			mnemonics.emplace_back(read_string());
			mnemonics.emplace_back(read_string());
			v2 = reader.read_compressed_u32();
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_os_jcc>(v, v2, std::move(mnemonics), InstrOpInfoFlags::NONE);
			break;

		case CtorKind::os_jcc_b_1:
			v3 = reader.read_compressed_u32();
			v = reader.read_compressed_u32();
			v2 = reader.read_compressed_u32();
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			info = make_info<SimpleInstrInfo_os_jcc>(v, v3, std::move(mnemonics), v2);
			break;

		case CtorKind::os_jcc_b_2:
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			mnemonics.emplace_back(read_string());
			v3 = reader.read_compressed_u32();
			v = reader.read_compressed_u32();
			v2 = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_os_jcc>(v, v3, std::move(mnemonics), v2);
			break;

		case CtorKind::os_jcc_b_3:
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			mnemonics.emplace_back(read_string());
			mnemonics.emplace_back(read_string());
			v3 = reader.read_compressed_u32();
			v = reader.read_compressed_u32();
			v2 = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_os_jcc>(v, v3, std::move(mnemonics), v2);
			break;

		case CtorKind::os_loopcc:
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			mnemonics.emplace_back(read_string());
			v3 = reader.read_compressed_u32();
			v = reader.read_compressed_u32();
			v2 = static_cast<std::uint32_t>(reader.read_u8());
			info = make_info<SimpleInstrInfo_os_loop>(v, v3, static_cast<Register>(v2), std::move(mnemonics));
			break;

		case CtorKind::os_loop:
			v = reader.read_compressed_u32();
			v2 = static_cast<std::uint32_t>(reader.read_u8());
			mnemonics.clear();
			mnemonics.push_back(std::move(s));
			info = make_info<SimpleInstrInfo_os_loop>(v, NO_CC_INDEX, static_cast<Register>(v2), std::move(mnemonics));
			break;

		case CtorKind::pclmulqdq:
			v = static_cast<std::uint32_t>(reader.read_u8());
			info = make_info<SimpleInstrInfo_pclmulqdq>(std::move(s), get_pseudo_ops(static_cast<PseudoOpsKind>(v)));
			break;

		case CtorKind::pops:
			v = static_cast<std::uint32_t>(reader.read_u8());
			info = make_info<SimpleInstrInfo_pops>(std::move(s), get_pseudo_ops(static_cast<PseudoOpsKind>(v)));
			break;

		case CtorKind::reg:
			v = static_cast<std::uint32_t>(reader.read_u8());
			info = make_info<SimpleInstrInfo_reg>(std::move(s), static_cast<Register>(v));
			break;

		case CtorKind::Reg16:
			info = make_info<SimpleInstrInfo_Reg16>(std::move(s));
			break;

		case CtorKind::Reg32:
			info = make_info<SimpleInstrInfo_Reg32>(std::move(s));
			break;

		case CtorKind::ST1_2:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_ST1>(std::move(s), v, false);
			break;

		case CtorKind::ST1_3:
			v = reader.read_compressed_u32();
			v2 = static_cast<std::uint32_t>(reader.read_u8());
			ICED_DEBUG_ASSERT(v2 <= 1);
			info = make_info<SimpleInstrInfo_ST1>(std::move(s), v, v2 != 0);
			break;

		case CtorKind::ST2:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_ST2>(std::move(s), v);
			break;

		case CtorKind::invlpga:
			v = reader.read_compressed_u32();
			info = make_info<SimpleInstrInfo_invlpga>(v, std::move(s));
			break;

	default:
		ICED_UNREACHABLE();
	}
	return info;
}

struct InstrInfosHolder {
	InstrInfos infos;

	InstrInfosHolder() {
		const auto r = std::make_unique<TblReader>();
		DataReader& reader = r->reader;
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
				r->s.assign(1, 'v');
				r->s.append(r->read_string());
			} else
				r->s.assign(r->read_string());

			std::unique_ptr<InstrInfo> info = create_info_a(*r, ctor_kind, i);
			if (!info)
				info = create_info_b(*r, ctor_kind);

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

} // namespace iced_x86::internal::intel
