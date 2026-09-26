// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/regs.rs

#include "internal/formatter/gas/regs.hpp"

#include <cstddef>
#include <string>

#include "internal/formatter/regs_tbl.hpp"

namespace iced_x86::internal::gas {

namespace {
struct AllRegistersHolder {
	RegsTbl regs;

	AllRegistersHolder() {
		const RegsTbl& regs_tbl = get_regs_tbl();
		std::string s;
		s.reserve(regs_tbl::MAX_STRING_LENGTH + 1);
		for (std::size_t i = 0; i < regs.size(); i++) {
			s.push_back('%');
			s.append(regs_tbl[i].lower());
			regs[i] = FormatterString(s);
			s.clear();
		}
	}
};
} // namespace

const RegsTbl& get_all_registers() {
	static const AllRegistersHolder holder;
	return holder.regs;
}

} // namespace iced_x86::internal::gas
