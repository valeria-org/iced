// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Tests generated from the Rust doc examples (memory_size.rs)

#include "test_framework.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/memory_size_ext.hpp"

#include <unordered_set>

using namespace iced_x86;

TEST_CASE("core/memory_size_ext/doc_memory_size") {
	{
		const auto& info = memory_size_ext::info(MemorySize::Packed256_UInt16);
		CHECK(info.memory_size() == MemorySize::Packed256_UInt16);
	}
}

TEST_CASE("core/memory_size_ext/doc_size") {
	{
		const auto& info = memory_size_ext::info(MemorySize::UInt32);
		CHECK(info.size() == 4);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Packed256_UInt16);
		CHECK(info.size() == 32);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Broadcast512_UInt64);
		CHECK(info.size() == 8);
	}
}

TEST_CASE("core/memory_size_ext/doc_element_size") {
	{
		const auto& info = memory_size_ext::info(MemorySize::UInt32);
		CHECK(info.element_size() == 4);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Packed256_UInt16);
		CHECK(info.element_size() == 2);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Broadcast512_UInt64);
		CHECK(info.element_size() == 8);
	}
}

TEST_CASE("core/memory_size_ext/doc_element_type") {
	{
		const auto& info = memory_size_ext::info(MemorySize::UInt32);
		CHECK(info.element_type() == MemorySize::UInt32);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Packed256_UInt16);
		CHECK(info.element_type() == MemorySize::UInt16);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Broadcast512_UInt64);
		CHECK(info.element_type() == MemorySize::UInt64);
	}
}

TEST_CASE("core/memory_size_ext/doc_element_type_info") {
	{
		const auto& info = memory_size_ext::info(MemorySize::UInt32).element_type_info();
		CHECK(info.memory_size() == MemorySize::UInt32);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Packed256_UInt16).element_type_info();
		CHECK(info.memory_size() == MemorySize::UInt16);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Broadcast512_UInt64).element_type_info();
		CHECK(info.memory_size() == MemorySize::UInt64);
	}
}

TEST_CASE("core/memory_size_ext/doc_is_signed") {
	{
		const auto& info = memory_size_ext::info(MemorySize::UInt32);
		CHECK(!info.is_signed());
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Int32);
		CHECK(info.is_signed());
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Float64);
		CHECK(info.is_signed());
	}
}

TEST_CASE("core/memory_size_ext/doc_is_broadcast") {
	{
		const auto& info = memory_size_ext::info(MemorySize::UInt32);
		CHECK(!info.is_broadcast());
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Packed256_UInt16);
		CHECK(!info.is_broadcast());
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Broadcast512_UInt64);
		CHECK(info.is_broadcast());
	}
}

TEST_CASE("core/memory_size_ext/doc_is_packed") {
	{
		const auto& info = memory_size_ext::info(MemorySize::UInt32);
		CHECK(!info.is_packed());
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Packed256_UInt16);
		CHECK(info.is_packed());
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Broadcast512_UInt64);
		CHECK(!info.is_packed());
	}
}

TEST_CASE("core/memory_size_ext/doc_element_count") {
	{
		const auto& info = memory_size_ext::info(MemorySize::UInt32);
		CHECK(info.element_count() == 1);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Packed256_UInt16);
		CHECK(info.element_count() == 16);
	}
	{
		const auto& info = memory_size_ext::info(MemorySize::Broadcast512_UInt64);
		CHECK(info.element_count() == 1);
	}
}

TEST_CASE("core/memory_size_ext/doc_info") {
	{
		const auto& info = memory_size_ext::info(MemorySize::Packed256_UInt16);
		CHECK(info.size() == 32);
	}
}

TEST_CASE("core/memory_size_ext/doc_size_2") {
	CHECK(memory_size_ext::size(MemorySize::UInt32) == 4);
	CHECK(memory_size_ext::size(MemorySize::Packed256_UInt16) == 32);
	CHECK(memory_size_ext::size(MemorySize::Broadcast512_UInt64) == 8);
}

TEST_CASE("core/memory_size_ext/doc_element_size_2") {
	CHECK(memory_size_ext::element_size(MemorySize::UInt32) == 4);
	CHECK(memory_size_ext::element_size(MemorySize::Packed256_UInt16) == 2);
	CHECK(memory_size_ext::element_size(MemorySize::Broadcast512_UInt64) == 8);
}

TEST_CASE("core/memory_size_ext/doc_element_type_2") {
	CHECK(memory_size_ext::element_type(MemorySize::UInt32) == MemorySize::UInt32);
	CHECK(memory_size_ext::element_type(MemorySize::Packed256_UInt16) == MemorySize::UInt16);
	CHECK(memory_size_ext::element_type(MemorySize::Broadcast512_UInt64) == MemorySize::UInt64);
}

TEST_CASE("core/memory_size_ext/doc_element_type_info_2") {
	CHECK(memory_size_ext::element_type_info(MemorySize::UInt32).memory_size() == MemorySize::UInt32);
	CHECK(memory_size_ext::element_type_info(MemorySize::Packed256_UInt16).memory_size() == MemorySize::UInt16);
	CHECK(memory_size_ext::element_type_info(MemorySize::Broadcast512_UInt64).memory_size() == MemorySize::UInt64);
}

TEST_CASE("core/memory_size_ext/doc_is_signed_2") {
	CHECK(!memory_size_ext::is_signed(MemorySize::UInt32));
	CHECK(memory_size_ext::is_signed(MemorySize::Int32));
	CHECK(memory_size_ext::is_signed(MemorySize::Float64));
}

TEST_CASE("core/memory_size_ext/doc_is_packed_2") {
	CHECK(!memory_size_ext::is_packed(MemorySize::UInt32));
	CHECK(memory_size_ext::is_packed(MemorySize::Packed256_UInt16));
	CHECK(!memory_size_ext::is_packed(MemorySize::Broadcast512_UInt64));
}

TEST_CASE("core/memory_size_ext/doc_element_count_2") {
	CHECK(memory_size_ext::element_count(MemorySize::UInt32) == 1);
	CHECK(memory_size_ext::element_count(MemorySize::Packed256_UInt16) == 16);
	CHECK(memory_size_ext::element_count(MemorySize::Broadcast512_UInt64) == 1);
}

TEST_CASE("core/memory_size_ext/doc_is_broadcast_2") {
	CHECK(!memory_size_ext::is_broadcast(MemorySize::Packed64_Float16));
	CHECK(memory_size_ext::is_broadcast(MemorySize::Broadcast512_UInt64));
}

TEST_CASE("core/memory_size_ext/info_table") {
	for (std::size_t i = 0; i < IcedConstants::MEMORY_SIZE_ENUM_COUNT; i++) {
		const auto memory_size = static_cast<MemorySize>(i);
		const MemorySizeInfo& info = memory_size_ext::info(memory_size);
		CHECK(info.memory_size() == memory_size);
		CHECK(memory_size_ext::size(memory_size) == info.size());
		CHECK(memory_size_ext::element_size(memory_size) == info.element_size());
		CHECK(memory_size_ext::element_type(memory_size) == info.element_type());
		CHECK(&memory_size_ext::element_type_info(memory_size) == &info.element_type_info());
		CHECK(memory_size_ext::is_signed(memory_size) == info.is_signed());
		CHECK(memory_size_ext::is_packed(memory_size) == info.is_packed());
		CHECK(memory_size_ext::element_count(memory_size) == info.element_count());
		CHECK(memory_size_ext::is_broadcast(memory_size) == info.is_broadcast());
		CHECK(info.element_size() <= info.size());
	}
	std::unordered_set<MemorySizeInfo> set;
	set.insert(memory_size_ext::info(MemorySize::UInt8));
	set.insert(memory_size_ext::info(MemorySize::UInt8));
	set.insert(memory_size_ext::info(MemorySize::Int8));
	CHECK(set.size() == 2);
}
