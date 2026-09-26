// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

#include "iced_x86/op_code_info.hpp"
#include "internal/encoder/l_kind.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace iced_x86::internal {

// Creates the op code string, see `OpCodeInfo::op_code_string()`
class OpCodeFormatter {
public:
	OpCodeFormatter(const OpCodeInfo& op_code, std::string& sb, LKind lkind, bool has_modrm_info) noexcept
		: op_code(op_code), sb(sb), lkind(lkind), has_modrm_info(has_modrm_info) {}

	std::string format();

private:
	struct ModrmInfo {
		bool is_reg_only;
		std::int32_t rrr;
		std::int32_t bbb;
	};

	void append_hex_byte(std::uint8_t value);
	void append_op_code(std::uint32_t value, std::uint32_t value_len, bool sep);
	void append_table(bool sep);
	bool has_mod_rm() const noexcept;
	bool has_vsib() const noexcept;
	OpCodeOperandKind get_op_code_bits_operand() const noexcept;
	std::optional<ModrmInfo> get_modrm_info() const noexcept;
	void append_bits(const char* name, std::int32_t bits, std::uint32_t num_bits);
	void append_rest();
	std::string format_legacy();
	std::string format_3dnow();
	std::string format_vec_encoding(const char* encoding_name);

	const OpCodeInfo& op_code;
	std::string& sb;
	LKind lkind;
	bool has_modrm_info;
};

} // namespace iced_x86::internal
