// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Nasm formatter register names (Rust: formatter/nasm/regs.rs)

#pragma once

#include "internal/formatter/formatter_string.hpp"

namespace iced_x86::internal::nasm {

/// Gets the register names (index = `Register` value). Same as `get_regs_tbl()` except `st0`-`st7` are used instead of
/// `st(0)`-`st(7)`. The table is created the first time it's called.
const FormatterString* get_all_registers();

} // namespace iced_x86::internal::nasm
