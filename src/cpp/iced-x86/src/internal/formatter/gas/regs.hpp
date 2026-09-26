// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/regs.rs

#pragma once

#include "internal/formatter/regs_tbl_ls.hpp"

namespace iced_x86::internal::gas {

/// Gets all register names with a `%` prefix (Rust: `ALL_REGISTERS`). It's created the first time it's called.
const RegsTbl& get_all_registers();

} // namespace iced_x86::internal::gas
