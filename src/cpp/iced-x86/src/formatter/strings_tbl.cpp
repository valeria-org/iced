// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "internal/formatter/strings_tbl.hpp"

#include <cstddef>
#include <cstdint>

#include "internal/data_reader.hpp"
#include "internal/formatter/strings_data.hpp"
#include "internal/iced_assert.hpp"

namespace iced_x86::internal {

std::vector<std::string_view> get_strings_table_ref() {
	using namespace strings_data;
	DataReader reader(STRINGS_TBL_DATA, STRINGS_TBL_DATA_SIZE);
	std::vector<std::string_view> strings;
	strings.reserve(STRINGS_COUNT);
	for (std::size_t i = 0; i < STRINGS_COUNT; i++)
		strings.push_back(reader.read_ascii_str());
	ICED_DEBUG_ASSERT(reader.len_left() == PADDING_SIZE);

	return strings;
}

} // namespace iced_x86::internal
