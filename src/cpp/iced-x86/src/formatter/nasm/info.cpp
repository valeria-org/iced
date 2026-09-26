// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/nasm/info.rs, fmt_tbl.rs

#include "internal/formatter/nasm/info.hpp"

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
#include "iced_x86/memory_size.hpp"
#include "iced_x86/rounding_control.hpp"
#include "internal/data_reader.hpp"
#include "internal/formatter/fmt_common.hpp"
#include "internal/formatter/fmt_utils.hpp"
#include "internal/formatter/nasm/branch_size_info.hpp"
#include "internal/formatter/nasm/ctor_kind.hpp"
#include "internal/formatter/nasm/far_memory_size_info.hpp"
#include "internal/formatter/nasm/fmt_data.hpp"
#include "internal/formatter/nasm/instr_op_info_flags.hpp"
#include "internal/formatter/nasm/mem_size_tbl.hpp"
#include "internal/formatter/nasm/memory_size_info.hpp"
#include "internal/formatter/nasm/sign_extend_info.hpp"
#include "internal/formatter/pseudo_ops.hpp"
#include "internal/formatter/pseudo_ops_kind.hpp"
#include "internal/formatter/strings_tbl.hpp"

namespace iced_x86::internal::nasm {

static_assert(IcedConstants::MEMORY_SIZE_ENUM_COUNT <= (1U << InstrOpInfoFlags::MEMORY_SIZE_BITS), "");

namespace {

std::uint32_t get_bitness(CodeSize code_size) noexcept {
	static constexpr std::uint32_t CODESIZE_TO_BITNESS[4] = {0, 16, 32, 64};
	static_assert(static_cast<std::uint32_t>(CodeSize::Unknown) == 0, "");
	static_assert(static_cast<std::uint32_t>(CodeSize::Code16) == 1, "");
	static_assert(static_cast<std::uint32_t>(CodeSize::Code32) == 2, "");
	static_assert(static_cast<std::uint32_t>(CodeSize::Code64) == 3, "");
	return CODESIZE_TO_BITNESS[static_cast<std::size_t>(code_size)];
}

// Pseudo op mnemonics (Rust: `&'static [FormatterString]`)
struct PseudoOps {
	const FormatterString* data;
	std::size_t size;

	explicit PseudoOps(const std::vector<FormatterString>& vec) noexcept : data(vec.data()), size(vec.size()) {}
};

class SimpleInstrInfo final : public InstrInfo {
public:
	explicit SimpleInstrInfo(std::string mnemonic) : mnemonic_(std::move(mnemonic)), flags_(InstrOpInfoFlags::NONE) {}
	SimpleInstrInfo(std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags_);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_cc final : public InstrInfo {
public:
	SimpleInstrInfo_cc(std::uint32_t cc_index, std::vector<std::string>&& mnemonics)
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics))), cc_index_(cc_index) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		const auto& mnemonic = get_mnemonic_cc(options, cc_index_, mnemonics_);
		return InstrOpInfo::with_instruction(mnemonic, instruction, FLAGS);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::uint32_t cc_index_;
};

class SimpleInstrInfo_push_imm8 final : public InstrInfo {
public:
	SimpleInstrInfo_push_imm8(std::uint32_t bitness, SignExtendInfo sex_info, std::string mnemonic)
		: mnemonic_(std::move(mnemonic)), bitness_(bitness), sex_info_(sex_info) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = static_cast<std::uint32_t>(sex_info_) << InstrOpInfoFlags::SIGN_EXTEND_INFO_SHIFT;

		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (bitness_ != 0 && instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::OP_SIZE16;
			else if (bitness_ == 32)
				flags |= InstrOpInfoFlags::OP_SIZE32;
			else
				flags |= InstrOpInfoFlags::OP_SIZE64;
		}

		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
	SignExtendInfo sex_info_;
};

class SimpleInstrInfo_push_imm final : public InstrInfo {
public:
	SimpleInstrInfo_push_imm(std::uint32_t bitness, SignExtendInfo sex_info, std::string mnemonic)
		: mnemonic_(std::move(mnemonic)), bitness_(bitness), sex_info_(sex_info) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;

		bool sign_extend = true;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (bitness_ != 0 && instr_bitness != 0 && instr_bitness != bitness_) {
			if (instr_bitness == 64)
				flags |= InstrOpInfoFlags::OP_SIZE16;
		}
		else if (bitness_ == 16 && instr_bitness == 16)
			sign_extend = false;

		if (sign_extend)
			flags |= static_cast<std::uint32_t>(sex_info_) << InstrOpInfoFlags::SIGN_EXTEND_INFO_SHIFT;

		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
	SignExtendInfo sex_info_;
};

class SimpleInstrInfo_SignExt final : public InstrInfo {
public:
	SimpleInstrInfo_SignExt(SignExtendInfo sex_info_reg, SignExtendInfo sex_info_mem, std::string mnemonic, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), sex_info_reg_(sex_info_reg), sex_info_mem_(sex_info_mem), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		ICED_DEBUG_ASSERT(instruction.op_count() == 2);
		const SignExtendInfo sex_info =
			instruction.op0_kind() == OpKind::Memory || instruction.op1_kind() == OpKind::Memory ? sex_info_mem_ : sex_info_reg_;
		const std::uint32_t flags = flags_ | (static_cast<std::uint32_t>(sex_info) << InstrOpInfoFlags::SIGN_EXTEND_INFO_SHIFT);
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	SignExtendInfo sex_info_reg_;
	SignExtendInfo sex_info_mem_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_imul final : public InstrInfo {
public:
	SimpleInstrInfo_imul(SignExtendInfo sex_info, std::string mnemonic) : mnemonic_(std::move(mnemonic)), sex_info_(sex_info) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		const std::uint32_t flags = static_cast<std::uint32_t>(sex_info_) << InstrOpInfoFlags::SIGN_EXTEND_INFO_SHIFT;
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
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
	SignExtendInfo sex_info_;
};

class SimpleInstrInfo_AamAad final : public InstrInfo {
public:
	explicit SimpleInstrInfo_AamAad(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		if (instruction.immediate8() == 10)
			return InstrOpInfo::with_default(mnemonic_);
		return InstrOpInfo::with_instruction(mnemonic_, instruction, InstrOpInfoFlags::NONE);
	}

private:
	FormatterString mnemonic_;
};

constexpr std::uint32_t get_address_size_flags(OpKind op_kind) noexcept {
	switch (op_kind) {
	case OpKind::MemorySegSI:
	case OpKind::MemorySegDI:
	case OpKind::MemoryESDI:
		return InstrOpInfoFlags::ADDR_SIZE16;
	case OpKind::MemorySegESI:
	case OpKind::MemorySegEDI:
	case OpKind::MemoryESEDI:
		return InstrOpInfoFlags::ADDR_SIZE32;
	case OpKind::MemorySegRSI:
	case OpKind::MemorySegRDI:
	case OpKind::MemoryESRDI:
		return InstrOpInfoFlags::ADDR_SIZE64;
	default:
		return 0;
	}
}

class SimpleInstrInfo_String final : public InstrInfo {
public:
	explicit SimpleInstrInfo_String(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		const OpKind op_kind = instruction.op0_kind() != OpKind::Register ? instruction.op0_kind() : instruction.op1_kind();
		const std::uint32_t op_kind_flags = get_address_size_flags(op_kind);
		std::uint32_t instr_flags;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			instr_flags = op_kind_flags;
			break;
		case CodeSize::Code16:
			instr_flags = InstrOpInfoFlags::ADDR_SIZE16;
			break;
		case CodeSize::Code32:
			instr_flags = InstrOpInfoFlags::ADDR_SIZE32;
			break;
		case CodeSize::Code64:
		default:
			instr_flags = InstrOpInfoFlags::ADDR_SIZE64;
			break;
		}
		const std::uint32_t flags = op_kind_flags != instr_flags ? op_kind_flags : 0;
		auto info = InstrOpInfo::with_default(mnemonic_);
		info.flags = flags;
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_XLAT final : public InstrInfo {
public:
	explicit SimpleInstrInfo_XLAT(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		Register base_reg;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			base_reg = instruction.memory_base();
			break;
		case CodeSize::Code16:
			base_reg = Register::BX;
			break;
		case CodeSize::Code32:
			base_reg = Register::EBX;
			break;
		case CodeSize::Code64:
		default:
			base_reg = Register::RBX;
			break;
		}
		std::uint32_t flags = 0;
		const Register mem_base_reg = instruction.memory_base();
		if (mem_base_reg != base_reg) {
			if (mem_base_reg == Register::BX)
				flags |= InstrOpInfoFlags::ADDR_SIZE16;
			else if (mem_base_reg == Register::EBX)
				flags |= InstrOpInfoFlags::ADDR_SIZE32;
			else if (mem_base_reg == Register::RBX)
				flags |= InstrOpInfoFlags::ADDR_SIZE64;
		}
		auto info = InstrOpInfo::with_default(mnemonic_);
		info.flags = flags;
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
			return InstrOpInfo::with_instruction(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		auto info = InstrOpInfo::with_default(str_xchg_);
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

class SimpleInstrInfo_STIG1 final : public InstrInfo {
public:
	SimpleInstrInfo_STIG1(std::string mnemonic, bool pseudo_op) : mnemonic_(std::move(mnemonic)), pseudo_op_(pseudo_op) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		auto info = InstrOpInfo::with_default(mnemonic_);
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

class SimpleInstrInfo_STIG2 final : public InstrInfo {
public:
	SimpleInstrInfo_STIG2(std::string mnemonic, bool pseudo_op) : mnemonic_(std::move(mnemonic)), flags_(InstrOpInfoFlags::NONE), pseudo_op_(pseudo_op) {}
	SimpleInstrInfo_STIG2(std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), flags_(flags), pseudo_op_(false) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		auto info = InstrOpInfo::with_default(mnemonic_);
		info.flags = flags_;
		ICED_DEBUG_ASSERT(instruction.op_count() == 2);
		ICED_DEBUG_ASSERT(instruction.op1_kind() == OpKind::Register && instruction.op1_register() == Register::ST0);
		if (!pseudo_op_ || !(options.use_pseudo_ops() && instruction.op0_register() == Register::ST1)) {
			info.op_count = 1;
			static_assert(static_cast<std::uint32_t>(InstrOpKind::Register) == 0, "");
			// info.op_kinds[0] = InstrOpKind::Register;
			info.op_registers[0] = instruction.op0_register();
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
	bool pseudo_op_;
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
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
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

		auto info = InstrOpInfo::with_default(mnemonic_);
		info.op_count = 2;
		info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
		info.op_indexes[0] = 1;
		info.op_registers[0] = instruction.op1_register();
		info.op_kinds[1] = InstrOpInfo::to_instr_op_kind(instruction.op2_kind());
		info.op_indexes[1] = 2;
		info.op_registers[1] = instruction.op2_register();
		if (instr_bitness != 0 && instr_bitness != bitness) {
			if (bitness == 16)
				info.flags |= InstrOpInfoFlags::ADDR_SIZE16;
			else if (bitness == 32)
				info.flags |= InstrOpInfoFlags::ADDR_SIZE32;
			else
				info.flags |= InstrOpInfoFlags::ADDR_SIZE64;
		}
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_pblendvb final : public InstrInfo {
public:
	SimpleInstrInfo_pblendvb(std::string mnemonic, MemorySize mem_size) : mnemonic_(std::move(mnemonic)), mem_size_(mem_size) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		auto info = InstrOpInfo::with_default(mnemonic_);
		ICED_DEBUG_ASSERT(instruction.op_count() == 2);
		info.op_count = 3;
		info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op0_kind());
		info.op_registers[0] = instruction.op0_register();
		info.op_kinds[1] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
		info.op_indexes[1] = 1;
		info.op_registers[1] = instruction.op1_register();
		static_assert(static_cast<std::uint32_t>(InstrOpKind::Register) == 0, "");
		// info.op_kinds[2] = InstrOpKind::Register;
		info.op_registers[2] = Register::XMM0;
		info.set_memory_size(mem_size_);
		info.op_indexes[2] = InstrInfoConstants::OP_ACCESS_READ;
		return info;
	}

private:
	FormatterString mnemonic_;
	MemorySize mem_size_;
};

class SimpleInstrInfo_reverse final : public InstrInfo {
public:
	explicit SimpleInstrInfo_reverse(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		auto info = InstrOpInfo::with_default(mnemonic_);
		ICED_DEBUG_ASSERT(instruction.op_count() == 2);
		info.op_count = 2;
		info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
		info.op_indexes[0] = 1;
		info.op_registers[0] = instruction.op1_register();
		info.op_kinds[1] = InstrOpInfo::to_instr_op_kind(instruction.op0_kind());
		info.op_registers[1] = instruction.op0_register();
		info.set_memory_size(instruction.memory_size());
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_OpSize final : public InstrInfo {
public:
	SimpleInstrInfo_OpSize(CodeSize code_size, std::string mnemonic, std::string mnemonic16, std::string mnemonic32, std::string mnemonic64)
		: mnemonics_{FormatterString(std::move(mnemonic)), FormatterString(std::move(mnemonic16)), FormatterString(std::move(mnemonic32)),
					 FormatterString(std::move(mnemonic64))}
		, code_size_(code_size) {
		static_assert(static_cast<std::uint32_t>(CodeSize::Unknown) == 0, "");
		static_assert(static_cast<std::uint32_t>(CodeSize::Code16) == 1, "");
		static_assert(static_cast<std::uint32_t>(CodeSize::Code32) == 2, "");
		static_assert(static_cast<std::uint32_t>(CodeSize::Code64) == 3, "");
	}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		const auto& mnemonic = instruction.code_size() == code_size_ ? mnemonics_[static_cast<std::size_t>(CodeSize::Unknown)]
																	 : mnemonics_[static_cast<std::size_t>(code_size_)];
		return InstrOpInfo::with_instruction(mnemonic, instruction, InstrOpInfoFlags::NONE);
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
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		if (instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		const auto& mnemonic = mnemonics_[static_cast<std::size_t>(instruction.code_size())];
		return InstrOpInfo::with_instruction(mnemonic, instruction, flags);
	}

private:
	std::array<FormatterString, 4> mnemonics_;
};

class SimpleInstrInfo_OpSize3 final : public InstrInfo {
public:
	SimpleInstrInfo_OpSize3(std::uint32_t bitness, std::string mnemonic_default, std::string mnemonic_full)
		: mnemonic_default_(std::move(mnemonic_default)), mnemonic_full_(std::move(mnemonic_full)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		const auto& mnemonic = instr_bitness == 0 || (instr_bitness & bitness_) != 0 ? mnemonic_default_ : mnemonic_full_;
		return InstrOpInfo::with_instruction(mnemonic, instruction, InstrOpInfoFlags::NONE);
	}

private:
	FormatterString mnemonic_default_;
	FormatterString mnemonic_full_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_os final : public InstrInfo {
public:
	SimpleInstrInfo_os(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness), flags_(InstrOpInfoFlags::NONE) {}
	SimpleInstrInfo_os(std::uint32_t bitness, std::string mnemonic, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), bitness_(bitness), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
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
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_os_mem final : public InstrInfo {
public:
	SimpleInstrInfo_os_mem(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		const bool has_mem_op = instruction.op0_kind() == OpKind::Memory || instruction.op1_kind() == OpKind::Memory;
		if (has_mem_op &&
			!(instr_bitness == 0 || (instr_bitness != 64 && instr_bitness == bitness_) || (instr_bitness == 64 && bitness_ == 32))) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::OP_SIZE16;
			else if (bitness_ == 32)
				flags |= InstrOpInfoFlags::OP_SIZE32;
			else
				flags |= InstrOpInfoFlags::OP_SIZE64;
		}
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_os_mem2 final : public InstrInfo {
public:
	SimpleInstrInfo_os_mem2(std::uint32_t bitness, std::string mnemonic, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), bitness_(bitness), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = flags_;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (instr_bitness != 0 && (instr_bitness & bitness_) == 0) {
			if (instr_bitness != 16)
				flags |= InstrOpInfoFlags::OP_SIZE16;
			else
				flags |= InstrOpInfoFlags::OP_SIZE32;
		}
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_os_mem_reg16 final : public InstrInfo {
public:
	SimpleInstrInfo_os_mem_reg16(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		ICED_DEBUG_ASSERT(instruction.op_count() == 1);
		if (instruction.op0_kind() == OpKind::Memory) {
			if (!(instr_bitness == 0 || (instr_bitness != 64 && instr_bitness == bitness_) || (instr_bitness == 64 && bitness_ == 32))) {
				if (bitness_ == 16)
					flags |= InstrOpInfoFlags::OP_SIZE16;
				else if (bitness_ == 32)
					flags |= InstrOpInfoFlags::OP_SIZE32;
				else
					flags |= InstrOpInfoFlags::OP_SIZE64;
			}
		}
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
		if (instruction.op0_kind() == OpKind::Register) {
			Register reg = info.op_registers[0];
			std::uint32_t reg_size;
			if (Register::AX <= reg && reg <= Register::R15W)
				reg_size = 16;
			else if (Register::EAX <= reg && reg <= Register::R15D) {
				reg = r_to_r16(reg);
				reg_size = 32;
			}
			else if (Register::RAX <= reg && reg <= Register::R15) {
				reg = r_to_r16(reg);
				reg_size = 64;
			}
			else
				reg_size = 0;
			ICED_DEBUG_ASSERT(reg_size != 0);
			if (reg_size != 0) {
				info.op_registers[0] = reg;
				if (!((instr_bitness != 64 && instr_bitness == reg_size) || (instr_bitness == 64 && reg_size == 32))) {
					if (bitness_ == 16)
						info.flags |= InstrOpInfoFlags::OP_SIZE16;
					else if (bitness_ == 32)
						info.flags |= InstrOpInfoFlags::OP_SIZE32;
					else
						info.flags |= InstrOpInfoFlags::OP_SIZE64;
				}
			}
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_os_jcc final : public InstrInfo {
public:
	SimpleInstrInfo_os_jcc(std::uint32_t bitness, std::uint32_t cc_index, std::vector<std::string>&& mnemonics)
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics))), bitness_(bitness), cc_index_(cc_index), flags_(InstrOpInfoFlags::NONE) {}
	SimpleInstrInfo_os_jcc(std::uint32_t bitness, std::uint32_t cc_index, std::vector<std::string>&& mnemonics, std::uint32_t flags)
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics))), bitness_(bitness), cc_index_(cc_index), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = flags_;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		if (flags != InstrOpInfoFlags::NONE) {
			if (instr_bitness != 0 && instr_bitness != bitness_) {
				if (bitness_ == 16)
					flags |= InstrOpInfoFlags::OP_SIZE16;
				else if (bitness_ == 32)
					flags |= InstrOpInfoFlags::OP_SIZE32;
				else
					flags |= InstrOpInfoFlags::OP_SIZE64;
			}
		}
		else {
			BranchSizeInfo branch_info = BranchSizeInfo::Near;
			if (instr_bitness != 0 && instr_bitness != bitness_) {
				if (bitness_ == 16)
					branch_info = BranchSizeInfo::NearWord;
				else if (bitness_ == 32)
					branch_info = BranchSizeInfo::NearDword;
			}
			flags |= static_cast<std::uint32_t>(branch_info) << InstrOpInfoFlags::BRANCH_SIZE_INFO_SHIFT;
		}
		const Register prefix_seg = instruction.segment_prefix();
		if (prefix_seg == Register::CS)
			flags |= InstrOpInfoFlags::JCC_NOT_TAKEN;
		else if (prefix_seg == Register::DS)
			flags |= InstrOpInfoFlags::JCC_TAKEN;
		if (instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		const auto& mnemonic = get_mnemonic_cc(options, cc_index_, mnemonics_);
		return InstrOpInfo::with_instruction(mnemonic, instruction, flags);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::uint32_t bitness_;
	std::uint32_t cc_index_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_os_loop final : public InstrInfo {
public:
	SimpleInstrInfo_os_loop(std::uint32_t bitness, std::uint32_t cc_index, Register register_, std::vector<std::string>&& mnemonics)
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
		const bool add_reg = expected_reg != register_;
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				flags |= InstrOpInfoFlags::OP_SIZE16;
			else if (bitness_ == 32)
				flags |= InstrOpInfoFlags::OP_SIZE32;
			else
				flags |= InstrOpInfoFlags::OP_SIZE64;
		}
		const auto& mnemonic = cc_index_ == 0xFFFF'FFFF ? mnemonics_[0] : get_mnemonic_cc(options, cc_index_, mnemonics_);
		auto info = InstrOpInfo::with_instruction(mnemonic, instruction, flags);
		if (add_reg) {
			ICED_DEBUG_ASSERT(info.op_count == 1);
			info.op_count = 2;
			info.op_kinds[1] = InstrOpKind::Register;
			info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_READ_WRITE;
			info.op_registers[1] = register_;
		}
		return info;
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::uint32_t bitness_;
	std::uint32_t cc_index_;
	Register register_;
};

class SimpleInstrInfo_os_call final : public InstrInfo {
public:
	SimpleInstrInfo_os_call(std::uint32_t bitness, std::string mnemonic, bool can_have_bnd_prefix)
		: mnemonic_(std::move(mnemonic)), bitness_(bitness), can_have_bnd_prefix_(can_have_bnd_prefix) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		if (can_have_bnd_prefix_ && instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		BranchSizeInfo branch_info = BranchSizeInfo::None;
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				branch_info = BranchSizeInfo::Word;
			else if (bitness_ == 32)
				branch_info = BranchSizeInfo::Dword;
		}
		flags |= static_cast<std::uint32_t>(branch_info) << InstrOpInfoFlags::BRANCH_SIZE_INFO_SHIFT;
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
	bool can_have_bnd_prefix_;
};

class SimpleInstrInfo_far final : public InstrInfo {
public:
	SimpleInstrInfo_far(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		BranchSizeInfo branch_info = BranchSizeInfo::None;
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				branch_info = BranchSizeInfo::Word;
			else
				branch_info = BranchSizeInfo::Dword;
		}
		flags |= static_cast<std::uint32_t>(branch_info) << InstrOpInfoFlags::BRANCH_SIZE_INFO_SHIFT;
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
};

class SimpleInstrInfo_far_mem final : public InstrInfo {
public:
	SimpleInstrInfo_far_mem(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::SHOW_NO_MEM_SIZE_FORCE_SIZE;
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		FarMemorySizeInfo far_mem_size_info = FarMemorySizeInfo::None;
		if (instr_bitness != 0 && instr_bitness != bitness_) {
			if (bitness_ == 16)
				far_mem_size_info = FarMemorySizeInfo::Word;
			else
				far_mem_size_info = FarMemorySizeInfo::Dword;
		}
		flags |= static_cast<std::uint32_t>(far_mem_size_info) << InstrOpInfoFlags::FAR_MEMORY_SIZE_INFO_SHIFT;
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
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
		MemorySizeInfo mem_size_info = MemorySizeInfo::None;
		if (instr_bitness == 64) {
			if (mem_size == 32)
				flags |= InstrOpInfoFlags::ADDR_SIZE32;
			else
				mem_size_info = MemorySizeInfo::Qword;
		}
		else if (instr_bitness != mem_size) {
			ICED_DEBUG_ASSERT(mem_size == 16 || mem_size == 32);
			if (mem_size == 16)
				mem_size_info = MemorySizeInfo::Word;
			else
				mem_size_info = MemorySizeInfo::Dword;
		}
		flags |= static_cast<std::uint32_t>(mem_size_info) << InstrOpInfoFlags::MEMORY_SIZE_INFO_SHIFT;
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
};

// Rust: `SimpleInstrInfo_er::move_operands()`
void move_operands(InstrOpInfo& info, std::uint32_t index, InstrOpKind new_op_kind) noexcept {
	ICED_DEBUG_ASSERT(info.op_count <= 4);

	switch (index) {
	case 2:
		ICED_DEBUG_ASSERT(info.op_count < 4 || info.op_kinds[3] != InstrOpKind::Register);
		info.op_kinds[4] = info.op_kinds[3];
		info.op_kinds[3] = info.op_kinds[2];
		info.op_registers[3] = info.op_registers[2];
		info.op_kinds[2] = new_op_kind;
		info.op_indexes[4] = info.op_indexes[3];
		info.op_indexes[3] = info.op_indexes[2];
		info.op_indexes[2] = InstrInfoConstants::OP_ACCESS_NONE;
		info.op_count++;
		break;

	case 3:
		ICED_DEBUG_ASSERT(info.op_count < 4 || info.op_kinds[3] != InstrOpKind::Register);
		info.op_kinds[4] = info.op_kinds[3];
		info.op_kinds[3] = new_op_kind;
		info.op_indexes[4] = info.op_indexes[3];
		info.op_indexes[3] = InstrInfoConstants::OP_ACCESS_NONE;
		info.op_count++;
		break;

	default:
		ICED_UNREACHABLE();
	}
}

class SimpleInstrInfo_er final : public InstrInfo {
public:
	SimpleInstrInfo_er(std::uint32_t er_index, std::string mnemonic) : mnemonic_(std::move(mnemonic)), er_index_(er_index), flags_(InstrOpInfoFlags::NONE) {}
	SimpleInstrInfo_er(std::uint32_t er_index, std::string mnemonic, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), er_index_(er_index), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, flags_);
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
					case RoundingControl::None:
					default:
						return info;
					}
				}
				else {
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
					case RoundingControl::None:
					default:
						return info;
					}
				}
				move_operands(info, er_index_, rc_op_kind);
			}
			else if (instruction.suppress_all_exceptions())
				move_operands(info, er_index_, InstrOpKind::Sae);
		}
		else {
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
				case RoundingControl::None:
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
	std::uint32_t er_index_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_sae final : public InstrInfo {
public:
	SimpleInstrInfo_sae(std::uint32_t sae_index, std::string mnemonic) : mnemonic_(std::move(mnemonic)), sae_index_(sae_index) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		if (instruction.suppress_all_exceptions())
			move_operands(info, sae_index_, InstrOpKind::Sae);
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t sae_index_;
};

class SimpleInstrInfo_bcst final : public InstrInfo {
public:
	SimpleInstrInfo_bcst(std::string mnemonic, std::uint32_t flags_no_broadcast, const MemSizeInfo* mem_size_tbl)
		: mnemonic_(std::move(mnemonic)), flags_no_broadcast_(flags_no_broadcast), mem_size_tbl_(mem_size_tbl) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		const FormatterString* bcst_to = mem_size_tbl_[static_cast<std::size_t>(instruction.memory_size())].bcst_to;
		const std::uint32_t flags = !bcst_to->is_default() ? InstrOpInfoFlags::NONE : flags_no_broadcast_;
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_no_broadcast_;
	const MemSizeInfo* mem_size_tbl_;
};

class SimpleInstrInfo_bnd final : public InstrInfo {
public:
	SimpleInstrInfo_bnd(std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = flags_;
		if (instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
};

void remove_last_op(InstrOpInfo& info) noexcept {
	switch (info.op_count) {
	case 5:
		info.op_indexes[4] = OP_ACCESS_INVALID;
		break;
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
	SimpleInstrInfo_pops(std::string mnemonic, PseudoOps pseudo_ops) : mnemonic_(std::move(mnemonic)), pseudo_ops_(pseudo_ops) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		if (instruction.suppress_all_exceptions())
			move_operands(info, instruction.op_count() - 1, InstrOpKind::Sae);
		const std::size_t imm = instruction.immediate8();
		if (options.use_pseudo_ops() && imm < pseudo_ops_.size) {
			info.mnemonic = &pseudo_ops_.data[imm];
			remove_last_op(info);
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	PseudoOps pseudo_ops_;
};

class SimpleInstrInfo_pclmulqdq final : public InstrInfo {
public:
	SimpleInstrInfo_pclmulqdq(std::string mnemonic, PseudoOps pseudo_ops) : mnemonic_(std::move(mnemonic)), pseudo_ops_(pseudo_ops) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		if (options.use_pseudo_ops()) {
			std::size_t index;
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
				return info;
			}
			ICED_ASSERT(index < pseudo_ops_.size);
			info.mnemonic = &pseudo_ops_.data[index];
			remove_last_op(info);
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	PseudoOps pseudo_ops_;
};

class SimpleInstrInfo_Reg16 final : public InstrInfo {
public:
	explicit SimpleInstrInfo_Reg16(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		constexpr std::uint32_t FLAGS = InstrOpInfoFlags::NONE;
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, FLAGS);
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
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, FLAGS);
		info.op_registers[0] = r64_to_r32(info.op_registers[0]);
		info.op_registers[1] = r64_to_r32(info.op_registers[1]);
		info.op_registers[2] = r64_to_r32(info.op_registers[2]);
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_invlpga final : public InstrInfo {
public:
	SimpleInstrInfo_invlpga(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		static_cast<void>(instruction);
		auto info = InstrOpInfo::with_default(mnemonic_);
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
	SimpleInstrInfo_DeclareData(Code code, std::string mnemonic) : mnemonic_(std::move(mnemonic)), op_kind_(to_op_kind(code)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, InstrOpInfoFlags::MNEMONIC_IS_DIRECTIVE);
		info.op_count = static_cast<std::uint8_t>(instruction.declare_data_len());
		for (std::size_t i = 0; i < IcedConstants::MAX_OP_COUNT; i++) {
			info.op_kinds[i] = op_kind_;
			info.op_indexes[i] = InstrInfoConstants::OP_ACCESS_READ;
		}
		return info;
	}

private:
	static InstrOpKind to_op_kind(Code code) noexcept {
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

void add_suffix(std::string& result, const std::string& s, char c) {
	result.clear();
	result.reserve(s.size() + 1);
	result += s;
	result += c;
}

template <typename... Args>
std::vector<std::string>&& make_vec(std::vector<std::string>& vec, Args&&... args) {
	vec.clear();
	vec.reserve(sizeof...(Args));
	(vec.push_back(std::move(args)), ...);
	return std::move(vec);
}

struct AllInfosHolder {
	std::vector<std::unique_ptr<InstrInfo>> infos_storage;
	std::array<const InstrInfo*, IcedConstants::CODE_ENUM_COUNT> infos;

	// Creates an instruction info and stores it in `infos_storage` (a function so its temporaries aren't in the caller's stack frame)
	template <typename T, typename... Args>
	ICED_NOINLINE InstrInfo* add(Args&&... args) {
		infos_storage.push_back(std::make_unique<T>(std::forward<Args>(args)...));
		return infos_storage.back().get();
	}

	AllInfosHolder() {
		infos_storage.reserve(IcedConstants::CODE_ENUM_COUNT);
		DataReader reader(FORMATTER_TBL_DATA, FORMATTER_TBL_DATA_SIZE);
		const auto strings = get_strings_table_ref();
		const MemSizeInfo* mem_size_tbl = get_mem_size_tbl();
		const auto read_string = [&strings, &reader](std::string& result) {
			const std::size_t index = reader.read_compressed_u32();
			ICED_ASSERT(index < strings.size());
			result = strings[index];
		};
		const auto read_code_size = [&reader]() -> CodeSize {
			const std::size_t v = reader.read_u8();
			ICED_ASSERT(v <= static_cast<std::size_t>(CodeSize::Code64));
			return static_cast<CodeSize>(v);
		};
		const auto read_register = [&reader]() -> Register { return static_cast<Register>(reader.read_u8()); };
		const auto read_sign_extend_info = [&reader]() -> SignExtendInfo {
			const std::size_t v = reader.read_u8();
			ICED_ASSERT(v <= static_cast<std::size_t>(SignExtendInfo::Sex4));
			return static_cast<SignExtendInfo>(v);
		};
		const auto read_pseudo_ops = [&reader]() -> PseudoOps {
			return PseudoOps(get_pseudo_ops(static_cast<PseudoOpsKind>(reader.read_u8())));
		};
		std::size_t prev_index = 0;
		bool has_prev_index = false;
		for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
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
			}
			else {
				prev_index = reader.index() - 1;
				has_prev_index = true;
			}
			// The strings are declared here so the (debug build) stack frame doesn't contain a temporary per `case`
			std::string s;
			std::string s2;
			std::string s3;
			std::string s4;
			std::vector<std::string> mnemonics;
			std::vector<std::string> mnemonics_other;
			if ((f & 0x80) != 0) {
				const std::size_t index = reader.read_compressed_u32();
				ICED_ASSERT(index < strings.size());
				const std::string_view s0 = strings[index];
				s.reserve(s0.size() + 1);
				s += 'v';
				s += s0;
			}
			else
				read_string(s);

			std::uint32_t v;
			std::uint32_t v2;
			std::uint32_t v3;
			InstrInfo* info = nullptr;
			switch (ctor_kind) {
			case CtorKind::Normal_1:
				info = add<SimpleInstrInfo>(std::move(s));
				break;

			case CtorKind::Normal_2:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo>(std::move(s), v);
				break;

			case CtorKind::AamAad:
				info = add<SimpleInstrInfo_AamAad>(std::move(s));
				break;

			case CtorKind::asz:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_as>(v, std::move(s));
				break;

			case CtorKind::String:
				info = add<SimpleInstrInfo_String>(std::move(s));
				break;

			case CtorKind::bcst:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_bcst>(std::move(s), v, mem_size_tbl);
				break;

			case CtorKind::bnd:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_bnd>(std::move(s), v);
				break;

			case CtorKind::DeclareData:
				info = add<SimpleInstrInfo_DeclareData>(static_cast<Code>(i), std::move(s));
				break;

			case CtorKind::er_2:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_er>(v, std::move(s));
				break;

			case CtorKind::er_3:
				v = reader.read_compressed_u32();
				v2 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_er>(v, std::move(s), v2);
				break;

			case CtorKind::far:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_far>(v, std::move(s));
				break;

			case CtorKind::far_mem:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_far_mem>(v, std::move(s));
				break;

			case CtorKind::invlpga:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_invlpga>(v, std::move(s));
				break;

			case CtorKind::maskmovq:
				info = add<SimpleInstrInfo_maskmovq>(std::move(s));
				break;

			case CtorKind::movabs:
				info = add<SimpleInstrInfo_movabs>(std::move(s));
				break;

			case CtorKind::nop: {
				v = reader.read_compressed_u32();
				const Register r = read_register();
				info = add<SimpleInstrInfo_nop>(v, std::move(s), r);
				break;
			}

			case CtorKind::OpSize: {
				const CodeSize code_size = read_code_size();
				add_suffix(s2, s, 'w');
				add_suffix(s3, s, 'd');
				add_suffix(s4, s, 'q');
				info = add<SimpleInstrInfo_OpSize>(code_size, std::move(s), std::move(s2), std::move(s3), std::move(s4));
				break;
			}

			case CtorKind::OpSize2_bnd: {
				read_string(s2);
				read_string(s3);
				read_string(s4);
				info = add<SimpleInstrInfo_OpSize2_bnd>(std::move(s), std::move(s2), std::move(s3), std::move(s4));
				break;
			}

			case CtorKind::OpSize3: {
				const char c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_OpSize3>(v, std::move(s), std::move(s2));
				break;
			}

			case CtorKind::os_2:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os>(v, std::move(s));
				break;

			case CtorKind::os_3:
				v = reader.read_compressed_u32();
				v2 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os>(v, std::move(s), v2);
				break;

			case CtorKind::os_call:
				v = reader.read_compressed_u32();
				v2 = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v2 <= 1);
				info = add<SimpleInstrInfo_os_call>(v, std::move(s), v2 != 0);
				break;

			case CtorKind::CC_1:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v, make_vec(mnemonics, std::move(s)));
				break;

			case CtorKind::CC_2: {
				read_string(s2);
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v, make_vec(mnemonics, std::move(s), std::move(s2)));
				break;
			}

			case CtorKind::CC_3: {
				read_string(s2);
				read_string(s3);
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v, make_vec(mnemonics, std::move(s), std::move(s2), std::move(s3)));
				break;
			}

			case CtorKind::os_jcc_a_1:
				v2 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os_jcc>(v, v2, make_vec(mnemonics, std::move(s)));
				break;

			case CtorKind::os_jcc_a_2: {
				read_string(s2);
				v2 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os_jcc>(v, v2, make_vec(mnemonics, std::move(s), std::move(s2)));
				break;
			}

			case CtorKind::os_jcc_a_3: {
				read_string(s2);
				read_string(s3);
				v2 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os_jcc>(v, v2, make_vec(mnemonics, std::move(s), std::move(s2), std::move(s3)));
				break;
			}

			case CtorKind::os_jcc_b_1:
				v3 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				v2 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os_jcc>(v, v3, make_vec(mnemonics, std::move(s)), v2);
				break;

			case CtorKind::os_jcc_b_2: {
				read_string(s2);
				v3 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				v2 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os_jcc>(v, v3, make_vec(mnemonics, std::move(s), std::move(s2)), v2);
				break;
			}

			case CtorKind::os_jcc_b_3: {
				read_string(s2);
				read_string(s3);
				v3 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				v2 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os_jcc>(v, v3, make_vec(mnemonics, std::move(s), std::move(s2), std::move(s3)), v2);
				break;
			}

			case CtorKind::os_loopcc: {
				read_string(s2);
				v3 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				const Register r = read_register();
				info = add<SimpleInstrInfo_os_loop>(v, v3, r, make_vec(mnemonics, std::move(s), std::move(s2)));
				break;
			}

			case CtorKind::os_loop: {
				v = reader.read_compressed_u32();
				const Register r = read_register();
				info = add<SimpleInstrInfo_os_loop>(v, 0xFFFF'FFFF, r, make_vec(mnemonics, std::move(s)));
				break;
			}

			case CtorKind::os_mem:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os_mem>(v, std::move(s));
				break;

			case CtorKind::os_mem_reg16:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os_mem_reg16>(v, std::move(s));
				break;

			case CtorKind::os_mem2:
				v = reader.read_compressed_u32();
				v2 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_os_mem2>(v, std::move(s), v2);
				break;

			case CtorKind::pblendvb: {
				const std::size_t mem_size = reader.read_u8();
				ICED_ASSERT(mem_size < IcedConstants::MEMORY_SIZE_ENUM_COUNT);
				info = add<SimpleInstrInfo_pblendvb>(std::move(s), static_cast<MemorySize>(mem_size));
				break;
			}

			case CtorKind::pclmulqdq:
				info = add<SimpleInstrInfo_pclmulqdq>(std::move(s), read_pseudo_ops());
				break;

			case CtorKind::pops:
				info = add<SimpleInstrInfo_pops>(std::move(s), read_pseudo_ops());
				break;

			case CtorKind::Reg16:
				info = add<SimpleInstrInfo_Reg16>(std::move(s));
				break;

			case CtorKind::Reg32:
				info = add<SimpleInstrInfo_Reg32>(std::move(s));
				break;

			case CtorKind::reverse:
				info = add<SimpleInstrInfo_reverse>(std::move(s));
				break;

			case CtorKind::sae:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_sae>(v, std::move(s));
				break;

			case CtorKind::push_imm8: {
				v = reader.read_compressed_u32();
				const SignExtendInfo sex_info = read_sign_extend_info();
				info = add<SimpleInstrInfo_push_imm8>(v, sex_info, std::move(s));
				break;
			}

			case CtorKind::push_imm: {
				v = reader.read_compressed_u32();
				const SignExtendInfo sex_info = read_sign_extend_info();
				info = add<SimpleInstrInfo_push_imm>(v, sex_info, std::move(s));
				break;
			}

			case CtorKind::SignExt_3: {
				const SignExtendInfo sex_info = read_sign_extend_info();
				v2 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_SignExt>(sex_info, sex_info, std::move(s), v2);
				break;
			}

			case CtorKind::SignExt_4: {
				const SignExtendInfo sex_info_reg = read_sign_extend_info();
				const SignExtendInfo sex_info_mem = read_sign_extend_info();
				v3 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_SignExt>(sex_info_reg, sex_info_mem, std::move(s), v3);
				break;
			}

			case CtorKind::imul:
				info = add<SimpleInstrInfo_imul>(read_sign_extend_info(), std::move(s));
				break;

			case CtorKind::STIG1:
				v = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v <= 1);
				info = add<SimpleInstrInfo_STIG1>(std::move(s), v != 0);
				break;

			case CtorKind::STIG2_2a:
				v = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v <= 1);
				info = add<SimpleInstrInfo_STIG2>(std::move(s), v != 0);
				break;

			case CtorKind::STIG2_2b:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_STIG2>(std::move(s), v);
				break;

			case CtorKind::XLAT:
				info = add<SimpleInstrInfo_XLAT>(std::move(s));
				break;

			case CtorKind::Previous:
			default:
				ICED_UNREACHABLE();
			}

			infos[i] = info;
			if (restore_index)
				reader.set_index(current_index);
		}
		ICED_DEBUG_ASSERT(!reader.can_read());
	}
};

} // namespace

const InstrInfo* const* get_all_infos() {
	static const AllInfosHolder holder;
	return holder.infos.data();
}

} // namespace iced_x86::internal::nasm
