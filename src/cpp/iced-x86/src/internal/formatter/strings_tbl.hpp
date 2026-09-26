// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// The strings table used by the gas/intel/masm/nasm formatters (Rust: formatter/strings_tbl.rs)

#pragma once

#include <string_view>
#include <vector>

namespace iced_x86::internal {

/// Gets all strings in the strings table (see `strings_data.hpp`). The strings point to static data.
///
/// The returned vector isn't cached since only one formatter is normally used
std::vector<std::string_view> get_strings_table_ref();

} // namespace iced_x86::internal
