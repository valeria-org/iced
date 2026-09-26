// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Pseudo op mnemonics used by the gas/intel/masm/nasm formatters (Rust: formatter/pseudo_ops.rs)
// The pseudo ops are defined in pseudo_ops_defs.hpp (shared with the fast formatter)

#pragma once

#include <vector>

#include "internal/formatter/formatter_string.hpp"
#include "internal/formatter/pseudo_ops_kind.hpp"

namespace iced_x86::internal {

/// Gets the pseudo op mnemonics (index = imm8 value) of a `PseudoOpsKind` (created the first time it's called)
const std::vector<FormatterString>& get_pseudo_ops(PseudoOpsKind kind);

} // namespace iced_x86::internal
