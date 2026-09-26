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
static const OpCodeHandler* const* to_table(HandlerAllocator& allocator, const HandlerVec& handlers) noexcept {
	ICED_ASSERT(handlers.size() == 0x100);
	auto table = static_cast<const OpCodeHandler**>(allocator.alloc(sizeof(const OpCodeHandler*) * 0x100, alignof(const OpCodeHandler*)));
	for (std::size_t i = 0; i < 0x100; i++)
		table[i] = handlers[i];
	return table;
}

static DecoderTables create_decoder_tables() noexcept {
	DecoderTables tables{};
	HandlerAllocator allocator;

	{
		TableDeserializer deserializer(allocator, decoder_data_legacy::TBL_DATA, decoder_data_legacy::TBL_DATA_SIZE,
									   decoder_data_legacy::MAX_ID_NAMES, legacy_read_handlers);
		deserializer.deserialize();
		tables.handlers_map0 = to_table(allocator, deserializer.table(decoder_data_legacy::HANDLERS_MAP0_INDEX));
	}

	{
		TableDeserializer deserializer(allocator, decoder_data_vex::TBL_DATA, decoder_data_vex::TBL_DATA_SIZE, decoder_data_vex::MAX_ID_NAMES,
									   vex_read_handlers);
		deserializer.deserialize();
		tables.handlers_vex_map0 = to_table(allocator, deserializer.table(decoder_data_vex::HANDLERS_MAP0_INDEX));
		tables.handlers_vex_0f = to_table(allocator, deserializer.table(decoder_data_vex::HANDLERS_0F_INDEX));
		tables.handlers_vex_0f38 = to_table(allocator, deserializer.table(decoder_data_vex::HANDLERS_0F38_INDEX));
		tables.handlers_vex_0f3a = to_table(allocator, deserializer.table(decoder_data_vex::HANDLERS_0F3A_INDEX));
	}

	{
		TableDeserializer deserializer(allocator, decoder_data_evex::TBL_DATA, decoder_data_evex::TBL_DATA_SIZE, decoder_data_evex::MAX_ID_NAMES,
									   evex_read_handlers);
		deserializer.deserialize();
		tables.handlers_evex_0f = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_0F_INDEX));
		tables.handlers_evex_0f38 = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_0F38_INDEX));
		tables.handlers_evex_0f3a = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_0F3A_INDEX));
		tables.handlers_evex_map5 = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_MAP5_INDEX));
		tables.handlers_evex_map6 = to_table(allocator, deserializer.table(decoder_data_evex::HANDLERS_MAP6_INDEX));
	}

	{
		TableDeserializer deserializer(allocator, decoder_data_xop::TBL_DATA, decoder_data_xop::TBL_DATA_SIZE, decoder_data_xop::MAX_ID_NAMES,
									   vex_read_handlers);
		deserializer.deserialize();
		tables.handlers_xop_map8 = to_table(allocator, deserializer.table(decoder_data_xop::HANDLERS_MAP8_INDEX));
		tables.handlers_xop_map9 = to_table(allocator, deserializer.table(decoder_data_xop::HANDLERS_MAP9_INDEX));
		tables.handlers_xop_map10 = to_table(allocator, deserializer.table(decoder_data_xop::HANDLERS_MAP10_INDEX));
	}

	{
		TableDeserializer deserializer(allocator, decoder_data_mvex::TBL_DATA, decoder_data_mvex::TBL_DATA_SIZE, decoder_data_mvex::MAX_ID_NAMES,
									   mvex_read_handlers);
		deserializer.deserialize();
		tables.handlers_mvex_0f = to_table(allocator, deserializer.table(decoder_data_mvex::HANDLERS_0F_INDEX));
		tables.handlers_mvex_0f38 = to_table(allocator, deserializer.table(decoder_data_mvex::HANDLERS_0F38_INDEX));
		tables.handlers_mvex_0f3a = to_table(allocator, deserializer.table(decoder_data_mvex::HANDLERS_0F3A_INDEX));
	}

	tables.invalid_map = to_table(allocator, HandlerVec(0x100, get_invalid_handler()));

	return tables;
}

const DecoderTables& get_decoder_tables() noexcept {
	// Created once, never freed. DecoderTables is trivially destructible so no destructor is registered.
	static const DecoderTables tables = create_decoder_tables();
	return tables;
}

} // namespace iced_x86::internal
