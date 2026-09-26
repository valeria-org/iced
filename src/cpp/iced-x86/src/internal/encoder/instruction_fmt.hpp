// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

#include "iced_x86/op_code_info.hpp"
#include "internal/encoder/instr_str_fmt_option.hpp"
#include <cstdint>
#include <string>

namespace iced_x86::internal {

// Creates the instruction string, see `OpCodeInfo::instruction_string()`
class InstructionFormatter {
public:
	InstructionFormatter(const OpCodeInfo& op_code, InstrStrFmtOption fmt_opt, std::string& sb) noexcept;

	std::string format();

private:
	std::uint32_t get_k_index() noexcept;
	std::uint32_t get_bnd_index() noexcept;
	std::uint32_t get_vec_index(std::uint32_t op_index) noexcept;
	std::uint32_t get_tmm_index() noexcept;
	MemorySize get_memory_size(bool is_broadcast) const noexcept;
	void write_memory_size(MemorySize memory_size);
	bool is_sgdt_or_sidt() const noexcept;
	void write_register(const char* register_);
	void write_reg_op1(const char* register_);
	void write_reg_op2(const char* register_, std::uint32_t index);
	void write_decorator(const char* decorator);
	void write_reg_decorator(const char* register_, std::uint32_t index);
	void append_gpr_suffix(std::uint32_t count, std::uint32_t& index);
	void write_op_separator();
	void write(const char* s, bool upper);
	void write_gpr_mem(std::uint32_t reg_size);
	void write_reg_mem(const char* register_, std::uint32_t index);
	void write_memory();
	void write_memory1(bool is_broadcast);
	static bool is_fpu_instruction(Code code) noexcept;

	const OpCodeInfo& op_code;
	std::string& sb;
	std::uint32_t r32_count;
	std::uint32_t r64_count;
	std::uint32_t bnd_count;
	std::uint32_t start_op_index;
	std::uint32_t r32_index;
	std::uint32_t r64_index;
	std::uint32_t bnd_index;
	std::uint32_t k_index;
	std::uint32_t vec_index;
	std::uint32_t tmm_index;
	std::uint32_t op_count;
	// true: k2 {k1}, false: k1 {k2}
	bool op_mask_is_k1;
	bool no_vec_index;
	bool swap_vec_index_12;
	bool no_gpr_suffix;
	bool vec_index_same_as_op_index;
};

} // namespace iced_x86::internal
