// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Generic op code handlers (Rust: decoder/handlers.rs)

#pragma once

#include "internal/decoder/decoder_core.hpp"

#include <cstddef>
#include <vector>

namespace iced_x86::internal {

// Handlers are created once (when the tables are created) and never freed, see TableDeserializer.
// Arrays of handlers passed to the ctors (Rust: `Vec<(OpCodeHandlerDecodeFn, &'static OpCodeHandler)>`)
using HandlerVec = std::vector<const OpCodeHandler*>;

#define ICED_DECODE_FN_DECL static void decode(const OpCodeHandler* self_ptr, DecoderCore& decoder, Instruction& instruction)

struct OpCodeHandler_Invalid : OpCodeHandler {
	explicit constexpr OpCodeHandler_Invalid(bool has_modrm_) noexcept : OpCodeHandler(&decode, has_modrm_) {}
	ICED_DECODE_FN_DECL;
};

extern const OpCodeHandler_Invalid NULL_HANDLER;
extern const OpCodeHandler_Invalid INVALID_HANDLER;
extern const OpCodeHandler_Invalid INVALID_NO_MODRM_HANDLER;

inline bool is_null_instance_handler(const OpCodeHandler* handler) noexcept { return handler == &NULL_HANDLER; }
inline const OpCodeHandler* get_null_handler() noexcept { return &NULL_HANDLER; }
inline const OpCodeHandler* get_invalid_handler() noexcept { return &INVALID_HANDLER; }
inline const OpCodeHandler* get_invalid_no_modrm_handler() noexcept { return &INVALID_NO_MODRM_HANDLER; }

// Copies `N` handlers from `handlers` to `dest`. Aborts if the vector doesn't have exactly `N` elements (Rust: `try_into().unwrap()`)
template <std::size_t N>
inline void copy_handlers(const OpCodeHandler* (&dest)[N], const HandlerVec& handlers) noexcept {
	ICED_ASSERT(handlers.size() == N);
	for (std::size_t i = 0; i < N; i++)
		dest[i] = handlers[i];
}

struct OpCodeHandler_Simple : OpCodeHandler {
	Code code;
	OpCodeHandler_Simple(bool has_modrm_, Code code_) noexcept : OpCodeHandler(&decode, has_modrm_), code(code_) {}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_Int3 : OpCodeHandler {
	OpCodeHandler_Int3() noexcept : OpCodeHandler(&decode, false) {}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_Group8x8 : OpCodeHandler {
	const OpCodeHandler* table_low[8];
	const OpCodeHandler* table_high[8];
	OpCodeHandler_Group8x8(const HandlerVec& table_low_, const HandlerVec& table_high_) noexcept : OpCodeHandler(&decode, true) {
		copy_handlers(table_low, table_low_);
		copy_handlers(table_high, table_high_);
	}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_Group8x64 : OpCodeHandler {
	const OpCodeHandler* table_low[8];
	const OpCodeHandler* table_high[0x40];
	OpCodeHandler_Group8x64(const HandlerVec& table_low_, const HandlerVec& table_high_) noexcept : OpCodeHandler(&decode, true) {
		copy_handlers(table_low, table_low_);
		copy_handlers(table_high, table_high_);
	}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_Group : OpCodeHandler {
	const OpCodeHandler* group_handlers[8];
	explicit OpCodeHandler_Group(const HandlerVec& group_handlers_) noexcept : OpCodeHandler(&decode, true) {
		copy_handlers(group_handlers, group_handlers_);
	}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_AnotherTable : OpCodeHandler {
	const OpCodeHandler* handlers[0x100];
	explicit OpCodeHandler_AnotherTable(const HandlerVec& handlers_) noexcept : OpCodeHandler(&decode, false) { copy_handlers(handlers, handlers_); }
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_MandatoryPrefix2 : OpCodeHandler {
	const OpCodeHandler* handlers[4];
	OpCodeHandler_MandatoryPrefix2(bool has_modrm_, const OpCodeHandler* handler, const OpCodeHandler* handler_66, const OpCodeHandler* handler_f3,
								   const OpCodeHandler* handler_f2) noexcept
		: OpCodeHandler(&decode, has_modrm_), handlers{handler, handler_66, handler_f3, handler_f2} {
		static_assert(static_cast<std::uint32_t>(DecoderMandatoryPrefix::PNP) == 0, "");
		static_assert(static_cast<std::uint32_t>(DecoderMandatoryPrefix::P66) == 1, "");
		static_assert(static_cast<std::uint32_t>(DecoderMandatoryPrefix::PF3) == 2, "");
		static_assert(static_cast<std::uint32_t>(DecoderMandatoryPrefix::PF2) == 3, "");
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler_66));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler_f3));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler_f2));
		ICED_DEBUG_ASSERT(handlers[0]->has_modrm == has_modrm_);
		ICED_DEBUG_ASSERT(handlers[1]->has_modrm == has_modrm_);
		ICED_DEBUG_ASSERT(handlers[2]->has_modrm == has_modrm_);
		ICED_DEBUG_ASSERT(handlers[3]->has_modrm == has_modrm_);
	}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_W : OpCodeHandler {
	const OpCodeHandler* handlers[2];
	OpCodeHandler_W(const OpCodeHandler* handler_w0, const OpCodeHandler* handler_w1) noexcept
		: OpCodeHandler(&decode, true), handlers{handler_w0, handler_w1} {
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler_w0));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler_w1));
	}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_Bitness : OpCodeHandler {
	const OpCodeHandler* handler1632;
	const OpCodeHandler* handler64;
	OpCodeHandler_Bitness(const OpCodeHandler* handler1632_, const OpCodeHandler* handler64_) noexcept
		: OpCodeHandler(&decode, false), handler1632(handler1632_), handler64(handler64_) {
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler1632_));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler64_));
	}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_Bitness_DontReadModRM : OpCodeHandler {
	const OpCodeHandler* handler1632;
	const OpCodeHandler* handler64;
	OpCodeHandler_Bitness_DontReadModRM(const OpCodeHandler* handler1632_, const OpCodeHandler* handler64_) noexcept
		: OpCodeHandler(&decode, true), handler1632(handler1632_), handler64(handler64_) {
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler1632_));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler64_));
	}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_RM : OpCodeHandler {
	const OpCodeHandler* reg;
	const OpCodeHandler* mem;
	OpCodeHandler_RM(const OpCodeHandler* reg_, const OpCodeHandler* mem_) noexcept : OpCodeHandler(&decode, true), reg(reg_), mem(mem_) {
		ICED_DEBUG_ASSERT(!is_null_instance_handler(reg_));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(mem_));
	}
	ICED_DECODE_FN_DECL;
};

struct HandlerOptionsInfo {
	const OpCodeHandler* handler;
	std::uint32_t options;
};

struct OpCodeHandler_Options1632 : OpCodeHandler {
	const OpCodeHandler* default_handler;
	HandlerOptionsInfo infos[2];
	std::uint32_t info_options;
	OpCodeHandler_Options1632(const OpCodeHandler* default_handler_, const OpCodeHandler* handler1, std::uint32_t options1) noexcept
		: OpCodeHandler(&decode, false), default_handler(default_handler_),
		  infos{HandlerOptionsInfo{handler1, options1}, HandlerOptionsInfo{get_invalid_no_modrm_handler(), 0}}, info_options(options1) {
		ICED_DEBUG_ASSERT(!is_null_instance_handler(default_handler_));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler1));
	}
	OpCodeHandler_Options1632(const OpCodeHandler* default_handler_, const OpCodeHandler* handler1, std::uint32_t options1,
							  const OpCodeHandler* handler2, std::uint32_t options2) noexcept
		: OpCodeHandler(&decode, false), default_handler(default_handler_),
		  infos{HandlerOptionsInfo{handler1, options1}, HandlerOptionsInfo{handler2, options2}}, info_options(options1 | options2) {
		ICED_DEBUG_ASSERT(!is_null_instance_handler(default_handler_));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler1));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler2));
	}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_Options : OpCodeHandler {
	const OpCodeHandler* default_handler;
	HandlerOptionsInfo infos[2];
	std::uint32_t info_options;
	OpCodeHandler_Options(const OpCodeHandler* default_handler_, const OpCodeHandler* handler1, std::uint32_t options1) noexcept
		: OpCodeHandler(&decode, false), default_handler(default_handler_),
		  infos{HandlerOptionsInfo{handler1, options1}, HandlerOptionsInfo{get_invalid_no_modrm_handler(), 0}}, info_options(options1) {
		ICED_DEBUG_ASSERT(!is_null_instance_handler(default_handler_));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler1));
	}
	OpCodeHandler_Options(const OpCodeHandler* default_handler_, const OpCodeHandler* handler1, std::uint32_t options1, const OpCodeHandler* handler2,
						  std::uint32_t options2) noexcept
		: OpCodeHandler(&decode, false), default_handler(default_handler_),
		  infos{HandlerOptionsInfo{handler1, options1}, HandlerOptionsInfo{handler2, options2}}, info_options(options1 | options2) {
		ICED_DEBUG_ASSERT(!is_null_instance_handler(default_handler_));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler1));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(handler2));
	}
	ICED_DECODE_FN_DECL;
};

struct OpCodeHandler_Options_DontReadModRM : OpCodeHandler {
	const OpCodeHandler* default_handler;
	const OpCodeHandler* opt_handler;
	std::uint32_t flags;
	OpCodeHandler_Options_DontReadModRM(const OpCodeHandler* default_handler_, const OpCodeHandler* opt_handler_, std::uint32_t flags_) noexcept
		: OpCodeHandler(&decode, true), default_handler(default_handler_), opt_handler(opt_handler_), flags(flags_) {
		ICED_DEBUG_ASSERT(!is_null_instance_handler(default_handler_));
		ICED_DEBUG_ASSERT(!is_null_instance_handler(opt_handler_));
	}
	ICED_DECODE_FN_DECL;
};

} // namespace iced_x86::internal
