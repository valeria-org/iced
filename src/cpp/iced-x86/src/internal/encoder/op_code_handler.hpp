// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

#include "iced_x86/code_size.hpp"
#include "iced_x86/tuple_type.hpp"
#include "internal/encoder/ops.hpp"
#include "internal/encoder/w_bit.hpp"
#include <cstdint>
#include <optional>

namespace iced_x86::internal {

// Rust has one struct per handler kind (all start with a `OpCodeHandler` field). C++ uses one struct
// with a union containing the handler kind specific data so all handlers can be stored in one array.
struct OpCodeHandler {
	using EncodeFn = void (*)(const OpCodeHandler* self, Encoder& encoder, const Instruction& instruction);
	using TryConvertToDisp8NFn = std::optional<std::int8_t> (*)(const OpCodeHandler* self, Encoder& encoder, const Instruction& instruction,
		std::int32_t displ);

	static constexpr std::uint32_t MAX_OPERANDS = 5;

	EncodeFn encode;
	// nullptr if it's not EVEX/MVEX
	TryConvertToDisp8NFn try_convert_to_disp8n;
	const Op* operands[MAX_OPERANDS];
	std::uint32_t operands_len;
	std::uint32_t op_code;
	std::int32_t group_index;
	std::int32_t rm_group_index;
	std::uint32_t enc_flags3; // EncFlags3
	CodeSize op_size;
	CodeSize addr_size;
	bool is_2byte_opcode;
	bool is_special_instr;

	struct DeclareDataData {
		std::uint32_t elem_size;
	};
	struct LegacyData {
		std::uint32_t table_byte1;
		std::uint32_t table_byte2;
		std::uint32_t mandatory_prefix;
	};
	struct VexData {
		std::uint32_t table;
		std::uint32_t last_byte;
		std::uint32_t mask_w_l;
		std::uint32_t mask_l;
		std::uint32_t w1;
	};
	struct XopData {
		std::uint32_t table;
		std::uint32_t last_byte;
	};
	struct EvexData {
		std::uint32_t table;
		std::uint32_t p1_bits;
		std::uint32_t ll_bits;
		std::uint32_t mask_w;
		std::uint32_t mask_ll;
		TupleType tuple_type;
		WBit wbit;
	};
	struct MvexData {
		std::uint32_t table;
		std::uint32_t p1_bits;
		std::uint32_t mask_w;
		WBit wbit;
	};
	struct D3nowData {
		std::uint32_t immediate;
	};
	union {
		DeclareDataData declare_data;
		LegacyData legacy;
		VexData vex;
		XopData xop;
		EvexData evex;
		MvexData mvex;
		D3nowData d3now;
	} u;
};

// Error message used by the invalid handler
inline constexpr const char* INVALID_HANDLER_ERROR_MESSAGE = "Can't encode an invalid instruction";

// Returns the handlers table (one handler per `Code` value), created the first time it's called
const OpCodeHandler* get_handlers_table() noexcept;

} // namespace iced_x86::internal
