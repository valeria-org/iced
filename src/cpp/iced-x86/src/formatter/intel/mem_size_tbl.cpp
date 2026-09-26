// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/intel/mem_size_tbl.rs

#include "internal/formatter/intel/mem_size_tbl.hpp"

#include <cstddef>

#include "internal/formatter/fmt_consts.hpp"
#include "internal/formatter/intel/mem_size_tbl_data.hpp"
#include "internal/iced_assert.hpp"

namespace iced_x86::internal::intel {

static_assert(sizeof(MEM_SIZE_TBL_DATA) / sizeof(MEM_SIZE_TBL_DATA[0]) == IcedConstants::MEMORY_SIZE_ENUM_COUNT, "");

namespace {
struct MemSizeTblHolder {
	MemSizeTbl tbl;

	MemSizeTblHolder() {
		const FormatterConstants& c = get_formatter_constants();
		const FormatterArrayConstants& ac = get_array_constants();
		std::size_t i = 0;
		for (const auto d : MEM_SIZE_TBL_DATA) {
			const FormatterStringSlice keywords = get_memory_keywords(ac, d & MEMORY_KEYWORDS_MASK);
			const FormatterString& bcst_to = get_bcst_to_string(c, static_cast<std::uint32_t>(d) >> BROADCAST_TO_KIND_SHIFT);
			tbl[i++] = MemSizeInfo{&bcst_to, keywords};
		}
		ICED_ASSERT(i == tbl.size());
	}
};
} // namespace

const MemSizeTbl& get_mem_size_tbl() {
	static const MemSizeTblHolder holder;
	return holder.tbl;
}

} // namespace iced_x86::internal::intel
