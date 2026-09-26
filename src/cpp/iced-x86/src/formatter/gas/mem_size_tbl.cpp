// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/mem_size_tbl.rs

#include "internal/formatter/gas/mem_size_tbl.hpp"

#include <cstddef>

#include "internal/formatter/fmt_consts.hpp"
#include "internal/formatter/gas/mem_size_tbl_data.hpp"
#include "internal/iced_assert.hpp"

namespace iced_x86::internal::gas {

static_assert(static_cast<std::size_t>(IcedConstants::FIRST_BROADCAST_MEMORY_SIZE) + sizeof(BCST_TO_DATA) / sizeof(BCST_TO_DATA[0]) == IcedConstants::MEMORY_SIZE_ENUM_COUNT, "");

namespace {
struct MemSizeTblHolder {
	MemSizeTbl tbl;

	MemSizeTblHolder() {
		const FormatterConstants& c = get_formatter_constants();
		std::size_t i = 0;
		for (; i < static_cast<std::size_t>(IcedConstants::FIRST_BROADCAST_MEMORY_SIZE); i++)
			tbl[i] = &c.empty;
		for (const auto d : BCST_TO_DATA)
			tbl[i++] = &get_bcst_to_string(c, d);
		ICED_ASSERT(i == tbl.size());
	}
};
} // namespace

const MemSizeTbl& get_mem_size_tbl() {
	static const MemSizeTblHolder holder;
	return holder.tbl;
}

} // namespace iced_x86::internal::gas
