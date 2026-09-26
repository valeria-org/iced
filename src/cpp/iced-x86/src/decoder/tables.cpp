// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "internal/decoder/tables.hpp"
#include "internal/decoder/data_evex.hpp"
#include "internal/decoder/data_legacy.hpp"
#include "internal/decoder/data_mvex.hpp"
#include "internal/decoder/data_vex.hpp"
#include "internal/decoder/data_xop.hpp"
#include "internal/decoder/table_de.hpp"

namespace iced_x86::internal {

// Copies the 0x100 handlers to memory that is never freed
static const HandlerEntry* to_table(HandlerAllocator& allocator, const HandlerVec& handlers) noexcept {
	ICED_ASSERT(handlers.size() == 0x100);
	auto table = static_cast<HandlerEntry*>(allocator.alloc(sizeof(HandlerEntry) * 0x100, alignof(HandlerEntry)));
	for (std::size_t i = 0; i < 0x100; i++)
		table[i] = to_handler_entry(handlers[i]);
	return table;
}

// The create_*_tables() functions aren't inlined so each TableDeserializer is in its own stack frame (debug builds
// don't reuse the stack slots of the other deserializers)
ICED_NOINLINE static void create_legacy_tables(HandlerAllocator& allocator, DecoderTables& tables) noexcept {
	TableDeserializer deserializer(allocator, decoder_data_legacy::TBL_DATA, decoder_data_legacy::TBL_DATA_SIZE,
								   decoder_data_legacy::MAX_ID_NAMES, legacy_read_handlers);
	deserializer.deserialize();
	tables.handlers_map0 = to_table(allocator, deserializer.table(decoder_data_legacy::HANDLERS_MAP0_INDEX));
}

ICED_NOINLINE static void create_vex_tables(HandlerAllocator& allocator, DecoderTables& tables) noexcept {
	TableDeserializer deserializer(allocator, decoder_data_vex::TBL_DATA, decoder_data_vex::TBL_DATA_SIZE, decoder_data_vex::MAX_ID_NAMES,
								   vex_read_handlers);
	deserializer.deserialize();
	tables.handlers_vex_map0 = to_table(allocator, deserializer.table(decoder_data_vex::HANDLERS_MAP0_INDEX));
	tables.handlers_vex_0f = to_table(allocator, deserializer.table(decoder_data_vex::HANDLERS_0F_INDEX));
	tables.handlers_vex_0f38 = to_table(allocator, deserializer.table(decoder_data_vex::HANDLERS_0F38_INDEX));
	tables.handlers_vex_0f3a = to_table(allocator, deserializer.table(decoder_data_vex::HANDLERS_0F3A_INDEX));
}

ICED_NOINLINE static void create_evex_tables(HandlerAllocator& allocator, DecoderTables& tables) noexcept {
	TableDeserializer deserializer(allocator, decoder_data_evex::TBL_DATA, decoder_data_evex::TBL_DATA_SIZE, decoder_data_evex::MAX_ID_NAMES,
								   evex_read_handlers);
	deserializer.deserialize();
	tables.handlers_evex_0f = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_0F_INDEX));
	tables.handlers_evex_0f38 = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_0F38_INDEX));
	tables.handlers_evex_0f3a = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_0F3A_INDEX));
	tables.handlers_evex_map5 = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_MAP5_INDEX));
	tables.handlers_evex_map6 = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_MAP6_INDEX));
}

ICED_NOINLINE static void create_xop_tables(HandlerAllocator& allocator, DecoderTables& tables) noexcept {
	TableDeserializer deserializer(allocator, decoder_data_xop::TBL_DATA, decoder_data_xop::TBL_DATA_SIZE, decoder_data_xop::MAX_ID_NAMES,
								   vex_read_handlers);
	deserializer.deserialize();
	tables.handlers_xop_map8 = to_table(allocator, deserializer.table(decoder_data_xop::HANDLERS_MAP8_INDEX));
	tables.handlers_xop_map9 = to_table(allocator, deserializer.table(decoder_data_xop::HANDLERS_MAP9_INDEX));
	tables.handlers_xop_map10 = to_table(allocator, deserializer.table(decoder_data_xop::HANDLERS_MAP10_INDEX));
}

ICED_NOINLINE static void create_mvex_tables(HandlerAllocator& allocator, DecoderTables& tables) noexcept {
	TableDeserializer deserializer(allocator, decoder_data_mvex::TBL_DATA, decoder_data_mvex::TBL_DATA_SIZE, decoder_data_mvex::MAX_ID_NAMES,
								   mvex_read_handlers);
	deserializer.deserialize();
	tables.handlers_mvex_0f = to_table(allocator, deserializer.table(decoder_data_mvex::HANDLERS_0F_INDEX));
	tables.handlers_mvex_0f38 = to_table(allocator, deserializer.table(decoder_data_mvex::HANDLERS_0F38_INDEX));
	tables.handlers_mvex_0f3a = to_table(allocator, deserializer.table(decoder_data_mvex::HANDLERS_0F3A_INDEX));
}

static DecoderTables create_decoder_tables() noexcept {
	DecoderTables tables{};
	HandlerAllocator allocator;

	create_legacy_tables(allocator, tables);
	create_vex_tables(allocator, tables);
	create_evex_tables(allocator, tables);
	create_xop_tables(allocator, tables);
	create_mvex_tables(allocator, tables);

	tables.invalid_map = to_table(allocator, HandlerVec(0x100, get_invalid_handler()));

	return tables;
}

const DecoderTables& get_decoder_tables() noexcept {
	// Created once, never freed. DecoderTables is trivially destructible so no destructor is registered.
	static const DecoderTables tables = create_decoder_tables();
	return tables;
}

} // namespace iced_x86::internal
