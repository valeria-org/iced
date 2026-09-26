// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "internal/formatter/regs_tbl_ls.hpp"

#include <cstddef>
#include <string_view>

#include "internal/formatter/regs_tbl.hpp"
#include "internal/iced_assert.hpp"

namespace iced_x86::internal {

namespace {
struct RegsTblHolder {
	RegsTbl regs;

	RegsTblHolder() {
		using namespace regs_tbl;
		std::size_t index = 0;
		for (auto& reg : regs) {
			ICED_ASSERT(index < REGS_DATA_SIZE);
			const std::size_t len = REGS_DATA[index];
			index++;
			ICED_ASSERT(index + len <= REGS_DATA_SIZE);
			reg = FormatterString(std::string_view(reinterpret_cast<const char*>(REGS_DATA + index), len));
			index += len;
		}
		ICED_DEBUG_ASSERT(REGS_DATA_SIZE - index == PADDING_SIZE);
	}
};
} // namespace

const RegsTbl& get_regs_tbl() {
	static const RegsTblHolder holder;
	return holder.regs;
}

} // namespace iced_x86::internal
