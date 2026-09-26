// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "internal/decoder/table_de.hpp"

#include <cstdlib>
#include <utility>

namespace iced_x86::internal {

void* HandlerAllocator::alloc(std::size_t size, std::size_t align) noexcept {
	ICED_ASSERT(align != 0 && (align & (align - 1)) == 0 && align <= alignof(std::max_align_t));
	std::size_t misalign = reinterpret_cast<std::uintptr_t>(chunk_) & (align - 1);
	std::size_t pad = misalign == 0 ? 0 : align - misalign;
	if (chunk_ == nullptr || pad + size > chunk_left_) {
		// The old chunk (if any) is abandoned. Its unused bytes are wasted, but they're usually just a few bytes.
		std::size_t new_size = size > CHUNK_SIZE ? size : CHUNK_SIZE;
		// Never freed (see class comment). malloc() returns memory aligned to alignof(std::max_align_t)
		chunk_ = static_cast<unsigned char*>(std::malloc(new_size));
		ICED_ASSERT(chunk_ != nullptr);
		chunk_left_ = new_size;
		pad = 0;
	}
	unsigned char* p = chunk_ + pad;
	chunk_ = p + size;
	chunk_left_ -= pad + size;
	return p;
}

TableDeserializer::TableDeserializer(HandlerAllocator& allocator, const std::uint8_t* data, std::size_t data_size, std::size_t max_ids,
									 HandlerReader handler_reader) noexcept
	: allocator_(allocator), reader_(data, data_size), handler_reader_(handler_reader), id_to_handler_(), temp_vecs_() {
	id_to_handler_.reserve(max_ids);
}

void TableDeserializer::deserialize() noexcept {
	while (reader_.can_read()) {
		auto kind = static_cast<SerializedDataKind>(reader_.read_u8());
		switch (kind) {
		case SerializedDataKind::HandlerReference: {
			const OpCodeHandler* tmp = read_handler();
			id_to_handler_.push_back(HandlerInfo{false, tmp, HandlerVec()});
			break;
		}

		case SerializedDataKind::ArrayReference: {
			std::size_t size = reader_.read_compressed_u32();
			HandlerVec tmp = read_handlers(size);
			id_to_handler_.push_back(HandlerInfo{true, nullptr, std::move(tmp)});
			break;
		}

		default:
			ICED_UNREACHABLE();
		}
	}
	ICED_DEBUG_ASSERT(!reader_.can_read());
}

const OpCodeHandler* TableDeserializer::read_handler_or_null_instance() noexcept {
	HandlerVec tmp_vec;
	if (!temp_vecs_.empty()) {
		tmp_vec = std::move(temp_vecs_.back());
		temp_vecs_.pop_back();
	}
	else
		tmp_vec.reserve(1);
	ICED_DEBUG_ASSERT(tmp_vec.empty());
	handler_reader_(*this, tmp_vec);
	ICED_ASSERT(tmp_vec.size() == 1);
	const OpCodeHandler* result = tmp_vec.back();
	tmp_vec.pop_back();
	temp_vecs_.push_back(std::move(tmp_vec));
	return result;
}

HandlerVec TableDeserializer::read_handlers(std::size_t count) noexcept {
	HandlerVec handlers;
	handlers.reserve(count);
	std::size_t i = 0;
	while (handlers.size() < count) {
		std::size_t len = handlers.size();
		handler_reader_(*this, handlers);
		ICED_DEBUG_ASSERT(handlers.size() >= len);
		std::size_t size = handlers.size() - len;
		if (size == 0)
			break; // will abort below
		i += size;
		ICED_DEBUG_ASSERT(handlers.size() == i);
	}
	ICED_ASSERT(handlers.size() == count);
	ICED_DEBUG_ASSERT(count == i);
	return handlers;
}

const OpCodeHandler* TableDeserializer::read_handler_reference() noexcept {
	std::size_t index = reader_.read_u8();
	ICED_ASSERT(index < id_to_handler_.size());
	const HandlerInfo& info = id_to_handler_[index];
	ICED_ASSERT(!info.is_handlers);
	return info.handler;
}

HandlerVec TableDeserializer::read_array_reference(std::uint32_t kind) noexcept {
	std::uint32_t read_kind = static_cast<std::uint32_t>(reader_.read_u8());
	ICED_DEBUG_ASSERT(read_kind == kind);
	(void)read_kind;
	(void)kind;
	std::size_t index = reader_.read_u8();
	ICED_ASSERT(index < id_to_handler_.size());
	const HandlerInfo& info = id_to_handler_[index];
	ICED_ASSERT(info.is_handlers);
	// There are a few dupe refs, clone the whole thing
	return info.handlers;
}

HandlerVec TableDeserializer::read_array_reference_no_clone(std::uint32_t kind) noexcept {
	std::uint32_t read_kind = static_cast<std::uint32_t>(reader_.read_u8());
	ICED_DEBUG_ASSERT(read_kind == kind);
	(void)read_kind;
	(void)kind;
	std::size_t index = reader_.read_u8();
	return table(index);
}

HandlerVec TableDeserializer::table(std::size_t index) noexcept {
	ICED_ASSERT(index < id_to_handler_.size());
	HandlerInfo& info = id_to_handler_[index];
	ICED_ASSERT(info.is_handlers);
	HandlerVec handlers = std::move(info.handlers);
	info.handlers = HandlerVec();
	ICED_DEBUG_ASSERT(!handlers.empty());
	return handlers;
}

} // namespace iced_x86::internal
