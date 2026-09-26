// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// FPU op code handlers (Rust: decoder/handlers/fpu.rs)

#pragma once

#include "internal/decoder/handlers.hpp"

namespace iced_x86::internal {

struct OpCodeHandler_ST_STi : OpCodeHandler {
	Code code;
	explicit OpCodeHandler_ST_STi(Code code_) noexcept : OpCodeHandler(&decode, true), code(code_) {}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_STi_ST : OpCodeHandler {
	Code code;
	explicit OpCodeHandler_STi_ST(Code code_) noexcept : OpCodeHandler(&decode, true), code(code_) {}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_STi : OpCodeHandler {
	Code code;
	explicit OpCodeHandler_STi(Code code_) noexcept : OpCodeHandler(&decode, true), code(code_) {}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_Mf : OpCodeHandler {
	Code code16;
	Code code32;
	// Rust: new(code)
	explicit OpCodeHandler_Mf(Code code) noexcept : OpCodeHandler(&decode, true), code16(code), code32(code) {}
	// Rust: new1(code16, code32)
	OpCodeHandler_Mf(Code code16_, Code code32_) noexcept : OpCodeHandler(&decode, true), code16(code16_), code32(code32_) {}
	ICED_DECODE_FN_DECL;
};

} // namespace iced_x86::internal
