// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/nasm/regs.rs

#include "internal/formatter/nasm/regs.hpp"

#include <cstddef>
#include <string>

#include "iced_x86/register.hpp"
#include "internal/formatter/regs_tbl_ls.hpp"

namespace iced_x86::internal::nasm {

namespace {
struct AllRegistersHolder {
	RegsTbl regs;

	AllRegistersHolder() : regs(get_regs_tbl()) {
		for (std::size_t i = 0; i < 8; i++) {
			std::string s = "st";
			s += static_cast<char>('0' + i);
			regs[static_cast<std::size_t>(Register::ST0) + i] = FormatterString(std::move(s));
		}
	}
};
} // namespace

const FormatterString* get_all_registers() {
	static const AllRegistersHolder holder;
	return holder.regs.data();
}

} // namespace iced_x86::internal::nasm
