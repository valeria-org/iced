// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/masm/info.rs, fmt_tbl.rs

#include "internal/formatter/masm/info.hpp"

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
#include "internal/data_reader.hpp"
#include "internal/formatter/fmt_common.hpp"
#include "internal/formatter/fmt_utils.hpp"
#include "internal/formatter/masm/ctor_kind.hpp"
#include "internal/formatter/masm/fmt_data.hpp"
#include "internal/formatter/masm/instr_op_info_flags.hpp"
#include "internal/formatter/pseudo_ops.hpp"
#include "internal/formatter/pseudo_ops_kind.hpp"
#include "internal/formatter/strings_tbl.hpp"

namespace iced_x86::internal::masm {

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
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics))), cc_index_(cc_index), flags_(InstrOpInfoFlags::NONE) {}
	SimpleInstrInfo_cc(std::uint32_t cc_index, std::vector<std::string>&& mnemonics, std::uint32_t flags)
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics))), cc_index_(cc_index), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		const auto& mnemonic = get_mnemonic_cc(options, cc_index_, mnemonics_);
		return InstrOpInfo::with_instruction(mnemonic, instruction, flags_);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::uint32_t cc_index_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_memsize final : public InstrInfo {
public:
	SimpleInstrInfo_memsize(std::uint32_t bitness, std::string mnemonic) : mnemonic_(std::move(mnemonic)), bitness_(bitness) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		const std::uint32_t instr_bitness = get_bitness(instruction.code_size());
		const std::uint32_t flags =
			instr_bitness == 0 || (instr_bitness & bitness_) != 0
				? InstrOpInfoFlags::MEM_SIZE_NOTHING
				: InstrOpInfoFlags::MEM_SIZE_NORMAL | InstrOpInfoFlags::SHOW_NO_MEM_SIZE_FORCE_SIZE | InstrOpInfoFlags::SHOW_MIN_MEM_SIZE_FORCE_SIZE;
		return InstrOpInfo::with_instruction(mnemonic_, instruction, flags);
	}

private:
	FormatterString mnemonic_;
	std::uint32_t bitness_;
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

class SimpleInstrInfo_Int3 final : public InstrInfo {
public:
	explicit SimpleInstrInfo_Int3(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		static_cast<void>(instruction);
		auto info = InstrOpInfo::with_default(mnemonic_);
		info.op_count = 1;
		info.op_kinds[0] = InstrOpKind::ExtraImmediate8_Value3;
		info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ;
		return info;
	}

private:
	FormatterString mnemonic_;
};

constexpr std::uint32_t FLAGS_STRING_SHORT_FORM = InstrOpInfoFlags::SHOW_NO_MEM_SIZE_FORCE_SIZE | InstrOpInfoFlags::SHOW_MIN_MEM_SIZE_FORCE_SIZE;

// Base class of the string instruction infos that have a short form without operands (eg. `movsb`)
class StringInstrInfo : public InstrInfo {
protected:
	StringInstrInfo(std::string mnemonic_args, std::string mnemonic_no_args)
		: mnemonic_args_(std::move(mnemonic_args)), mnemonic_no_args_(std::move(mnemonic_no_args)) {}

	FormatterString mnemonic_args_;
	FormatterString mnemonic_no_args_;
};

class SimpleInstrInfo_YD final : public StringInstrInfo {
public:
	SimpleInstrInfo_YD(std::string mnemonic_args, std::string mnemonic_no_args) : StringInstrInfo(std::move(mnemonic_args), std::move(mnemonic_no_args)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		OpKind short_form_op_kind;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			short_form_op_kind = instruction.op0_kind();
			break;
		case CodeSize::Code16:
			short_form_op_kind = OpKind::MemoryESDI;
			break;
		case CodeSize::Code32:
			short_form_op_kind = OpKind::MemoryESEDI;
			break;
		case CodeSize::Code64:
		default:
			short_form_op_kind = OpKind::MemoryESRDI;
			break;
		}
		const bool short_form = instruction.op0_kind() == short_form_op_kind;
		if (!short_form)
			return InstrOpInfo::with_instruction(mnemonic_args_, instruction, FLAGS_STRING_SHORT_FORM);
		auto info = InstrOpInfo::with_default(mnemonic_no_args_);
		info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
		return info;
	}
};

class SimpleInstrInfo_DX final : public StringInstrInfo {
public:
	SimpleInstrInfo_DX(std::string mnemonic_args, std::string mnemonic_no_args) : StringInstrInfo(std::move(mnemonic_args), std::move(mnemonic_no_args)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		OpKind short_form_op_kind;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			short_form_op_kind = instruction.op1_kind();
			break;
		case CodeSize::Code16:
			short_form_op_kind = OpKind::MemorySegSI;
			break;
		case CodeSize::Code32:
			short_form_op_kind = OpKind::MemorySegESI;
			break;
		case CodeSize::Code64:
		default:
			short_form_op_kind = OpKind::MemorySegRSI;
			break;
		}
		const bool short_form = instruction.op1_kind() == short_form_op_kind &&
								(instruction.segment_prefix() == Register::None || !show_segment_prefix(Register::None, instruction, options));
		if (!short_form)
			return InstrOpInfo::with_instruction(mnemonic_args_, instruction, FLAGS_STRING_SHORT_FORM);
		auto info = InstrOpInfo::with_default(mnemonic_no_args_);
		info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
		return info;
	}
};

class SimpleInstrInfo_YX final : public StringInstrInfo {
public:
	SimpleInstrInfo_YX(std::string mnemonic_args, std::string mnemonic_no_args) : StringInstrInfo(std::move(mnemonic_args), std::move(mnemonic_no_args)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		OpKind short_form_op_kind;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			short_form_op_kind = instruction.op0_kind();
			break;
		case CodeSize::Code16:
			short_form_op_kind = OpKind::MemoryESDI;
			break;
		case CodeSize::Code32:
			short_form_op_kind = OpKind::MemoryESEDI;
			break;
		case CodeSize::Code64:
		default:
			short_form_op_kind = OpKind::MemoryESRDI;
			break;
		}
		const bool short_form = instruction.op0_kind() == short_form_op_kind &&
								(instruction.segment_prefix() == Register::None || !show_segment_prefix(Register::None, instruction, options));
		if (!short_form)
			return InstrOpInfo::with_instruction(mnemonic_args_, instruction, FLAGS_STRING_SHORT_FORM);
		auto info = InstrOpInfo::with_default(mnemonic_no_args_);
		info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
		return info;
	}
};

class SimpleInstrInfo_XY final : public StringInstrInfo {
public:
	SimpleInstrInfo_XY(std::string mnemonic_args, std::string mnemonic_no_args) : StringInstrInfo(std::move(mnemonic_args), std::move(mnemonic_no_args)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		OpKind short_form_op_kind;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			short_form_op_kind = instruction.op1_kind();
			break;
		case CodeSize::Code16:
			short_form_op_kind = OpKind::MemoryESDI;
			break;
		case CodeSize::Code32:
			short_form_op_kind = OpKind::MemoryESEDI;
			break;
		case CodeSize::Code64:
		default:
			short_form_op_kind = OpKind::MemoryESRDI;
			break;
		}
		const bool short_form = instruction.op1_kind() == short_form_op_kind &&
								(instruction.segment_prefix() == Register::None || !show_segment_prefix(Register::None, instruction, options));
		if (!short_form)
			return InstrOpInfo::with_instruction(mnemonic_args_, instruction, FLAGS_STRING_SHORT_FORM);
		auto info = InstrOpInfo::with_default(mnemonic_no_args_);
		info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
		return info;
	}
};

class SimpleInstrInfo_YA final : public StringInstrInfo {
public:
	SimpleInstrInfo_YA(std::string mnemonic_args, std::string mnemonic_no_args) : StringInstrInfo(std::move(mnemonic_args), std::move(mnemonic_no_args)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		OpKind short_form_op_kind;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			short_form_op_kind = instruction.op0_kind();
			break;
		case CodeSize::Code16:
			short_form_op_kind = OpKind::MemoryESDI;
			break;
		case CodeSize::Code32:
			short_form_op_kind = OpKind::MemoryESEDI;
			break;
		case CodeSize::Code64:
		default:
			short_form_op_kind = OpKind::MemoryESRDI;
			break;
		}
		const bool short_form = instruction.op0_kind() == short_form_op_kind;
		if (!short_form) {
			auto info = InstrOpInfo::with_default(mnemonic_args_);
			info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
			info.op_count = 1;
			info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op0_kind());
			return info;
		}
		auto info = InstrOpInfo::with_default(mnemonic_no_args_);
		info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
		return info;
	}
};

class SimpleInstrInfo_AX final : public StringInstrInfo {
public:
	SimpleInstrInfo_AX(std::string mnemonic_args, std::string mnemonic_no_args) : StringInstrInfo(std::move(mnemonic_args), std::move(mnemonic_no_args)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		OpKind short_form_op_kind;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			short_form_op_kind = instruction.op1_kind();
			break;
		case CodeSize::Code16:
			short_form_op_kind = OpKind::MemorySegSI;
			break;
		case CodeSize::Code32:
			short_form_op_kind = OpKind::MemorySegESI;
			break;
		case CodeSize::Code64:
		default:
			short_form_op_kind = OpKind::MemorySegRSI;
			break;
		}
		const bool short_form = instruction.op1_kind() == short_form_op_kind &&
								(instruction.segment_prefix() == Register::None || !show_segment_prefix(Register::None, instruction, options));
		if (!short_form) {
			auto info = InstrOpInfo::with_default(mnemonic_args_);
			info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
			info.op_count = 1;
			info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
			info.op_indexes[0] = 1;
			return info;
		}
		auto info = InstrOpInfo::with_default(mnemonic_no_args_);
		info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
		return info;
	}
};

class SimpleInstrInfo_AY final : public StringInstrInfo {
public:
	SimpleInstrInfo_AY(std::string mnemonic_args, std::string mnemonic_no_args) : StringInstrInfo(std::move(mnemonic_args), std::move(mnemonic_no_args)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		OpKind short_form_op_kind;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			short_form_op_kind = instruction.op1_kind();
			break;
		case CodeSize::Code16:
			short_form_op_kind = OpKind::MemoryESDI;
			break;
		case CodeSize::Code32:
			short_form_op_kind = OpKind::MemoryESEDI;
			break;
		case CodeSize::Code64:
		default:
			short_form_op_kind = OpKind::MemoryESRDI;
			break;
		}
		const bool short_form = instruction.op1_kind() == short_form_op_kind &&
								(instruction.segment_prefix() == Register::None || !show_segment_prefix(Register::None, instruction, options));
		if (!short_form) {
			auto info = InstrOpInfo::with_default(mnemonic_args_);
			info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
			info.op_count = 1;
			info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
			info.op_indexes[0] = 1;
			return info;
		}
		auto info = InstrOpInfo::with_default(mnemonic_no_args_);
		info.flags = static_cast<std::uint16_t>(FLAGS_STRING_SHORT_FORM);
		return info;
	}
};

class SimpleInstrInfo_XLAT final : public StringInstrInfo {
public:
	SimpleInstrInfo_XLAT(std::string mnemonic_args, std::string mnemonic_no_args)
		: StringInstrInfo(std::move(mnemonic_args), std::move(mnemonic_no_args)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
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
		const bool short_form = instruction.memory_base() == base_reg &&
								(instruction.segment_prefix() == Register::None || !show_segment_prefix(Register::None, instruction, options));
		if (!short_form)
			return InstrOpInfo::with_instruction(mnemonic_args_, instruction,
												 InstrOpInfoFlags::SHOW_NO_MEM_SIZE_FORCE_SIZE | InstrOpInfoFlags::IGNORE_INDEX_REG);
		return InstrOpInfo::with_default(mnemonic_no_args_);
	}
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

class SimpleInstrInfo_STi_ST final : public InstrInfo {
public:
	SimpleInstrInfo_STi_ST(std::string mnemonic, bool pseudo_op) : mnemonic_(std::move(mnemonic)), pseudo_op_(pseudo_op) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		constexpr std::uint32_t FLAGS = 0;
		if (pseudo_op_ && options.use_pseudo_ops() && (instruction.op0_register() == Register::ST1 || instruction.op1_register() == Register::ST1))
			return InstrOpInfo::with_default(mnemonic_);
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, FLAGS);
		ICED_DEBUG_ASSERT(info.op_registers[1] == Register::ST0);
		info.op_registers[1] = REGISTER_ST;
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
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, InstrOpInfoFlags::NONE);
		ICED_DEBUG_ASSERT(info.op_registers[0] == Register::ST0);
		info.op_registers[0] = REGISTER_ST;
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_monitor final : public InstrInfo {
public:
	SimpleInstrInfo_monitor(std::string mnemonic, Register register1, Register register2, Register register3)
		: mnemonic_(std::move(mnemonic)), register1_(register1), register2_(register2), register3_(register3) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		auto info = InstrOpInfo::with_default(mnemonic_);
		info.op_count = 3;
		info.op_kinds[0] = InstrOpKind::Register;
		info.op_registers[0] = register1_;
		info.op_kinds[1] = InstrOpKind::Register;
		info.op_registers[1] = register2_;
		info.op_kinds[2] = InstrOpKind::Register;
		info.op_registers[2] = register3_;
		info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[2] = InstrInfoConstants::OP_ACCESS_READ;
		if ((instruction.code_size() == CodeSize::Code64 || instruction.code_size() == CodeSize::Unknown) &&
			(Register::EAX <= register2_ && register2_ <= Register::R15D)) {
			info.op_registers[1] = static_cast<Register>(static_cast<std::uint32_t>(info.op_registers[1]) + 0x10);
			info.op_registers[2] = static_cast<Register>(static_cast<std::uint32_t>(info.op_registers[2]) + 0x10);
		}
		return info;
	}

private:
	FormatterString mnemonic_;
	Register register1_;
	Register register2_;
	Register register3_;
};

class SimpleInstrInfo_mwait final : public InstrInfo {
public:
	explicit SimpleInstrInfo_mwait(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		auto info = InstrOpInfo::with_default(mnemonic_);
		info.op_count = 2;
		info.op_kinds[0] = InstrOpKind::Register;
		info.op_kinds[1] = InstrOpKind::Register;
		info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_READ;

		switch (instruction.code_size()) {
		case CodeSize::Code16:
			info.op_registers[0] = Register::AX;
			info.op_registers[1] = Register::ECX;
			break;
		case CodeSize::Code32:
			info.op_registers[0] = Register::EAX;
			info.op_registers[1] = Register::ECX;
			break;
		case CodeSize::Unknown:
		case CodeSize::Code64:
		default:
			info.op_registers[0] = Register::RAX;
			info.op_registers[1] = Register::RCX;
			break;
		}
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_mwaitx final : public InstrInfo {
public:
	explicit SimpleInstrInfo_mwaitx(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		auto info = InstrOpInfo::with_default(mnemonic_);
		info.op_count = 3;
		info.op_kinds[0] = InstrOpKind::Register;
		info.op_kinds[1] = InstrOpKind::Register;
		info.op_kinds[2] = InstrOpKind::Register;
		info.op_indexes[0] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[1] = InstrInfoConstants::OP_ACCESS_READ;
		info.op_indexes[2] = InstrInfoConstants::OP_ACCESS_COND_READ;

		switch (instruction.code_size()) {
		case CodeSize::Code16:
			info.op_registers[0] = Register::AX;
			info.op_registers[1] = Register::ECX;
			info.op_registers[2] = Register::EBX;
			break;
		case CodeSize::Code32:
			info.op_registers[0] = Register::EAX;
			info.op_registers[1] = Register::ECX;
			info.op_registers[2] = Register::EBX;
			break;
		case CodeSize::Unknown:
		case CodeSize::Code64:
		default:
			info.op_registers[0] = Register::RAX;
			info.op_registers[1] = Register::RCX;
			info.op_registers[2] = Register::RBX;
			break;
		}
		return info;
	}

private:
	FormatterString mnemonic_;
};

class SimpleInstrInfo_maskmovq final : public InstrInfo {
public:
	SimpleInstrInfo_maskmovq(std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		ICED_DEBUG_ASSERT(instruction.op_count() == 3);
		OpKind short_form_op_kind;
		switch (instruction.code_size()) {
		case CodeSize::Unknown:
			short_form_op_kind = instruction.op0_kind();
			break;
		case CodeSize::Code16:
			short_form_op_kind = OpKind::MemorySegDI;
			break;
		case CodeSize::Code32:
			short_form_op_kind = OpKind::MemorySegEDI;
			break;
		case CodeSize::Code64:
		default:
			short_form_op_kind = OpKind::MemorySegRDI;
			break;
		}
		const bool short_form = instruction.op0_kind() == short_form_op_kind &&
								(instruction.segment_prefix() == Register::None || !show_segment_prefix(Register::None, instruction, options));
		if (!short_form)
			return InstrOpInfo::with_instruction(mnemonic_, instruction, flags_);
		auto info = InstrOpInfo::with_default(mnemonic_);
		info.flags = static_cast<std::uint16_t>(flags_);
		info.op_count = 2;
		info.op_kinds[0] = InstrOpInfo::to_instr_op_kind(instruction.op1_kind());
		info.op_indexes[0] = 1;
		info.op_registers[0] = instruction.op1_register();
		info.op_kinds[1] = InstrOpInfo::to_instr_op_kind(instruction.op2_kind());
		info.op_indexes[1] = 2;
		info.op_registers[1] = instruction.op2_register();
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_pblendvb final : public InstrInfo {
public:
	explicit SimpleInstrInfo_pblendvb(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

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
		info.op_kinds[2] = InstrOpKind::Register;
		info.op_registers[2] = Register::XMM0;
		info.op_indexes[2] = InstrInfoConstants::OP_ACCESS_READ;
		return info;
	}

private:
	FormatterString mnemonic_;
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

class SimpleInstrInfo_OpSize_cc final : public InstrInfo {
public:
	SimpleInstrInfo_OpSize_cc(CodeSize code_size, std::uint32_t cc_index, std::vector<std::string>&& mnemonics, std::vector<std::string>&& mnemonics_other)
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics)))
		, mnemonics_other_(FormatterString::with_strings(std::move(mnemonics_other)))
		, cc_index_(cc_index)
		, code_size_(code_size) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		const auto& mnemonics = instruction.code_size() == code_size_ ? mnemonics_ : mnemonics_other_;
		const auto& mnemonic = get_mnemonic_cc(options, cc_index_, mnemonics);
		return InstrOpInfo::with_instruction(mnemonic, instruction, InstrOpInfoFlags::NONE);
	}

private:
	std::vector<FormatterString> mnemonics_;
	std::vector<FormatterString> mnemonics_other_;
	std::uint32_t cc_index_;
	CodeSize code_size_;
};

class SimpleInstrInfo_OpSize2 final : public InstrInfo {
public:
	SimpleInstrInfo_OpSize2(std::string mnemonic, std::string mnemonic16, std::string mnemonic32, std::string mnemonic64, bool can_use_bnd)
		: mnemonics_{FormatterString(std::move(mnemonic)), FormatterString(std::move(mnemonic16)), FormatterString(std::move(mnemonic32)),
					 FormatterString(std::move(mnemonic64))}
		, can_use_bnd_(can_use_bnd) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		std::uint32_t flags = InstrOpInfoFlags::NONE;
		if (can_use_bnd_ && instruction.has_repne_prefix())
			flags |= InstrOpInfoFlags::BND_PREFIX;
		const auto& mnemonic = mnemonics_[static_cast<std::size_t>(instruction.code_size())];
		return InstrOpInfo::with_instruction(mnemonic, instruction, flags);
	}

private:
	std::array<FormatterString, 4> mnemonics_;
	bool can_use_bnd_;
};

class SimpleInstrInfo_fword final : public InstrInfo {
public:
	SimpleInstrInfo_fword(CodeSize code_size, std::uint32_t flags, std::string mnemonic, std::string mnemonic2)
		: mnemonic_(std::move(mnemonic)), mnemonic2_(std::move(mnemonic2)), code_size_(code_size), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		const auto& mnemonic =
			instruction.code_size() == code_size_ || instruction.code_size() == CodeSize::Unknown ? mnemonic_ : mnemonic2_;
		return InstrOpInfo::with_instruction(mnemonic, instruction, flags_);
	}

private:
	FormatterString mnemonic_;
	FormatterString mnemonic2_;
	CodeSize code_size_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_jcc final : public InstrInfo {
public:
	SimpleInstrInfo_jcc(std::uint32_t cc_index, std::vector<std::string>&& mnemonics)
		: mnemonics_(FormatterString::with_strings(std::move(mnemonics))), cc_index_(cc_index) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		std::uint32_t flags = InstrOpInfoFlags::NONE;
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
	std::uint32_t cc_index_;
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
	SimpleInstrInfo_pops(std::string mnemonic, PseudoOps pseudo_ops) : mnemonic_(std::move(mnemonic)), pseudo_ops_(pseudo_ops), flags_(InstrOpInfoFlags::NONE) {}
	SimpleInstrInfo_pops(std::string mnemonic, PseudoOps pseudo_ops, std::uint32_t flags)
		: mnemonic_(std::move(mnemonic)), pseudo_ops_(pseudo_ops), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, flags_);
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
	std::uint32_t flags_;
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

class SimpleInstrInfo_imul final : public InstrInfo {
public:
	explicit SimpleInstrInfo_imul(std::string mnemonic) : mnemonic_(std::move(mnemonic)) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, InstrOpInfoFlags::NONE);
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
	SimpleInstrInfo_Reg16(std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, flags_);
		info.op_registers[0] = r_to_r16(info.op_registers[0]);
		info.op_registers[1] = r_to_r16(info.op_registers[1]);
		info.op_registers[2] = r_to_r16(info.op_registers[2]);
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_Reg32 final : public InstrInfo {
public:
	SimpleInstrInfo_Reg32(std::string mnemonic, std::uint32_t flags) : mnemonic_(std::move(mnemonic)), flags_(flags) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		auto info = InstrOpInfo::with_instruction(mnemonic_, instruction, flags_);
		info.op_registers[0] = r64_to_r32(info.op_registers[0]);
		info.op_registers[1] = r64_to_r32(info.op_registers[1]);
		info.op_registers[2] = r64_to_r32(info.op_registers[2]);
		return info;
	}

private:
	FormatterString mnemonic_;
	std::uint32_t flags_;
};

class SimpleInstrInfo_reg final : public InstrInfo {
public:
	SimpleInstrInfo_reg(std::string mnemonic, Register register_) : mnemonic_(std::move(mnemonic)), register_(register_) {}

	InstrOpInfo op_info(const FormatterOptions& options, const Instruction& instruction) const noexcept override {
		static_cast<void>(options);
		static_cast<void>(instruction);
		auto info = InstrOpInfo::with_default(mnemonic_);
		info.op_count = 1;
		info.op_kinds[0] = InstrOpKind::Register;
		info.op_registers[0] = register_;
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
				const std::string_view s0 = strings[reader.read_compressed_u32()];
				s.reserve(s0.size() + 1);
				s += 'v';
				s += s0;
			}
			else
				read_string(s);

			char c;
			std::uint32_t v;
			std::uint32_t v2;
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

			case CtorKind::AX: {
				c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				info = add<SimpleInstrInfo_AX>(std::move(s), std::move(s2));
				break;
			}

			case CtorKind::AY: {
				c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				info = add<SimpleInstrInfo_AY>(std::move(s), std::move(s2));
				break;
			}

			case CtorKind::bnd:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_bnd>(std::move(s), v);
				break;

			case CtorKind::DeclareData:
				info = add<SimpleInstrInfo_DeclareData>(static_cast<Code>(i), std::move(s));
				break;

			case CtorKind::DX: {
				c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				info = add<SimpleInstrInfo_DX>(std::move(s), std::move(s2));
				break;
			}

			case CtorKind::fword: {
				c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				const CodeSize code_size = read_code_size();
				v2 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_fword>(code_size, v2, std::move(s), std::move(s2));
				break;
			}

			case CtorKind::Int3:
				info = add<SimpleInstrInfo_Int3>(std::move(s));
				break;

			case CtorKind::imul:
				info = add<SimpleInstrInfo_imul>(std::move(s));
				break;

			case CtorKind::invlpga:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_invlpga>(v, std::move(s));
				break;

			case CtorKind::CCa_1:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v, make_vec(mnemonics, std::move(s)));
				break;

			case CtorKind::CCa_2: {
				read_string(s2);
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v, make_vec(mnemonics, std::move(s), std::move(s2)));
				break;
			}

			case CtorKind::CCa_3: {
				read_string(s2);
				read_string(s3);
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v, make_vec(mnemonics, std::move(s), std::move(s2), std::move(s3)));
				break;
			}

			case CtorKind::CCb_1:
				v2 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v2, make_vec(mnemonics, std::move(s)), v);
				break;

			case CtorKind::CCb_2: {
				read_string(s2);
				v2 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v2, make_vec(mnemonics, std::move(s), std::move(s2)), v);
				break;
			}

			case CtorKind::CCb_3: {
				read_string(s2);
				read_string(s3);
				v2 = reader.read_compressed_u32();
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v2, make_vec(mnemonics, std::move(s), std::move(s2), std::move(s3)), v);
				break;
			}

			case CtorKind::jcc_1:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_jcc>(v, make_vec(mnemonics, std::move(s)));
				break;

			case CtorKind::jcc_2: {
				read_string(s2);
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_jcc>(v, make_vec(mnemonics, std::move(s), std::move(s2)));
				break;
			}

			case CtorKind::jcc_3: {
				read_string(s2);
				read_string(s3);
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_jcc>(v, make_vec(mnemonics, std::move(s), std::move(s2), std::move(s3)));
				break;
			}

			case CtorKind::Loopcc1: {
				read_string(s2);
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_cc>(v, make_vec(mnemonics, std::move(s), std::move(s2)));
				break;
			}

			case CtorKind::Loopcc2: {
				read_string(s2);
				c = static_cast<char>(reader.read_u8());
				v2 = reader.read_compressed_u32();
				add_suffix(s3, s, c);
				add_suffix(s4, s2, c);
				const CodeSize code_size = read_code_size();
				info = add<SimpleInstrInfo_OpSize_cc>(code_size, v2, make_vec(mnemonics, std::move(s), std::move(s2)),
																   make_vec(mnemonics_other, std::move(s3), std::move(s4)));
				break;
			}

			case CtorKind::maskmovq:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_maskmovq>(std::move(s), v);
				break;

			case CtorKind::memsize:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_memsize>(v, std::move(s));
				break;

			case CtorKind::monitor: {
				const Register r1 = read_register();
				const Register r2 = read_register();
				const Register r3 = read_register();
				info = add<SimpleInstrInfo_monitor>(std::move(s), r1, r2, r3);
				break;
			}

			case CtorKind::mwait:
				info = add<SimpleInstrInfo_mwait>(std::move(s));
				break;

			case CtorKind::mwaitx:
				info = add<SimpleInstrInfo_mwaitx>(std::move(s));
				break;

			case CtorKind::nop: {
				v = reader.read_compressed_u32();
				const Register r = read_register();
				info = add<SimpleInstrInfo_nop>(v, std::move(s), r);
				break;
			}

			case CtorKind::OpSize_1: {
				const CodeSize code_size = read_code_size();
				add_suffix(s2, s, 'w');
				add_suffix(s3, s, 'd');
				add_suffix(s4, s, 'q');
				info = add<SimpleInstrInfo_OpSize>(code_size, std::move(s), std::move(s2), std::move(s3), std::move(s4));
				break;
			}

			case CtorKind::OpSize_2: {
				c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				const CodeSize code_size = read_code_size();
				info = add<SimpleInstrInfo_OpSize>(code_size, std::move(s), s2, s2, s2);
				break;
			}

			case CtorKind::OpSize2: {
				read_string(s2);
				read_string(s3);
				read_string(s4);
				v = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v <= 1);
				info = add<SimpleInstrInfo_OpSize2>(std::move(s), std::move(s2), std::move(s3), std::move(s4), v != 0);
				break;
			}

			case CtorKind::pblendvb:
				info = add<SimpleInstrInfo_pblendvb>(std::move(s));
				break;

			case CtorKind::pclmulqdq:
				info = add<SimpleInstrInfo_pclmulqdq>(std::move(s), read_pseudo_ops());
				break;

			case CtorKind::pops_2:
				info = add<SimpleInstrInfo_pops>(std::move(s), read_pseudo_ops());
				break;

			case CtorKind::pops_3: {
				const PseudoOps pseudo_ops = read_pseudo_ops();
				v2 = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_pops>(std::move(s), pseudo_ops, v2);
				break;
			}

			case CtorKind::reg:
				info = add<SimpleInstrInfo_reg>(std::move(s), read_register());
				break;

			case CtorKind::Reg16:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_Reg16>(std::move(s), v);
				break;

			case CtorKind::Reg32:
				v = reader.read_compressed_u32();
				info = add<SimpleInstrInfo_Reg32>(std::move(s), v);
				break;

			case CtorKind::reverse:
				info = add<SimpleInstrInfo_reverse>(std::move(s));
				break;

			case CtorKind::ST_STi:
				info = add<SimpleInstrInfo_ST_STi>(std::move(s));
				break;

			case CtorKind::STi_ST:
				v = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v <= 1);
				info = add<SimpleInstrInfo_STi_ST>(std::move(s), v != 0);
				break;

			case CtorKind::STIG1:
				v = static_cast<std::uint32_t>(reader.read_u8());
				ICED_DEBUG_ASSERT(v <= 1);
				info = add<SimpleInstrInfo_STIG1>(std::move(s), v != 0);
				break;

			case CtorKind::XLAT: {
				add_suffix(s2, s, 'b');
				info = add<SimpleInstrInfo_XLAT>(std::move(s), std::move(s2));
				break;
			}

			case CtorKind::XY: {
				c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				info = add<SimpleInstrInfo_XY>(std::move(s), std::move(s2));
				break;
			}

			case CtorKind::YA: {
				c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				info = add<SimpleInstrInfo_YA>(std::move(s), std::move(s2));
				break;
			}

			case CtorKind::YD: {
				c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				info = add<SimpleInstrInfo_YD>(std::move(s), std::move(s2));
				break;
			}

			case CtorKind::YX: {
				c = static_cast<char>(reader.read_u8());
				add_suffix(s2, s, c);
				info = add<SimpleInstrInfo_YX>(std::move(s), std::move(s2));
				break;
			}

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

} // namespace iced_x86::internal::masm
