// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Deserializes the generated decoder tables (src/decoder/data_*.cpp) into handlers (Rust: decoder/table_de/mod.rs)

#pragma once

#include "iced_x86/code.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/register.hpp"
#include "iced_x86/tuple_type.hpp"
#include "internal/data_reader.hpp"
#include "internal/decoder/evex_op_code_handler_kind.hpp"
#include "internal/decoder/handlers.hpp"
#include "internal/decoder/legacy_op_code_handler_kind.hpp"
#include "internal/decoder/mvex_op_code_handler_kind.hpp"
#include "internal/decoder/serialized_data_kind.hpp"
#include "internal/decoder/vex_op_code_handler_kind.hpp"
#include "internal/iced_assert.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

namespace iced_x86::internal {

// Allocates the handlers. The memory is never freed (the handlers are used by all decoders until the process exits),
// same as the Rust code which leaks the boxed handlers.
class HandlerAllocator {
public:
	HandlerAllocator() noexcept : chunk_(nullptr), chunk_left_(0) {}
	void* alloc(std::size_t size, std::size_t align) noexcept;

private:
	static constexpr std::size_t CHUNK_SIZE = 0x10000;
	unsigned char* chunk_;
	std::size_t chunk_left_;
};

// Creates a handler: `ICED_NEW_HANDLER(deserializer, OpCodeHandler_RM, {deserializer.read_handler(), deserializer.read_handler()})`.
// The ctor args are evaluated from left to right since it's a braced-init-list (same order as Rust).
#define ICED_NEW_HANDLER(deserializer, T, ...) (::new ((deserializer).alloc(sizeof(T), alignof(T))) T __VA_ARGS__)

struct Code2 {
	Code code1;
	Code code2;
};

struct Code3 {
	Code code1;
	Code code2;
	Code code3;
};

class TableDeserializer {
public:
	using HandlerReader = void (*)(TableDeserializer& deserializer, HandlerVec& result);

	TableDeserializer(HandlerAllocator& allocator, const std::uint8_t* data, std::size_t data_size, std::size_t max_ids,
					  HandlerReader handler_reader) noexcept;

	void deserialize() noexcept;

	void* alloc(std::size_t size, std::size_t align) noexcept { return allocator_.alloc(size, align); }

	LegacyOpCodeHandlerKind read_legacy_op_code_handler_kind() noexcept { return static_cast<LegacyOpCodeHandlerKind>(reader_.read_u8()); }
	VexOpCodeHandlerKind read_vex_op_code_handler_kind() noexcept { return static_cast<VexOpCodeHandlerKind>(reader_.read_u8()); }
	EvexOpCodeHandlerKind read_evex_op_code_handler_kind() noexcept { return static_cast<EvexOpCodeHandlerKind>(reader_.read_u8()); }
	MvexOpCodeHandlerKind read_mvex_op_code_handler_kind() noexcept { return static_cast<MvexOpCodeHandlerKind>(reader_.read_u8()); }

	Code read_code() noexcept {
		std::uint32_t v = reader_.read_compressed_u32();
		ICED_DEBUG_ASSERT(v < IcedConstants::CODE_ENUM_COUNT);
		return static_cast<Code>(v);
	}

	Code2 read_code2() noexcept {
		std::uint32_t v = reader_.read_compressed_u32();
		ICED_DEBUG_ASSERT(v < IcedConstants::CODE_ENUM_COUNT);
		ICED_DEBUG_ASSERT(v + 1 < IcedConstants::CODE_ENUM_COUNT);
		return Code2{static_cast<Code>(v), static_cast<Code>(v + 1)};
	}

	Code3 read_code3() noexcept {
		std::uint32_t v = reader_.read_compressed_u32();
		ICED_DEBUG_ASSERT(v < IcedConstants::CODE_ENUM_COUNT);
		ICED_DEBUG_ASSERT(v + 2 < IcedConstants::CODE_ENUM_COUNT);
		return Code3{static_cast<Code>(v), static_cast<Code>(v + 1), static_cast<Code>(v + 2)};
	}

	Register read_register() noexcept {
		std::size_t v = reader_.read_u8();
		ICED_DEBUG_ASSERT(v < IcedConstants::REGISTER_ENUM_COUNT);
		return static_cast<Register>(v);
	}

	std::uint32_t read_decoder_options() noexcept { return reader_.read_compressed_u32(); }
	std::uint32_t read_handler_flags() noexcept { return reader_.read_compressed_u32(); }
	std::uint32_t read_legacy_handler_flags() noexcept { return reader_.read_compressed_u32(); }

	TupleType read_tuple_type() noexcept {
		std::size_t v = reader_.read_u8();
		ICED_DEBUG_ASSERT(v < IcedConstants::TUPLE_TYPE_ENUM_COUNT);
		return static_cast<TupleType>(v);
	}

	bool read_boolean() noexcept { return reader_.read_u8() != 0; }
	std::uint32_t read_u32() noexcept { return reader_.read_compressed_u32(); }

	const OpCodeHandler* read_handler() noexcept {
		const OpCodeHandler* result = read_handler_or_null_instance();
		ICED_DEBUG_ASSERT(!is_null_instance_handler(result));
		return result;
	}

	const OpCodeHandler* read_handler_or_null_instance() noexcept;
	HandlerVec read_handlers(std::size_t count) noexcept;
	const OpCodeHandler* read_handler_reference() noexcept;
	HandlerVec read_array_reference(std::uint32_t kind) noexcept;
	HandlerVec read_array_reference_no_clone(std::uint32_t kind) noexcept;
	HandlerVec table(std::size_t index) noexcept;

private:
	struct HandlerInfo {
		// true if it's `handlers`, false if it's `handler`
		bool is_handlers;
		const OpCodeHandler* handler;
		HandlerVec handlers;
	};

	HandlerAllocator& allocator_;
	DataReader reader_;
	HandlerReader handler_reader_;
	std::vector<HandlerInfo> id_to_handler_;
	std::vector<HandlerVec> temp_vecs_;
};

// Handler readers (src/decoder/*_reader.cpp)
void legacy_read_handlers(TableDeserializer& deserializer, HandlerVec& result) noexcept;
void vex_read_handlers(TableDeserializer& deserializer, HandlerVec& result) noexcept;
void evex_read_handlers(TableDeserializer& deserializer, HandlerVec& result) noexcept;
void mvex_read_handlers(TableDeserializer& deserializer, HandlerVec& result) noexcept;

} // namespace iced_x86::internal
