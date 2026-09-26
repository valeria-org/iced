// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "internal/formatter/strings_tbl.hpp"

#include <cstddef>
#include <cstdint>

#include "internal/formatter/strings_data.hpp"
#include "internal/iced_assert.hpp"

namespace iced_x86::internal {

std::vector<std::string_view> get_strings_table_ref() {
	using namespace strings_data;
	std::vector<std::string_view> strings;
	strings.reserve(STRINGS_COUNT);
	std::size_t index = 0;
	for (std::size_t i = 0; i < STRINGS_COUNT; i++) {
		ICED_ASSERT(index < STRINGS_TBL_DATA_SIZE);
		const std::size_t len = STRINGS_TBL_DATA[index];
		index++;
		ICED_ASSERT(index + len <= STRINGS_TBL_DATA_SIZE);
		strings.emplace_back(reinterpret_cast<const char*>(STRINGS_TBL_DATA + index), len);
		index += len;
	}
	ICED_DEBUG_ASSERT(STRINGS_TBL_DATA_SIZE - index == PADDING_SIZE);

	return strings;
}

} // namespace iced_x86::internal
