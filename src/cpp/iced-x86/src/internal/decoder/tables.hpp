// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// The decoder tables (Rust: decoder/handlers/tables.rs)

#pragma once

#include "internal/decoder/handlers.hpp"

namespace iced_x86::internal {

// Each table has 0x100 handlers
struct DecoderTables {
	const OpCodeHandler* const* invalid_map;
	const OpCodeHandler* const* handlers_map0;
	const OpCodeHandler* const* handlers_vex_map0;
	const OpCodeHandler* const* handlers_vex_0f;
	const OpCodeHandler* const* handlers_vex_0f38;
	const OpCodeHandler* const* handlers_vex_0f3a;
	const OpCodeHandler* const* handlers_evex_0f;
	const OpCodeHandler* const* handlers_evex_0f38;
	const OpCodeHandler* const* handlers_evex_0f3a;
	const OpCodeHandler* const* handlers_evex_map5;
	const OpCodeHandler* const* handlers_evex_map6;
	const OpCodeHandler* const* handlers_xop_map8;
	const OpCodeHandler* const* handlers_xop_map9;
	const OpCodeHandler* const* handlers_xop_map10;
	const OpCodeHandler* const* handlers_mvex_0f;
	const OpCodeHandler* const* handlers_mvex_0f38;
	const OpCodeHandler* const* handlers_mvex_0f3a;
};

// Creates the tables the first time it's called (thread safe)
const DecoderTables& get_decoder_tables() noexcept;

} // namespace iced_x86::internal
