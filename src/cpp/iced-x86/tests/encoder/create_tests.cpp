// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's encoder/tests/create.rs

#include "encoder/encoder_test_utils.hpp"
#include "iced_x86/code_size.hpp"
#include "iced_x86/encoder.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/memory_operand.hpp"
#include "iced_x86/rep_prefix_kind.hpp"
#include "iced_x86/rounding_control.hpp"
#include "test_framework.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Rust: `if cfg!(debug_assertions) { assert!(panic::catch_unwind(|| expr).is_err()) } else { let _ = expr; }`
#ifndef NDEBUG
#define CHECK_DEBUG_PANICS(expr) CHECK(::iced_x86::tests::aborts([&] { (void)(expr); }))
#else
#define CHECK_DEBUG_PANICS(expr) ((void)(expr))
#endif

namespace iced_x86::tests {

namespace {

std::vector<std::uint8_t> get_data(const Instruction& instr) {
	std::size_t elem_size;
	switch (instr.code()) {
	case Code::DeclareByte:
		elem_size = 1;
		break;
	case Code::DeclareWord:
		elem_size = 2;
		break;
	case Code::DeclareDword:
		elem_size = 4;
		break;
	case Code::DeclareQword:
		elem_size = 8;
		break;
	default:
		throw std::runtime_error("unreachable");
	}
	const std::size_t length = instr.declare_data_len() * elem_size;
	std::vector<std::uint8_t> v;
	v.reserve(length);
	for (std::size_t i = 0; i < length; i++)
		v.push_back(instr.get_declare_byte_value(i));
	return v;
}

// (bitness, hex_bytes, decoder_options, created instruction)
using WithTest = std::tuple<std::uint32_t, const char*, std::uint32_t, Instruction>;

void with_test_core(const std::vector<WithTest>& tests) {
	for (const auto& [bitness, hex_bytes, options, created_instr] : tests) {
		const auto bytes = to_vec_u8(hex_bytes);
		auto decoder = create_decoder(bitness, bytes, get_default_ip(bitness), options).decoder;
		const std::uint64_t orig_rip = decoder.ip();
		Instruction decoded_instr = decoder.decode();
		decoded_instr.set_code_size(CodeSize::Unknown);
		decoded_instr.set_len(0);
		decoded_instr.set_next_ip(0);

		CHECK_MSG(decoded_instr.eq_all_bits(created_instr), hex_bytes);

		Encoder encoder(decoder.bitness());
		(void)encoder.encode(created_instr, orig_rip).value();
		CHECK_MSG(encoder.take_buffer() == bytes, hex_bytes);
	}
}

} // namespace

TEST_CASE("encoder/encoder_ignores_prefixes_if_declare_data") {
	const auto verify = [](const Instruction& orig_instr) {
		Instruction instr = orig_instr;
		const auto orig_data = get_data(instr);
		instr.set_has_lock_prefix(true);
		instr.set_has_repe_prefix(true);
		instr.set_has_repne_prefix(true);
		instr.set_segment_prefix(Register::GS);
		instr.set_has_xrelease_prefix(true);
		instr.set_has_xacquire_prefix(true);
		instr.set_suppress_all_exceptions(true);
		instr.set_zeroing_masking(true);
		for (std::uint32_t bitness : {16U, 32U, 64U}) {
			Encoder encoder(bitness);
			(void)encoder.encode(instr, 0).value();
			CHECK(encoder.take_buffer() == orig_data);
		}
	};

	verify(Instruction::with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08));
	verify(Instruction::with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08));
	verify(Instruction::with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08));
	verify(Instruction::with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08));
	verify(Instruction::try_with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08).value());
	verify(Instruction::try_with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08).value());
	verify(Instruction::try_with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08).value());
	verify(Instruction::try_with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08).value());
}

TEST_CASE("encoder/declare_data_byte_order_is_same") {
	auto data = std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08};
	Instruction db = Instruction::with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08);
	Instruction dw = Instruction::with_declare_word_8(0xA977, 0x9DCE, 0x0555, 0x6C42, 0x3286, 0x4FFE, 0x2734, 0x08AA);
	Instruction dd = Instruction::with_declare_dword_4(0x9DCE'A977, 0x6C42'0555, 0x4FFE'3286, 0x08AA'2734);
	Instruction dq = Instruction::with_declare_qword_2(0x6C42'0555'9DCE'A977, 0x08AA'2734'4FFE'3286);
	auto data1 = get_data(db);
	auto data2 = get_data(dw);
	auto data4 = get_data(dd);
	auto data8 = get_data(dq);
	CHECK(data1 == data);
	CHECK(data2 == data);
	CHECK(data4 == data);
	CHECK(data8 == data);
}

TEST_CASE("encoder/try_declare_data_byte_order_is_same") {
	auto data = std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08};
	Instruction db = Instruction::try_with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08)
		.value();
	Instruction dw = Instruction::try_with_declare_word_8(0xA977, 0x9DCE, 0x0555, 0x6C42, 0x3286, 0x4FFE, 0x2734, 0x08AA).value();
	Instruction dd = Instruction::try_with_declare_dword_4(0x9DCE'A977, 0x6C42'0555, 0x4FFE'3286, 0x08AA'2734).value();
	Instruction dq = Instruction::try_with_declare_qword_2(0x6C42'0555'9DCE'A977, 0x08AA'2734'4FFE'3286).value();
	auto data1 = get_data(db);
	auto data2 = get_data(dw);
	auto data4 = get_data(dd);
	auto data8 = get_data(dq);
	CHECK(data1 == data);
	CHECK(data2 == data);
	CHECK(data4 == data);
	CHECK(data8 == data);
}

TEST_CASE("encoder/declare_byte_can_get_set") {
	Instruction db = Instruction::with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08);
	db.set_declare_byte_value(0, 0xE2);
	db.set_declare_byte_value(1, 0xC5);
	db.set_declare_byte_value(2, 0xFA);
	db.set_declare_byte_value(3, 0xB4);
	db.set_declare_byte_value(4, 0xCB);
	db.set_declare_byte_value(5, 0xE3);
	db.set_declare_byte_value(6, 0x4D);
	db.set_declare_byte_value(7, 0xE4);
	db.set_declare_byte_value(8, 0x96);
	db.set_declare_byte_value(9, 0x98);
	db.set_declare_byte_value(10, 0xFD);
	db.set_declare_byte_value(11, 0x56);
	db.set_declare_byte_value(12, 0x82);
	db.set_declare_byte_value(13, 0x8D);
	db.set_declare_byte_value(14, 0x06);
	db.set_declare_byte_value(15, 0xC3);
	CHECK_EQ(db.get_declare_byte_value(0), 0xE2);
	CHECK_EQ(db.get_declare_byte_value(1), 0xC5);
	CHECK_EQ(db.get_declare_byte_value(2), 0xFA);
	CHECK_EQ(db.get_declare_byte_value(3), 0xB4);
	CHECK_EQ(db.get_declare_byte_value(4), 0xCB);
	CHECK_EQ(db.get_declare_byte_value(5), 0xE3);
	CHECK_EQ(db.get_declare_byte_value(6), 0x4D);
	CHECK_EQ(db.get_declare_byte_value(7), 0xE4);
	CHECK_EQ(db.get_declare_byte_value(8), 0x96);
	CHECK_EQ(db.get_declare_byte_value(9), 0x98);
	CHECK_EQ(db.get_declare_byte_value(10), 0xFD);
	CHECK_EQ(db.get_declare_byte_value(11), 0x56);
	CHECK_EQ(db.get_declare_byte_value(12), 0x82);
	CHECK_EQ(db.get_declare_byte_value(13), 0x8D);
	CHECK_EQ(db.get_declare_byte_value(14), 0x06);
	CHECK_EQ(db.get_declare_byte_value(15), 0xC3);
}

TEST_CASE("encoder/declare_byte_can_get_set_rev") {
	Instruction db = Instruction::with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08);
	db.set_declare_byte_value(15, 0xC3);
	db.set_declare_byte_value(14, 0x06);
	db.set_declare_byte_value(13, 0x8D);
	db.set_declare_byte_value(12, 0x82);
	db.set_declare_byte_value(11, 0x56);
	db.set_declare_byte_value(10, 0xFD);
	db.set_declare_byte_value(9, 0x98);
	db.set_declare_byte_value(8, 0x96);
	db.set_declare_byte_value(7, 0xE4);
	db.set_declare_byte_value(6, 0x4D);
	db.set_declare_byte_value(5, 0xE3);
	db.set_declare_byte_value(4, 0xCB);
	db.set_declare_byte_value(3, 0xB4);
	db.set_declare_byte_value(2, 0xFA);
	db.set_declare_byte_value(1, 0xC5);
	db.set_declare_byte_value(0, 0xE2);
	CHECK_EQ(db.get_declare_byte_value(0), 0xE2);
	CHECK_EQ(db.get_declare_byte_value(1), 0xC5);
	CHECK_EQ(db.get_declare_byte_value(2), 0xFA);
	CHECK_EQ(db.get_declare_byte_value(3), 0xB4);
	CHECK_EQ(db.get_declare_byte_value(4), 0xCB);
	CHECK_EQ(db.get_declare_byte_value(5), 0xE3);
	CHECK_EQ(db.get_declare_byte_value(6), 0x4D);
	CHECK_EQ(db.get_declare_byte_value(7), 0xE4);
	CHECK_EQ(db.get_declare_byte_value(8), 0x96);
	CHECK_EQ(db.get_declare_byte_value(9), 0x98);
	CHECK_EQ(db.get_declare_byte_value(10), 0xFD);
	CHECK_EQ(db.get_declare_byte_value(11), 0x56);
	CHECK_EQ(db.get_declare_byte_value(12), 0x82);
	CHECK_EQ(db.get_declare_byte_value(13), 0x8D);
	CHECK_EQ(db.get_declare_byte_value(14), 0x06);
	CHECK_EQ(db.get_declare_byte_value(15), 0xC3);
}

TEST_CASE("encoder/try_declare_byte_can_get_set") {
	Instruction db = Instruction::try_with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08).value();
	db.try_set_declare_byte_value(0, 0xE2).value();
	db.try_set_declare_byte_value(1, 0xC5).value();
	db.try_set_declare_byte_value(2, 0xFA).value();
	db.try_set_declare_byte_value(3, 0xB4).value();
	db.try_set_declare_byte_value(4, 0xCB).value();
	db.try_set_declare_byte_value(5, 0xE3).value();
	db.try_set_declare_byte_value(6, 0x4D).value();
	db.try_set_declare_byte_value(7, 0xE4).value();
	db.try_set_declare_byte_value(8, 0x96).value();
	db.try_set_declare_byte_value(9, 0x98).value();
	db.try_set_declare_byte_value(10, 0xFD).value();
	db.try_set_declare_byte_value(11, 0x56).value();
	db.try_set_declare_byte_value(12, 0x82).value();
	db.try_set_declare_byte_value(13, 0x8D).value();
	db.try_set_declare_byte_value(14, 0x06).value();
	db.try_set_declare_byte_value(15, 0xC3).value();
	CHECK_EQ(db.try_get_declare_byte_value(0).value(), 0xE2);
	CHECK_EQ(db.try_get_declare_byte_value(1).value(), 0xC5);
	CHECK_EQ(db.try_get_declare_byte_value(2).value(), 0xFA);
	CHECK_EQ(db.try_get_declare_byte_value(3).value(), 0xB4);
	CHECK_EQ(db.try_get_declare_byte_value(4).value(), 0xCB);
	CHECK_EQ(db.try_get_declare_byte_value(5).value(), 0xE3);
	CHECK_EQ(db.try_get_declare_byte_value(6).value(), 0x4D);
	CHECK_EQ(db.try_get_declare_byte_value(7).value(), 0xE4);
	CHECK_EQ(db.try_get_declare_byte_value(8).value(), 0x96);
	CHECK_EQ(db.try_get_declare_byte_value(9).value(), 0x98);
	CHECK_EQ(db.try_get_declare_byte_value(10).value(), 0xFD);
	CHECK_EQ(db.try_get_declare_byte_value(11).value(), 0x56);
	CHECK_EQ(db.try_get_declare_byte_value(12).value(), 0x82);
	CHECK_EQ(db.try_get_declare_byte_value(13).value(), 0x8D);
	CHECK_EQ(db.try_get_declare_byte_value(14).value(), 0x06);
	CHECK_EQ(db.try_get_declare_byte_value(15).value(), 0xC3);
}

TEST_CASE("encoder/try_declare_byte_can_get_set_rev") {
	Instruction db = Instruction::try_with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08).value();
	db.try_set_declare_byte_value(15, 0xC3).value();
	db.try_set_declare_byte_value(14, 0x06).value();
	db.try_set_declare_byte_value(13, 0x8D).value();
	db.try_set_declare_byte_value(12, 0x82).value();
	db.try_set_declare_byte_value(11, 0x56).value();
	db.try_set_declare_byte_value(10, 0xFD).value();
	db.try_set_declare_byte_value(9, 0x98).value();
	db.try_set_declare_byte_value(8, 0x96).value();
	db.try_set_declare_byte_value(7, 0xE4).value();
	db.try_set_declare_byte_value(6, 0x4D).value();
	db.try_set_declare_byte_value(5, 0xE3).value();
	db.try_set_declare_byte_value(4, 0xCB).value();
	db.try_set_declare_byte_value(3, 0xB4).value();
	db.try_set_declare_byte_value(2, 0xFA).value();
	db.try_set_declare_byte_value(1, 0xC5).value();
	db.try_set_declare_byte_value(0, 0xE2).value();
	CHECK_EQ(db.try_get_declare_byte_value(0).value(), 0xE2);
	CHECK_EQ(db.try_get_declare_byte_value(1).value(), 0xC5);
	CHECK_EQ(db.try_get_declare_byte_value(2).value(), 0xFA);
	CHECK_EQ(db.try_get_declare_byte_value(3).value(), 0xB4);
	CHECK_EQ(db.try_get_declare_byte_value(4).value(), 0xCB);
	CHECK_EQ(db.try_get_declare_byte_value(5).value(), 0xE3);
	CHECK_EQ(db.try_get_declare_byte_value(6).value(), 0x4D);
	CHECK_EQ(db.try_get_declare_byte_value(7).value(), 0xE4);
	CHECK_EQ(db.try_get_declare_byte_value(8).value(), 0x96);
	CHECK_EQ(db.try_get_declare_byte_value(9).value(), 0x98);
	CHECK_EQ(db.try_get_declare_byte_value(10).value(), 0xFD);
	CHECK_EQ(db.try_get_declare_byte_value(11).value(), 0x56);
	CHECK_EQ(db.try_get_declare_byte_value(12).value(), 0x82);
	CHECK_EQ(db.try_get_declare_byte_value(13).value(), 0x8D);
	CHECK_EQ(db.try_get_declare_byte_value(14).value(), 0x06);
	CHECK_EQ(db.try_get_declare_byte_value(15).value(), 0xC3);
}

TEST_CASE("encoder/declare_word_can_get_set") {
	Instruction dw = Instruction::with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08);
	dw.set_declare_word_value(0, 0xE2C5);
	dw.set_declare_word_value(1, 0xFAB4);
	dw.set_declare_word_value(2, 0xCBE3);
	dw.set_declare_word_value(3, 0x4DE4);
	dw.set_declare_word_value(4, 0x9698);
	dw.set_declare_word_value(5, 0xFD56);
	dw.set_declare_word_value(6, 0x828D);
	dw.set_declare_word_value(7, 0x06C3);
	CHECK_EQ(dw.get_declare_word_value(0), 0xE2C5);
	CHECK_EQ(dw.get_declare_word_value(1), 0xFAB4);
	CHECK_EQ(dw.get_declare_word_value(2), 0xCBE3);
	CHECK_EQ(dw.get_declare_word_value(3), 0x4DE4);
	CHECK_EQ(dw.get_declare_word_value(4), 0x9698);
	CHECK_EQ(dw.get_declare_word_value(5), 0xFD56);
	CHECK_EQ(dw.get_declare_word_value(6), 0x828D);
	CHECK_EQ(dw.get_declare_word_value(7), 0x06C3);
}

TEST_CASE("encoder/declare_word_can_get_set_rev") {
	Instruction dw = Instruction::with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08);
	dw.set_declare_word_value(7, 0x06C3);
	dw.set_declare_word_value(6, 0x828D);
	dw.set_declare_word_value(5, 0xFD56);
	dw.set_declare_word_value(4, 0x9698);
	dw.set_declare_word_value(3, 0x4DE4);
	dw.set_declare_word_value(2, 0xCBE3);
	dw.set_declare_word_value(1, 0xFAB4);
	dw.set_declare_word_value(0, 0xE2C5);
	CHECK_EQ(dw.get_declare_word_value(0), 0xE2C5);
	CHECK_EQ(dw.get_declare_word_value(1), 0xFAB4);
	CHECK_EQ(dw.get_declare_word_value(2), 0xCBE3);
	CHECK_EQ(dw.get_declare_word_value(3), 0x4DE4);
	CHECK_EQ(dw.get_declare_word_value(4), 0x9698);
	CHECK_EQ(dw.get_declare_word_value(5), 0xFD56);
	CHECK_EQ(dw.get_declare_word_value(6), 0x828D);
	CHECK_EQ(dw.get_declare_word_value(7), 0x06C3);
}

TEST_CASE("encoder/try_declare_word_can_get_set") {
	Instruction dw = Instruction::try_with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08).value();
	dw.try_set_declare_word_value(0, 0xE2C5).value();
	dw.try_set_declare_word_value(1, 0xFAB4).value();
	dw.try_set_declare_word_value(2, 0xCBE3).value();
	dw.try_set_declare_word_value(3, 0x4DE4).value();
	dw.try_set_declare_word_value(4, 0x9698).value();
	dw.try_set_declare_word_value(5, 0xFD56).value();
	dw.try_set_declare_word_value(6, 0x828D).value();
	dw.try_set_declare_word_value(7, 0x06C3).value();
	CHECK_EQ(dw.try_get_declare_word_value(0).value(), 0xE2C5);
	CHECK_EQ(dw.try_get_declare_word_value(1).value(), 0xFAB4);
	CHECK_EQ(dw.try_get_declare_word_value(2).value(), 0xCBE3);
	CHECK_EQ(dw.try_get_declare_word_value(3).value(), 0x4DE4);
	CHECK_EQ(dw.try_get_declare_word_value(4).value(), 0x9698);
	CHECK_EQ(dw.try_get_declare_word_value(5).value(), 0xFD56);
	CHECK_EQ(dw.try_get_declare_word_value(6).value(), 0x828D);
	CHECK_EQ(dw.try_get_declare_word_value(7).value(), 0x06C3);
}

TEST_CASE("encoder/try_declare_word_can_get_set_rev") {
	Instruction dw = Instruction::try_with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08).value();
	dw.try_set_declare_word_value(7, 0x06C3).value();
	dw.try_set_declare_word_value(6, 0x828D).value();
	dw.try_set_declare_word_value(5, 0xFD56).value();
	dw.try_set_declare_word_value(4, 0x9698).value();
	dw.try_set_declare_word_value(3, 0x4DE4).value();
	dw.try_set_declare_word_value(2, 0xCBE3).value();
	dw.try_set_declare_word_value(1, 0xFAB4).value();
	dw.try_set_declare_word_value(0, 0xE2C5).value();
	CHECK_EQ(dw.try_get_declare_word_value(0).value(), 0xE2C5);
	CHECK_EQ(dw.try_get_declare_word_value(1).value(), 0xFAB4);
	CHECK_EQ(dw.try_get_declare_word_value(2).value(), 0xCBE3);
	CHECK_EQ(dw.try_get_declare_word_value(3).value(), 0x4DE4);
	CHECK_EQ(dw.try_get_declare_word_value(4).value(), 0x9698);
	CHECK_EQ(dw.try_get_declare_word_value(5).value(), 0xFD56);
	CHECK_EQ(dw.try_get_declare_word_value(6).value(), 0x828D);
	CHECK_EQ(dw.try_get_declare_word_value(7).value(), 0x06C3);
}

TEST_CASE("encoder/declare_dword_can_get_set") {
	Instruction dd = Instruction::with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08);
	dd.set_declare_dword_value(0, 0xE2C5'FAB4);
	dd.set_declare_dword_value(1, 0xCBE3'4DE4);
	dd.set_declare_dword_value(2, 0x9698'FD56);
	dd.set_declare_dword_value(3, 0x828D'06C3);
	CHECK_EQ(dd.get_declare_dword_value(0), 0xE2C5'FAB4);
	CHECK_EQ(dd.get_declare_dword_value(1), 0xCBE3'4DE4);
	CHECK_EQ(dd.get_declare_dword_value(2), 0x9698'FD56);
	CHECK_EQ(dd.get_declare_dword_value(3), 0x828D'06C3);
}

TEST_CASE("encoder/declare_dword_can_get_set_rev") {
	Instruction dd = Instruction::with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08);
	dd.set_declare_dword_value(3, 0x828D'06C3);
	dd.set_declare_dword_value(2, 0x9698'FD56);
	dd.set_declare_dword_value(1, 0xCBE3'4DE4);
	dd.set_declare_dword_value(0, 0xE2C5'FAB4);
	CHECK_EQ(dd.get_declare_dword_value(0), 0xE2C5'FAB4);
	CHECK_EQ(dd.get_declare_dword_value(1), 0xCBE3'4DE4);
	CHECK_EQ(dd.get_declare_dword_value(2), 0x9698'FD56);
	CHECK_EQ(dd.get_declare_dword_value(3), 0x828D'06C3);
}

TEST_CASE("encoder/try_declare_dword_can_get_set") {
	Instruction dd = Instruction::try_with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08).value();
	dd.try_set_declare_dword_value(0, 0xE2C5'FAB4).value();
	dd.try_set_declare_dword_value(1, 0xCBE3'4DE4).value();
	dd.try_set_declare_dword_value(2, 0x9698'FD56).value();
	dd.try_set_declare_dword_value(3, 0x828D'06C3).value();
	CHECK_EQ(dd.try_get_declare_dword_value(0).value(), 0xE2C5'FAB4);
	CHECK_EQ(dd.try_get_declare_dword_value(1).value(), 0xCBE3'4DE4);
	CHECK_EQ(dd.try_get_declare_dword_value(2).value(), 0x9698'FD56);
	CHECK_EQ(dd.try_get_declare_dword_value(3).value(), 0x828D'06C3);
}

TEST_CASE("encoder/try_declare_dword_can_get_set_rev") {
	Instruction dd = Instruction::try_with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08).value();
	dd.try_set_declare_dword_value(3, 0x828D'06C3).value();
	dd.try_set_declare_dword_value(2, 0x9698'FD56).value();
	dd.try_set_declare_dword_value(1, 0xCBE3'4DE4).value();
	dd.try_set_declare_dword_value(0, 0xE2C5'FAB4).value();
	CHECK_EQ(dd.try_get_declare_dword_value(0).value(), 0xE2C5'FAB4);
	CHECK_EQ(dd.try_get_declare_dword_value(1).value(), 0xCBE3'4DE4);
	CHECK_EQ(dd.try_get_declare_dword_value(2).value(), 0x9698'FD56);
	CHECK_EQ(dd.try_get_declare_dword_value(3).value(), 0x828D'06C3);
}

TEST_CASE("encoder/declare_qword_can_get_set") {
	Instruction dq = Instruction::with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08);
	dq.set_declare_qword_value(0, 0xE2C5'FAB4'CBE3'4DE4);
	dq.set_declare_qword_value(1, 0x9698'FD56'828D'06C3);
	CHECK_EQ(dq.get_declare_qword_value(0), 0xE2C5'FAB4'CBE3'4DE4);
	CHECK_EQ(dq.get_declare_qword_value(1), 0x9698'FD56'828D'06C3);
}

TEST_CASE("encoder/declare_qword_can_get_set_rev") {
	Instruction dq = Instruction::with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08);
	dq.set_declare_qword_value(1, 0x9698'FD56'828D'06C3);
	dq.set_declare_qword_value(0, 0xE2C5'FAB4'CBE3'4DE4);
	CHECK_EQ(dq.get_declare_qword_value(0), 0xE2C5'FAB4'CBE3'4DE4);
	CHECK_EQ(dq.get_declare_qword_value(1), 0x9698'FD56'828D'06C3);
}

TEST_CASE("encoder/try_declare_qword_can_get_set") {
	Instruction dq = Instruction::try_with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08).value();
	dq.try_set_declare_qword_value(0, 0xE2C5'FAB4'CBE3'4DE4).value();
	dq.try_set_declare_qword_value(1, 0x9698'FD56'828D'06C3).value();
	CHECK_EQ(dq.try_get_declare_qword_value(0).value(), 0xE2C5'FAB4'CBE3'4DE4);
	CHECK_EQ(dq.try_get_declare_qword_value(1).value(), 0x9698'FD56'828D'06C3);
}

TEST_CASE("encoder/try_declare_qword_can_get_set_rev") {
	Instruction dq = Instruction::try_with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08).value();
	dq.try_set_declare_qword_value(1, 0x9698'FD56'828D'06C3).value();
	dq.try_set_declare_qword_value(0, 0xE2C5'FAB4'CBE3'4DE4).value();
	CHECK_EQ(dq.try_get_declare_qword_value(0).value(), 0xE2C5'FAB4'CBE3'4DE4);
	CHECK_EQ(dq.try_get_declare_qword_value(1).value(), 0x9698'FD56'828D'06C3);
}

TEST_CASE("encoder/declare_data_does_not_use_other_properties") {
	const auto verify = [](const Instruction& instr) {
		CHECK_EQ(instr.segment_prefix(), Register::None);
		CHECK_EQ(instr.code_size(), CodeSize::Unknown);
		CHECK_EQ(instr.rounding_control(), RoundingControl::None);
		CHECK_EQ(instr.ip(), UINT64_C(0));
		CHECK(!instr.is_broadcast());
		CHECK(!instr.has_op_mask());
		CHECK(!instr.suppress_all_exceptions());
		CHECK(!instr.zeroing_masking());
		CHECK(!instr.has_xacquire_prefix());
		CHECK(!instr.has_xrelease_prefix());
		CHECK(!instr.has_rep_prefix());
		CHECK(!instr.has_repe_prefix());
		CHECK(!instr.has_repne_prefix());
		CHECK(!instr.has_lock_prefix());
	};

	const std::vector<std::uint8_t> data(16, 0xFF);

	verify(Instruction::with_declare_byte(data.data(), data.size()).value());
	verify(Instruction::with_declare_word_slice_u8(data.data(), data.size()).value());
	verify(Instruction::with_declare_dword_slice_u8(data.data(), data.size()).value());
	verify(Instruction::with_declare_qword_slice_u8(data.data(), data.size()).value());
}

TEST_CASE("encoder/with_declare_byte") {
	Instruction instr = Instruction::with_declare_byte_1(0x77);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(1));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77}));

	instr = Instruction::with_declare_byte_2(0x77, 0xA9);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(2));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9}));

	instr = Instruction::with_declare_byte_3(0x77, 0xA9, 0xCE);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(3));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE}));

	instr = Instruction::with_declare_byte_4(0x77, 0xA9, 0xCE, 0x9D);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(4));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D}));

	instr = Instruction::with_declare_byte_5(0x77, 0xA9, 0xCE, 0x9D, 0x55);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(5));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55}));

	instr = Instruction::with_declare_byte_6(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(6));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05}));

	instr = Instruction::with_declare_byte_7(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(7));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42}));

	instr = Instruction::with_declare_byte_8(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(8));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C}));

	instr = Instruction::with_declare_byte_9(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(9));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86}));

	instr = Instruction::with_declare_byte_10(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(10));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32}));

	instr = Instruction::with_declare_byte_11(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(11));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE}));

	instr = Instruction::with_declare_byte_12(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(12));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F}));

	instr = Instruction::with_declare_byte_13(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(13));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34}));

	instr = Instruction::with_declare_byte_14(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(14));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27}));

	instr = Instruction::with_declare_byte_15(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(15));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA}));

	instr = Instruction::with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08);
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(16));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08}));
}

TEST_CASE("encoder/try_with_declare_byte") {
	Instruction instr = Instruction::try_with_declare_byte_1(0x77).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(1));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77}));

	instr = Instruction::try_with_declare_byte_2(0x77, 0xA9).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(2));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9}));

	instr = Instruction::try_with_declare_byte_3(0x77, 0xA9, 0xCE).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(3));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE}));

	instr = Instruction::try_with_declare_byte_4(0x77, 0xA9, 0xCE, 0x9D).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(4));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D}));

	instr = Instruction::try_with_declare_byte_5(0x77, 0xA9, 0xCE, 0x9D, 0x55).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(5));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55}));

	instr = Instruction::try_with_declare_byte_6(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(6));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05}));

	instr = Instruction::try_with_declare_byte_7(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(7));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42}));

	instr = Instruction::try_with_declare_byte_8(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(8));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C}));

	instr = Instruction::try_with_declare_byte_9(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(9));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86}));

	instr = Instruction::try_with_declare_byte_10(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(10));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32}));

	instr = Instruction::try_with_declare_byte_11(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(11));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE}));

	instr = Instruction::try_with_declare_byte_12(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(12));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F}));

	instr = Instruction::try_with_declare_byte_13(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(13));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34}));

	instr = Instruction::try_with_declare_byte_14(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(14));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27}));

	instr = 
		Instruction::try_with_declare_byte_15(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA).value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(15));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA}));

	instr = Instruction::try_with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08)
		.value();
	CHECK_EQ(instr.code(), Code::DeclareByte);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(16));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08}));
}

TEST_CASE("encoder/with_declare_word") {
	Instruction instr = Instruction::with_declare_word_1(0x77A9);
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(1));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77}));

	instr = Instruction::with_declare_word_2(0x77A9, 0xCE9D);
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(2));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE}));

	instr = Instruction::with_declare_word_3(0x77A9, 0xCE9D, 0x5505);
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(3));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55}));

	instr = Instruction::with_declare_word_4(0x77A9, 0xCE9D, 0x5505, 0x426C);
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(4));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42}));

	instr = Instruction::with_declare_word_5(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632);
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(5));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86}));

	instr = Instruction::with_declare_word_6(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F);
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(6));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86, 0x4F, 0xFE}));

	instr = Instruction::with_declare_word_7(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427);
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(7));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86, 0x4F, 0xFE, 0x27, 0x34}));

	instr = Instruction::with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08);
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(8));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86, 0x4F, 0xFE, 0x27, 0x34, 0x08, 0xAA}));
}

TEST_CASE("encoder/try_with_declare_word") {
	Instruction instr = Instruction::try_with_declare_word_1(0x77A9).value();
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(1));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77}));

	instr = Instruction::try_with_declare_word_2(0x77A9, 0xCE9D).value();
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(2));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE}));

	instr = Instruction::try_with_declare_word_3(0x77A9, 0xCE9D, 0x5505).value();
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(3));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55}));

	instr = Instruction::try_with_declare_word_4(0x77A9, 0xCE9D, 0x5505, 0x426C).value();
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(4));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42}));

	instr = Instruction::try_with_declare_word_5(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632).value();
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(5));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86}));

	instr = Instruction::try_with_declare_word_6(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F).value();
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(6));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86, 0x4F, 0xFE}));

	instr = Instruction::try_with_declare_word_7(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427).value();
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(7));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86, 0x4F, 0xFE, 0x27, 0x34}));

	instr = Instruction::try_with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08).value();
	CHECK_EQ(instr.code(), Code::DeclareWord);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(8));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86, 0x4F, 0xFE, 0x27, 0x34, 0x08, 0xAA}));
}

TEST_CASE("encoder/with_declare_dword") {
	Instruction instr = Instruction::with_declare_dword_1(0x77A9'CE9D);
	CHECK_EQ(instr.code(), Code::DeclareDword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(1));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x9D, 0xCE, 0xA9, 0x77}));

	instr = Instruction::with_declare_dword_2(0x77A9'CE9D, 0x5505'426C);
	CHECK_EQ(instr.code(), Code::DeclareDword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(2));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x9D, 0xCE, 0xA9, 0x77, 0x6C, 0x42, 0x05, 0x55}));

	instr = Instruction::with_declare_dword_3(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F);
	CHECK_EQ(instr.code(), Code::DeclareDword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(3));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x9D, 0xCE, 0xA9, 0x77, 0x6C, 0x42, 0x05, 0x55, 0x4F, 0xFE, 0x32, 0x86}));

	instr = Instruction::with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08);
	CHECK_EQ(instr.code(), Code::DeclareDword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(4));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x9D, 0xCE, 0xA9, 0x77, 0x6C, 0x42, 0x05, 0x55, 0x4F, 0xFE, 0x32, 0x86, 0x08, 0xAA, 0x27, 0x34}));
}

TEST_CASE("encoder/try_with_declare_dword") {
	Instruction instr = Instruction::try_with_declare_dword_1(0x77A9'CE9D).value();
	CHECK_EQ(instr.code(), Code::DeclareDword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(1));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x9D, 0xCE, 0xA9, 0x77}));

	instr = Instruction::try_with_declare_dword_2(0x77A9'CE9D, 0x5505'426C).value();
	CHECK_EQ(instr.code(), Code::DeclareDword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(2));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x9D, 0xCE, 0xA9, 0x77, 0x6C, 0x42, 0x05, 0x55}));

	instr = Instruction::try_with_declare_dword_3(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F).value();
	CHECK_EQ(instr.code(), Code::DeclareDword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(3));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x9D, 0xCE, 0xA9, 0x77, 0x6C, 0x42, 0x05, 0x55, 0x4F, 0xFE, 0x32, 0x86}));

	instr = Instruction::try_with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08).value();
	CHECK_EQ(instr.code(), Code::DeclareDword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(4));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x9D, 0xCE, 0xA9, 0x77, 0x6C, 0x42, 0x05, 0x55, 0x4F, 0xFE, 0x32, 0x86, 0x08, 0xAA, 0x27, 0x34}));
}

TEST_CASE("encoder/with_declare_qword") {
	Instruction instr = Instruction::with_declare_qword_1(0x77A9'CE9D'5505'426C);
	CHECK_EQ(instr.code(), Code::DeclareQword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(1));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x6C, 0x42, 0x05, 0x55, 0x9D, 0xCE, 0xA9, 0x77}));

	instr = Instruction::with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08);
	CHECK_EQ(instr.code(), Code::DeclareQword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(2));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x6C, 0x42, 0x05, 0x55, 0x9D, 0xCE, 0xA9, 0x77, 0x08, 0xAA, 0x27, 0x34, 0x4F, 0xFE, 0x32, 0x86}));
}

TEST_CASE("encoder/try_with_declare_qword") {
	Instruction instr = Instruction::try_with_declare_qword_1(0x77A9'CE9D'5505'426C).value();
	CHECK_EQ(instr.code(), Code::DeclareQword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(1));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x6C, 0x42, 0x05, 0x55, 0x9D, 0xCE, 0xA9, 0x77}));

	instr = Instruction::try_with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08).value();
	CHECK_EQ(instr.code(), Code::DeclareQword);
	CHECK_EQ(instr.declare_data_len(), static_cast<std::size_t>(2));
	CHECK((get_data(instr) == std::vector<std::uint8_t>{0x6C, 0x42, 0x05, 0x55, 0x9D, 0xCE, 0xA9, 0x77, 0x08, 0xAA, 0x27, 0x34, 0x4F, 0xFE, 0x32, 0x86}));
}

TEST_CASE("encoder/with_declare_byte_slice") {
	const std::vector<std::pair<Instruction, std::vector<std::uint8_t>>> tests = {
		{Instruction::with_declare_byte_1(0x77), {0x77}},
		{Instruction::with_declare_byte_2(0x77, 0xA9), {0x77, 0xA9}},
		{Instruction::with_declare_byte_3(0x77, 0xA9, 0xCE), {0x77, 0xA9, 0xCE}},
		{Instruction::with_declare_byte_4(0x77, 0xA9, 0xCE, 0x9D), {0x77, 0xA9, 0xCE, 0x9D}},
		{Instruction::with_declare_byte_5(0x77, 0xA9, 0xCE, 0x9D, 0x55), {0x77, 0xA9, 0xCE, 0x9D, 0x55}},
		{Instruction::with_declare_byte_6(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05}},
		{Instruction::with_declare_byte_7(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42}},
		{Instruction::with_declare_byte_8(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C}},
		{Instruction::with_declare_byte_9(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86}},
		{Instruction::with_declare_byte_10(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32}},
		{Instruction::with_declare_byte_11(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE}},
		{Instruction::with_declare_byte_12(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F}},
		{Instruction::with_declare_byte_13(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34}},
		{Instruction::with_declare_byte_14(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27}},
		{Instruction::with_declare_byte_15(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA}},
		{Instruction::with_declare_byte_16(0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08), {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08}},
	};
	for (const auto& [instr1, data] : tests) {
		Instruction instr2 = Instruction::with_declare_byte(data.data(), data.size()).value();
		CHECK(instr1.eq_all_bits(instr2));
	}
}

TEST_CASE("encoder/with_declare_word_slice") {
	const std::vector<std::pair<Instruction, std::vector<std::uint8_t>>> tests = {
		{Instruction::with_declare_word_1(0x77A9), {0xA9, 0x77}},
		{Instruction::with_declare_word_2(0x77A9, 0xCE9D), {0xA9, 0x77, 0x9D, 0xCE}},
		{Instruction::with_declare_word_3(0x77A9, 0xCE9D, 0x5505), {0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55}},
		{Instruction::with_declare_word_4(0x77A9, 0xCE9D, 0x5505, 0x426C), {0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42}},
		{Instruction::with_declare_word_5(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632), {0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86}},
		{Instruction::with_declare_word_6(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F), {0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86, 0x4F, 0xFE}},
		{Instruction::with_declare_word_7(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427), {0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86, 0x4F, 0xFE, 0x27, 0x34}},
		{Instruction::with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08), {0xA9, 0x77, 0x9D, 0xCE, 0x05, 0x55, 0x6C, 0x42, 0x32, 0x86, 0x4F, 0xFE, 0x27, 0x34, 0x08, 0xAA}},
	};
	for (const auto& [instr1, data] : tests) {
		Instruction instr2 = Instruction::with_declare_word_slice_u8(data.data(), data.size()).value();
		CHECK(instr1.eq_all_bits(instr2));
	}
}

TEST_CASE("encoder/with_declare_dword_slice") {
	const std::vector<std::pair<Instruction, std::vector<std::uint8_t>>> tests = {
		{Instruction::with_declare_dword_1(0x77A9'CE9D), {0x9D, 0xCE, 0xA9, 0x77}},
		{Instruction::with_declare_dword_2(0x77A9'CE9D, 0x5505'426C), {0x9D, 0xCE, 0xA9, 0x77, 0x6C, 0x42, 0x05, 0x55}},
		{Instruction::with_declare_dword_3(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F), {0x9D, 0xCE, 0xA9, 0x77, 0x6C, 0x42, 0x05, 0x55, 0x4F, 0xFE, 0x32, 0x86}},
		{Instruction::with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08), {0x9D, 0xCE, 0xA9, 0x77, 0x6C, 0x42, 0x05, 0x55, 0x4F, 0xFE, 0x32, 0x86, 0x08, 0xAA, 0x27, 0x34}},
	};
	for (const auto& [instr1, data] : tests) {
		Instruction instr2 = Instruction::with_declare_dword_slice_u8(data.data(), data.size()).value();
		CHECK(instr1.eq_all_bits(instr2));
	}
}

TEST_CASE("encoder/with_declare_qword_slice") {
	const std::vector<std::pair<Instruction, std::vector<std::uint8_t>>> tests = {
		{Instruction::with_declare_qword_1(0x77A9'CE9D'5505'426C), {0x6C, 0x42, 0x05, 0x55, 0x9D, 0xCE, 0xA9, 0x77}},
		{Instruction::with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08), {0x6C, 0x42, 0x05, 0x55, 0x9D, 0xCE, 0xA9, 0x77, 0x08, 0xAA, 0x27, 0x34, 0x4F, 0xFE, 0x32, 0x86}},
	};
	for (const auto& [instr1, data] : tests) {
		Instruction instr2 = Instruction::with_declare_qword_slice_u8(data.data(), data.size()).value();
		CHECK(instr1.eq_all_bits(instr2));
	}
}

TEST_CASE("encoder/with_declare_word_slice2") {
	const std::vector<std::pair<Instruction, std::vector<std::uint16_t>>> tests = {
		{Instruction::with_declare_word_1(0x77A9), {0x77A9}},
		{Instruction::with_declare_word_2(0x77A9, 0xCE9D), {0x77A9, 0xCE9D}},
		{Instruction::with_declare_word_3(0x77A9, 0xCE9D, 0x5505), {0x77A9, 0xCE9D, 0x5505}},
		{Instruction::with_declare_word_4(0x77A9, 0xCE9D, 0x5505, 0x426C), {0x77A9, 0xCE9D, 0x5505, 0x426C}},
		{Instruction::with_declare_word_5(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632), {0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632}},
		{Instruction::with_declare_word_6(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F), {0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F}},
		{Instruction::with_declare_word_7(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427), {0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427}},
		{Instruction::with_declare_word_8(0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08), {0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08}},
	};
	for (const auto& [instr1, data] : tests) {
		Instruction instr2 = Instruction::with_declare_word(data.data(), data.size()).value();
		CHECK(instr1.eq_all_bits(instr2));
	}
}

TEST_CASE("encoder/with_declare_dword_slice2") {
	const std::vector<std::pair<Instruction, std::vector<std::uint32_t>>> tests = {
		{Instruction::with_declare_dword_1(0x77A9'CE9D), {0x77A9'CE9D}},
		{Instruction::with_declare_dword_2(0x77A9'CE9D, 0x5505'426C), {0x77A9'CE9D, 0x5505'426C}},
		{Instruction::with_declare_dword_3(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F), {0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F}},
		{Instruction::with_declare_dword_4(0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08), {0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08}},
	};
	for (const auto& [instr1, data] : tests) {
		Instruction instr2 = Instruction::with_declare_dword(data.data(), data.size()).value();
		CHECK(instr1.eq_all_bits(instr2));
	}
}

TEST_CASE("encoder/with_declare_qword_slice2") {
	const std::vector<std::pair<Instruction, std::vector<std::uint64_t>>> tests = {
		{Instruction::with_declare_qword_1(0x77A9'CE9D'5505'426C), {0x77A9'CE9D'5505'426C}},
		{Instruction::with_declare_qword_2(0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08), {0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08}},
	};
	for (const auto& [instr1, data] : tests) {
		Instruction instr2 = Instruction::with_declare_qword(data.data(), data.size()).value();
		CHECK(instr1.eq_all_bits(instr2));
	}
}

TEST_CASE("encoder/try_with_test") {
	const std::vector<WithTest> tests = {
		{64, "90", DecoderOptions::NONE, Instruction::with(Code::Nopd)},
		{64, "48B9FFFFFFFFFFFFFFFF", DecoderOptions::NONE, Instruction::with2(Code::Mov_r64_imm64, Register::RCX, -1).value()},
		{64, "48B9FFFFFFFFFFFFFFFF", DecoderOptions::NONE, Instruction::with2(Code::Mov_r64_imm64, Register::RCX, -1).value()},
		{64, "48B9123456789ABCDE31", DecoderOptions::NONE, Instruction::with2(Code::Mov_r64_imm64, Register::RCX, static_cast<std::uint64_t>(0x31DE'BC9A'7856'3412)).value()},
		{64, "48B9FFFFFFFF00000000", DecoderOptions::NONE, Instruction::with2(Code::Mov_r64_imm64, Register::RCX, static_cast<std::uint32_t>(0xFFFF'FFFF)).value()},
		{64, "8FC1", DecoderOptions::NONE, Instruction::with1(Code::Pop_rm64, Register::RCX).value()},
		{64, "648F847501EFCDAB", DecoderOptions::NONE, Instruction::with1(Code::Pop_rm64, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS)).value()},
		{64, "C6F85A", DecoderOptions::NONE, Instruction::with1(Code::Xabort_imm8, 0x5A).value()},
		{64, "66685AA5", DecoderOptions::NONE, Instruction::with1(Code::Push_imm16, 0xA55A).value()},
		{32, "685AA51234", DecoderOptions::NONE, Instruction::with1(Code::Pushd_imm32, 0x3412'A55A).value()},
		{64, "666A5A", DecoderOptions::NONE, Instruction::with1(Code::Pushw_imm8, 0x5A).value()},
		{32, "6A5A", DecoderOptions::NONE, Instruction::with1(Code::Pushd_imm8, 0x5A).value()},
		{64, "6A5A", DecoderOptions::NONE, Instruction::with1(Code::Pushq_imm8, 0x5A).value()},
		{64, "685AA512A4", DecoderOptions::NONE, Instruction::with1(Code::Pushq_imm32, -0x5BED'5AA6).value()},
		{32, "66705A", DecoderOptions::NONE, Instruction::with_branch(Code::Jo_rel8_16, 0x4D).value()},
		{32, "705A", DecoderOptions::NONE, Instruction::with_branch(Code::Jo_rel8_32, 0x8000'004C).value()},
		{64, "705A", DecoderOptions::NONE, Instruction::with_branch(Code::Jo_rel8_64, 0x8000'0000'0000'004C).value()},
		{32, "669A12345678", DecoderOptions::NONE, Instruction::with_far_branch(Code::Call_ptr1616, 0x7856, 0x3412).value()},
		{32, "9A123456789ABC", DecoderOptions::NONE, Instruction::with_far_branch(Code::Call_ptr1632, 0xBC9A, 0x7856'3412).value()},
		{16, "C7F85AA5", DecoderOptions::NONE, Instruction::with_xbegin(16, 0x254E).value()},
		{32, "C7F85AA51234", DecoderOptions::NONE, Instruction::with_xbegin(32, 0xB412'A550).value()},
		{64, "C7F85AA51234", DecoderOptions::NONE, Instruction::with_xbegin(64, 0x8000'0000'3412'A550).value()},
		{64, "00D1", DecoderOptions::NONE, Instruction::with2(Code::Add_rm8_r8, Register::CL, Register::DL).value()},
		{64, "64028C7501EFCDAB", DecoderOptions::NONE, Instruction::with2(Code::Add_r8_rm8, Register::CL, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS)).value()},
		{64, "80C15A", DecoderOptions::NONE, Instruction::with2(Code::Add_rm8_imm8, Register::CL, 0x5A).value()},
		{64, "6681C15AA5", DecoderOptions::NONE, Instruction::with2(Code::Add_rm16_imm16, Register::CX, 0xA55A).value()},
		{64, "81C15AA51234", DecoderOptions::NONE, Instruction::with2(Code::Add_rm32_imm32, Register::ECX, 0x3412'A55A).value()},
		{64, "48B904152637A55A5678", DecoderOptions::NONE, Instruction::with2(Code::Mov_r64_imm64, Register::RCX, static_cast<std::uint64_t>(0x7856'5AA5'3726'1504)).value()},
		{64, "6683C15A", DecoderOptions::NONE, Instruction::with2(Code::Add_rm16_imm8, Register::CX, 0x5A).value()},
		{64, "83C15A", DecoderOptions::NONE, Instruction::with2(Code::Add_rm32_imm8, Register::ECX, 0x5A).value()},
		{64, "4883C15A", DecoderOptions::NONE, Instruction::with2(Code::Add_rm64_imm8, Register::RCX, 0x5A).value()},
		{64, "4881C15AA51234", DecoderOptions::NONE, Instruction::with2(Code::Add_rm64_imm32, Register::RCX, 0x3412'A55A).value()},
		{64, "64A0123456789ABCDEF0", DecoderOptions::NONE, Instruction::with2(Code::Mov_AL_moffs8, Register::AL, MemoryOperand::with_base_displ_size_bcst_seg(Register::None, static_cast<std::int64_t>(static_cast<std::uint64_t>(0xF0DE'BC9A'7856'3412)), 8, false, Register::FS)).value()},
		{64, "6400947501EFCDAB", DecoderOptions::NONE, Instruction::with2(Code::Add_rm8_r8, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), Register::DL).value()},
		{64, "6480847501EFCDAB5A", DecoderOptions::NONE, Instruction::with2(Code::Add_rm8_imm8, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x5A).value()},
		{64, "646681847501EFCDAB5AA5", DecoderOptions::NONE, Instruction::with2(Code::Add_rm16_imm16, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0xA55A).value()},
		{64, "6481847501EFCDAB5AA51234", DecoderOptions::NONE, Instruction::with2(Code::Add_rm32_imm32, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x3412'A55A).value()},
		{64, "646683847501EFCDAB5A", DecoderOptions::NONE, Instruction::with2(Code::Add_rm16_imm8, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x5A).value()},
		{64, "6483847501EFCDAB5A", DecoderOptions::NONE, Instruction::with2(Code::Add_rm32_imm8, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x5A).value()},
		{64, "644883847501EFCDAB5A", DecoderOptions::NONE, Instruction::with2(Code::Add_rm64_imm8, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x5A).value()},
		{64, "644881847501EFCDAB5AA51234", DecoderOptions::NONE, Instruction::with2(Code::Add_rm64_imm32, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x3412'A55A).value()},
		{64, "E65A", DecoderOptions::NONE, Instruction::with2(Code::Out_imm8_AL, 0x5A, Register::AL).value()},
		{64, "E65A", DecoderOptions::NONE, Instruction::with2(Code::Out_imm8_AL, 0x5A, Register::AL).value()},
		{64, "66C85AA5A6", DecoderOptions::NONE, Instruction::with2(Code::Enterw_imm16_imm8, 0xA55A, 0xA6).value()},
		{64, "66C85AA5A6", DecoderOptions::NONE, Instruction::with2(Code::Enterw_imm16_imm8, 0xA55A, 0xA6).value()},
		{64, "64A2123456789ABCDEF0", DecoderOptions::NONE, Instruction::with2(Code::Mov_moffs8_AL, MemoryOperand::with_base_displ_size_bcst_seg(Register::None, static_cast<std::int64_t>(static_cast<std::uint64_t>(0xF0DE'BC9A'7856'3412)), 8, false, Register::FS), Register::AL).value()},
		{64, "6669CAA55A", DecoderOptions::NONE, Instruction::with3(Code::Imul_r16_rm16_imm16, Register::CX, Register::DX, 0x5AA5).value()},
		{64, "69CA5AA51234", DecoderOptions::NONE, Instruction::with3(Code::Imul_r32_rm32_imm32, Register::ECX, Register::EDX, 0x3412'A55A).value()},
		{64, "666BCA5A", DecoderOptions::NONE, Instruction::with3(Code::Imul_r16_rm16_imm8, Register::CX, Register::DX, 0x5A).value()},
		{64, "6BCA5A", DecoderOptions::NONE, Instruction::with3(Code::Imul_r32_rm32_imm8, Register::ECX, Register::EDX, 0x5A).value()},
		{64, "486BCA5A", DecoderOptions::NONE, Instruction::with3(Code::Imul_r64_rm64_imm8, Register::RCX, Register::RDX, 0x5A).value()},
		{64, "4869CA5AA512A4", DecoderOptions::NONE, Instruction::with3(Code::Imul_r64_rm64_imm32, Register::RCX, Register::RDX, -0x5BED'5AA6).value()},
		{64, "6466698C7501EFCDAB5AA5", DecoderOptions::NONE, Instruction::with3(Code::Imul_r16_rm16_imm16, Register::CX, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0xA55A).value()},
		{64, "64698C7501EFCDAB5AA51234", DecoderOptions::NONE, Instruction::with3(Code::Imul_r32_rm32_imm32, Register::ECX, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x3412'A55A).value()},
		{64, "64666B8C7501EFCDAB5A", DecoderOptions::NONE, Instruction::with3(Code::Imul_r16_rm16_imm8, Register::CX, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x5A).value()},
		{64, "646B8C7501EFCDAB5A", DecoderOptions::NONE, Instruction::with3(Code::Imul_r32_rm32_imm8, Register::ECX, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x5A).value()},
		{64, "64486B8C7501EFCDAB5A", DecoderOptions::NONE, Instruction::with3(Code::Imul_r64_rm64_imm8, Register::RCX, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x5A).value()},
		{64, "6448698C7501EFCDAB5AA512A4", DecoderOptions::NONE, Instruction::with3(Code::Imul_r64_rm64_imm32, Register::RCX, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), -0x5BED'5AA6).value()},
		{64, "660F78C1A5FD", DecoderOptions::NONE, Instruction::with3(Code::Extrq_xmm_imm8_imm8, Register::XMM1, 0xA5, 0xFD).value()},
		{64, "660F78C1A5FD", DecoderOptions::NONE, Instruction::with3(Code::Extrq_xmm_imm8_imm8, Register::XMM1, 0xA5, 0xFD).value()},
		{64, "64660FA4947501EFCDAB5A", DecoderOptions::NONE, Instruction::with3(Code::Shld_rm16_r16_imm8, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), Register::DX, 0x5A).value()},
		{64, "64660FA4947501EFCDAB5A", DecoderOptions::NONE, Instruction::with3(Code::Shld_rm16_r16_imm8, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), Register::DX, 0x5A).value()},
		{64, "F20F78CAA5FD", DecoderOptions::NONE, Instruction::with4(Code::Insertq_xmm_xmm_imm8_imm8, Register::XMM1, Register::XMM2, 0xA5, 0xFD).value()},
		{64, "F20F78CAA5FD", DecoderOptions::NONE, Instruction::with4(Code::Insertq_xmm_xmm_imm8_imm8, Register::XMM1, Register::XMM2, 0xA5, 0xFD).value()},
		{16, "0FB855AA", DecoderOptions::JMPE, Instruction::with_branch(Code::Jmpe_disp16, 0xAA55).value()},
		{32, "0FB8123455AA", DecoderOptions::JMPE, Instruction::with_branch(Code::Jmpe_disp32, 0xAA55'3412).value()},
		{32, "64676E", DecoderOptions::NONE, Instruction::with_outsb(16, Register::FS, RepPrefixKind::None).value()},
		{64, "64676E", DecoderOptions::NONE, Instruction::with_outsb(32, Register::FS, RepPrefixKind::None).value()},
		{64, "646E", DecoderOptions::NONE, Instruction::with_outsb(64, Register::FS, RepPrefixKind::None).value()},
		{32, "6466676F", DecoderOptions::NONE, Instruction::with_outsw(16, Register::FS, RepPrefixKind::None).value()},
		{64, "6466676F", DecoderOptions::NONE, Instruction::with_outsw(32, Register::FS, RepPrefixKind::None).value()},
		{64, "64666F", DecoderOptions::NONE, Instruction::with_outsw(64, Register::FS, RepPrefixKind::None).value()},
		{32, "64676F", DecoderOptions::NONE, Instruction::with_outsd(16, Register::FS, RepPrefixKind::None).value()},
		{64, "64676F", DecoderOptions::NONE, Instruction::with_outsd(32, Register::FS, RepPrefixKind::None).value()},
		{64, "646F", DecoderOptions::NONE, Instruction::with_outsd(64, Register::FS, RepPrefixKind::None).value()},
		{32, "67AE", DecoderOptions::NONE, Instruction::with_scasb(16, RepPrefixKind::None).value()},
		{64, "67AE", DecoderOptions::NONE, Instruction::with_scasb(32, RepPrefixKind::None).value()},
		{64, "AE", DecoderOptions::NONE, Instruction::with_scasb(64, RepPrefixKind::None).value()},
		{32, "6667AF", DecoderOptions::NONE, Instruction::with_scasw(16, RepPrefixKind::None).value()},
		{64, "6667AF", DecoderOptions::NONE, Instruction::with_scasw(32, RepPrefixKind::None).value()},
		{64, "66AF", DecoderOptions::NONE, Instruction::with_scasw(64, RepPrefixKind::None).value()},
		{32, "67AF", DecoderOptions::NONE, Instruction::with_scasd(16, RepPrefixKind::None).value()},
		{64, "67AF", DecoderOptions::NONE, Instruction::with_scasd(32, RepPrefixKind::None).value()},
		{64, "AF", DecoderOptions::NONE, Instruction::with_scasd(64, RepPrefixKind::None).value()},
		{64, "6748AF", DecoderOptions::NONE, Instruction::with_scasq(32, RepPrefixKind::None).value()},
		{64, "48AF", DecoderOptions::NONE, Instruction::with_scasq(64, RepPrefixKind::None).value()},
		{32, "6467AC", DecoderOptions::NONE, Instruction::with_lodsb(16, Register::FS, RepPrefixKind::None).value()},
		{64, "6467AC", DecoderOptions::NONE, Instruction::with_lodsb(32, Register::FS, RepPrefixKind::None).value()},
		{64, "64AC", DecoderOptions::NONE, Instruction::with_lodsb(64, Register::FS, RepPrefixKind::None).value()},
		{32, "646667AD", DecoderOptions::NONE, Instruction::with_lodsw(16, Register::FS, RepPrefixKind::None).value()},
		{64, "646667AD", DecoderOptions::NONE, Instruction::with_lodsw(32, Register::FS, RepPrefixKind::None).value()},
		{64, "6466AD", DecoderOptions::NONE, Instruction::with_lodsw(64, Register::FS, RepPrefixKind::None).value()},
		{32, "6467AD", DecoderOptions::NONE, Instruction::with_lodsd(16, Register::FS, RepPrefixKind::None).value()},
		{64, "6467AD", DecoderOptions::NONE, Instruction::with_lodsd(32, Register::FS, RepPrefixKind::None).value()},
		{64, "64AD", DecoderOptions::NONE, Instruction::with_lodsd(64, Register::FS, RepPrefixKind::None).value()},
		{64, "646748AD", DecoderOptions::NONE, Instruction::with_lodsq(32, Register::FS, RepPrefixKind::None).value()},
		{64, "6448AD", DecoderOptions::NONE, Instruction::with_lodsq(64, Register::FS, RepPrefixKind::None).value()},
		{32, "676C", DecoderOptions::NONE, Instruction::with_insb(16, RepPrefixKind::None).value()},
		{64, "676C", DecoderOptions::NONE, Instruction::with_insb(32, RepPrefixKind::None).value()},
		{64, "6C", DecoderOptions::NONE, Instruction::with_insb(64, RepPrefixKind::None).value()},
		{32, "66676D", DecoderOptions::NONE, Instruction::with_insw(16, RepPrefixKind::None).value()},
		{64, "66676D", DecoderOptions::NONE, Instruction::with_insw(32, RepPrefixKind::None).value()},
		{64, "666D", DecoderOptions::NONE, Instruction::with_insw(64, RepPrefixKind::None).value()},
		{32, "676D", DecoderOptions::NONE, Instruction::with_insd(16, RepPrefixKind::None).value()},
		{64, "676D", DecoderOptions::NONE, Instruction::with_insd(32, RepPrefixKind::None).value()},
		{64, "6D", DecoderOptions::NONE, Instruction::with_insd(64, RepPrefixKind::None).value()},
		{32, "67AA", DecoderOptions::NONE, Instruction::with_stosb(16, RepPrefixKind::None).value()},
		{64, "67AA", DecoderOptions::NONE, Instruction::with_stosb(32, RepPrefixKind::None).value()},
		{64, "AA", DecoderOptions::NONE, Instruction::with_stosb(64, RepPrefixKind::None).value()},
		{32, "6667AB", DecoderOptions::NONE, Instruction::with_stosw(16, RepPrefixKind::None).value()},
		{64, "6667AB", DecoderOptions::NONE, Instruction::with_stosw(32, RepPrefixKind::None).value()},
		{64, "66AB", DecoderOptions::NONE, Instruction::with_stosw(64, RepPrefixKind::None).value()},
		{32, "67AB", DecoderOptions::NONE, Instruction::with_stosd(16, RepPrefixKind::None).value()},
		{64, "67AB", DecoderOptions::NONE, Instruction::with_stosd(32, RepPrefixKind::None).value()},
		{64, "AB", DecoderOptions::NONE, Instruction::with_stosd(64, RepPrefixKind::None).value()},
		{64, "6748AB", DecoderOptions::NONE, Instruction::with_stosq(32, RepPrefixKind::None).value()},
		{64, "48AB", DecoderOptions::NONE, Instruction::with_stosq(64, RepPrefixKind::None).value()},
		{32, "6467A6", DecoderOptions::NONE, Instruction::with_cmpsb(16, Register::FS, RepPrefixKind::None).value()},
		{64, "6467A6", DecoderOptions::NONE, Instruction::with_cmpsb(32, Register::FS, RepPrefixKind::None).value()},
		{64, "64A6", DecoderOptions::NONE, Instruction::with_cmpsb(64, Register::FS, RepPrefixKind::None).value()},
		{32, "646667A7", DecoderOptions::NONE, Instruction::with_cmpsw(16, Register::FS, RepPrefixKind::None).value()},
		{64, "646667A7", DecoderOptions::NONE, Instruction::with_cmpsw(32, Register::FS, RepPrefixKind::None).value()},
		{64, "6466A7", DecoderOptions::NONE, Instruction::with_cmpsw(64, Register::FS, RepPrefixKind::None).value()},
		{32, "6467A7", DecoderOptions::NONE, Instruction::with_cmpsd(16, Register::FS, RepPrefixKind::None).value()},
		{64, "6467A7", DecoderOptions::NONE, Instruction::with_cmpsd(32, Register::FS, RepPrefixKind::None).value()},
		{64, "64A7", DecoderOptions::NONE, Instruction::with_cmpsd(64, Register::FS, RepPrefixKind::None).value()},
		{64, "646748A7", DecoderOptions::NONE, Instruction::with_cmpsq(32, Register::FS, RepPrefixKind::None).value()},
		{64, "6448A7", DecoderOptions::NONE, Instruction::with_cmpsq(64, Register::FS, RepPrefixKind::None).value()},
		{32, "6467A4", DecoderOptions::NONE, Instruction::with_movsb(16, Register::FS, RepPrefixKind::None).value()},
		{64, "6467A4", DecoderOptions::NONE, Instruction::with_movsb(32, Register::FS, RepPrefixKind::None).value()},
		{64, "64A4", DecoderOptions::NONE, Instruction::with_movsb(64, Register::FS, RepPrefixKind::None).value()},
		{32, "646667A5", DecoderOptions::NONE, Instruction::with_movsw(16, Register::FS, RepPrefixKind::None).value()},
		{64, "646667A5", DecoderOptions::NONE, Instruction::with_movsw(32, Register::FS, RepPrefixKind::None).value()},
		{64, "6466A5", DecoderOptions::NONE, Instruction::with_movsw(64, Register::FS, RepPrefixKind::None).value()},
		{32, "6467A5", DecoderOptions::NONE, Instruction::with_movsd(16, Register::FS, RepPrefixKind::None).value()},
		{64, "6467A5", DecoderOptions::NONE, Instruction::with_movsd(32, Register::FS, RepPrefixKind::None).value()},
		{64, "64A5", DecoderOptions::NONE, Instruction::with_movsd(64, Register::FS, RepPrefixKind::None).value()},
		{64, "646748A5", DecoderOptions::NONE, Instruction::with_movsq(32, Register::FS, RepPrefixKind::None).value()},
		{64, "6448A5", DecoderOptions::NONE, Instruction::with_movsq(64, Register::FS, RepPrefixKind::None).value()},
		{32, "64670FF7D3", DecoderOptions::NONE, Instruction::with_maskmovq(16, Register::MM2, Register::MM3, Register::FS).value()},
		{64, "64670FF7D3", DecoderOptions::NONE, Instruction::with_maskmovq(32, Register::MM2, Register::MM3, Register::FS).value()},
		{64, "640FF7D3", DecoderOptions::NONE, Instruction::with_maskmovq(64, Register::MM2, Register::MM3, Register::FS).value()},
		{32, "6467660FF7D3", DecoderOptions::NONE, Instruction::with_maskmovdqu(16, Register::XMM2, Register::XMM3, Register::FS).value()},
		{64, "6467660FF7D3", DecoderOptions::NONE, Instruction::with_maskmovdqu(32, Register::XMM2, Register::XMM3, Register::FS).value()},
		{64, "64660FF7D3", DecoderOptions::NONE, Instruction::with_maskmovdqu(64, Register::XMM2, Register::XMM3, Register::FS).value()},

		{32, "6467F36E", DecoderOptions::NONE, Instruction::with_outsb(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F36E", DecoderOptions::NONE, Instruction::with_outsb(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F36E", DecoderOptions::NONE, Instruction::with_outsb(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "646667F36F", DecoderOptions::NONE, Instruction::with_outsw(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "646667F36F", DecoderOptions::NONE, Instruction::with_outsw(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6466F36F", DecoderOptions::NONE, Instruction::with_outsw(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "6467F36F", DecoderOptions::NONE, Instruction::with_outsd(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F36F", DecoderOptions::NONE, Instruction::with_outsd(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F36F", DecoderOptions::NONE, Instruction::with_outsd(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "67F3AE", DecoderOptions::NONE, Instruction::with_scasb(16, RepPrefixKind::Repe).value()},
		{64, "67F3AE", DecoderOptions::NONE, Instruction::with_scasb(32, RepPrefixKind::Repe).value()},
		{64, "F3AE", DecoderOptions::NONE, Instruction::with_scasb(64, RepPrefixKind::Repe).value()},
		{32, "6667F3AF", DecoderOptions::NONE, Instruction::with_scasw(16, RepPrefixKind::Repe).value()},
		{64, "6667F3AF", DecoderOptions::NONE, Instruction::with_scasw(32, RepPrefixKind::Repe).value()},
		{64, "66F3AF", DecoderOptions::NONE, Instruction::with_scasw(64, RepPrefixKind::Repe).value()},
		{32, "67F3AF", DecoderOptions::NONE, Instruction::with_scasd(16, RepPrefixKind::Repe).value()},
		{64, "67F3AF", DecoderOptions::NONE, Instruction::with_scasd(32, RepPrefixKind::Repe).value()},
		{64, "F3AF", DecoderOptions::NONE, Instruction::with_scasd(64, RepPrefixKind::Repe).value()},
		{64, "67F348AF", DecoderOptions::NONE, Instruction::with_scasq(32, RepPrefixKind::Repe).value()},
		{64, "F348AF", DecoderOptions::NONE, Instruction::with_scasq(64, RepPrefixKind::Repe).value()},
		{32, "6467F3AC", DecoderOptions::NONE, Instruction::with_lodsb(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F3AC", DecoderOptions::NONE, Instruction::with_lodsb(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F3AC", DecoderOptions::NONE, Instruction::with_lodsb(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "646667F3AD", DecoderOptions::NONE, Instruction::with_lodsw(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "646667F3AD", DecoderOptions::NONE, Instruction::with_lodsw(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6466F3AD", DecoderOptions::NONE, Instruction::with_lodsw(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "6467F3AD", DecoderOptions::NONE, Instruction::with_lodsd(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F3AD", DecoderOptions::NONE, Instruction::with_lodsd(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F3AD", DecoderOptions::NONE, Instruction::with_lodsd(64, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F348AD", DecoderOptions::NONE, Instruction::with_lodsq(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F348AD", DecoderOptions::NONE, Instruction::with_lodsq(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "67F36C", DecoderOptions::NONE, Instruction::with_insb(16, RepPrefixKind::Repe).value()},
		{64, "67F36C", DecoderOptions::NONE, Instruction::with_insb(32, RepPrefixKind::Repe).value()},
		{64, "F36C", DecoderOptions::NONE, Instruction::with_insb(64, RepPrefixKind::Repe).value()},
		{32, "6667F36D", DecoderOptions::NONE, Instruction::with_insw(16, RepPrefixKind::Repe).value()},
		{64, "6667F36D", DecoderOptions::NONE, Instruction::with_insw(32, RepPrefixKind::Repe).value()},
		{64, "66F36D", DecoderOptions::NONE, Instruction::with_insw(64, RepPrefixKind::Repe).value()},
		{32, "67F36D", DecoderOptions::NONE, Instruction::with_insd(16, RepPrefixKind::Repe).value()},
		{64, "67F36D", DecoderOptions::NONE, Instruction::with_insd(32, RepPrefixKind::Repe).value()},
		{64, "F36D", DecoderOptions::NONE, Instruction::with_insd(64, RepPrefixKind::Repe).value()},
		{32, "67F3AA", DecoderOptions::NONE, Instruction::with_stosb(16, RepPrefixKind::Repe).value()},
		{64, "67F3AA", DecoderOptions::NONE, Instruction::with_stosb(32, RepPrefixKind::Repe).value()},
		{64, "F3AA", DecoderOptions::NONE, Instruction::with_stosb(64, RepPrefixKind::Repe).value()},
		{32, "6667F3AB", DecoderOptions::NONE, Instruction::with_stosw(16, RepPrefixKind::Repe).value()},
		{64, "6667F3AB", DecoderOptions::NONE, Instruction::with_stosw(32, RepPrefixKind::Repe).value()},
		{64, "66F3AB", DecoderOptions::NONE, Instruction::with_stosw(64, RepPrefixKind::Repe).value()},
		{32, "67F3AB", DecoderOptions::NONE, Instruction::with_stosd(16, RepPrefixKind::Repe).value()},
		{64, "67F3AB", DecoderOptions::NONE, Instruction::with_stosd(32, RepPrefixKind::Repe).value()},
		{64, "F3AB", DecoderOptions::NONE, Instruction::with_stosd(64, RepPrefixKind::Repe).value()},
		{64, "67F348AB", DecoderOptions::NONE, Instruction::with_stosq(32, RepPrefixKind::Repe).value()},
		{64, "F348AB", DecoderOptions::NONE, Instruction::with_stosq(64, RepPrefixKind::Repe).value()},
		{32, "6467F3A6", DecoderOptions::NONE, Instruction::with_cmpsb(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F3A6", DecoderOptions::NONE, Instruction::with_cmpsb(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F3A6", DecoderOptions::NONE, Instruction::with_cmpsb(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "646667F3A7", DecoderOptions::NONE, Instruction::with_cmpsw(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "646667F3A7", DecoderOptions::NONE, Instruction::with_cmpsw(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6466F3A7", DecoderOptions::NONE, Instruction::with_cmpsw(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "6467F3A7", DecoderOptions::NONE, Instruction::with_cmpsd(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F3A7", DecoderOptions::NONE, Instruction::with_cmpsd(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F3A7", DecoderOptions::NONE, Instruction::with_cmpsd(64, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F348A7", DecoderOptions::NONE, Instruction::with_cmpsq(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F348A7", DecoderOptions::NONE, Instruction::with_cmpsq(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "6467F3A4", DecoderOptions::NONE, Instruction::with_movsb(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F3A4", DecoderOptions::NONE, Instruction::with_movsb(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F3A4", DecoderOptions::NONE, Instruction::with_movsb(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "646667F3A5", DecoderOptions::NONE, Instruction::with_movsw(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "646667F3A5", DecoderOptions::NONE, Instruction::with_movsw(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6466F3A5", DecoderOptions::NONE, Instruction::with_movsw(64, Register::FS, RepPrefixKind::Repe).value()},
		{32, "6467F3A5", DecoderOptions::NONE, Instruction::with_movsd(16, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F3A5", DecoderOptions::NONE, Instruction::with_movsd(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F3A5", DecoderOptions::NONE, Instruction::with_movsd(64, Register::FS, RepPrefixKind::Repe).value()},
		{64, "6467F348A5", DecoderOptions::NONE, Instruction::with_movsq(32, Register::FS, RepPrefixKind::Repe).value()},
		{64, "64F348A5", DecoderOptions::NONE, Instruction::with_movsq(64, Register::FS, RepPrefixKind::Repe).value()},

		{32, "6467F26E", DecoderOptions::NONE, Instruction::with_outsb(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F26E", DecoderOptions::NONE, Instruction::with_outsb(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F26E", DecoderOptions::NONE, Instruction::with_outsb(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "646667F26F", DecoderOptions::NONE, Instruction::with_outsw(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "646667F26F", DecoderOptions::NONE, Instruction::with_outsw(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6466F26F", DecoderOptions::NONE, Instruction::with_outsw(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "6467F26F", DecoderOptions::NONE, Instruction::with_outsd(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F26F", DecoderOptions::NONE, Instruction::with_outsd(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F26F", DecoderOptions::NONE, Instruction::with_outsd(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "67F2AE", DecoderOptions::NONE, Instruction::with_scasb(16, RepPrefixKind::Repne).value()},
		{64, "67F2AE", DecoderOptions::NONE, Instruction::with_scasb(32, RepPrefixKind::Repne).value()},
		{64, "F2AE", DecoderOptions::NONE, Instruction::with_scasb(64, RepPrefixKind::Repne).value()},
		{32, "6667F2AF", DecoderOptions::NONE, Instruction::with_scasw(16, RepPrefixKind::Repne).value()},
		{64, "6667F2AF", DecoderOptions::NONE, Instruction::with_scasw(32, RepPrefixKind::Repne).value()},
		{64, "66F2AF", DecoderOptions::NONE, Instruction::with_scasw(64, RepPrefixKind::Repne).value()},
		{32, "67F2AF", DecoderOptions::NONE, Instruction::with_scasd(16, RepPrefixKind::Repne).value()},
		{64, "67F2AF", DecoderOptions::NONE, Instruction::with_scasd(32, RepPrefixKind::Repne).value()},
		{64, "F2AF", DecoderOptions::NONE, Instruction::with_scasd(64, RepPrefixKind::Repne).value()},
		{64, "67F248AF", DecoderOptions::NONE, Instruction::with_scasq(32, RepPrefixKind::Repne).value()},
		{64, "F248AF", DecoderOptions::NONE, Instruction::with_scasq(64, RepPrefixKind::Repne).value()},
		{32, "6467F2AC", DecoderOptions::NONE, Instruction::with_lodsb(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F2AC", DecoderOptions::NONE, Instruction::with_lodsb(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F2AC", DecoderOptions::NONE, Instruction::with_lodsb(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "646667F2AD", DecoderOptions::NONE, Instruction::with_lodsw(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "646667F2AD", DecoderOptions::NONE, Instruction::with_lodsw(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6466F2AD", DecoderOptions::NONE, Instruction::with_lodsw(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "6467F2AD", DecoderOptions::NONE, Instruction::with_lodsd(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F2AD", DecoderOptions::NONE, Instruction::with_lodsd(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F2AD", DecoderOptions::NONE, Instruction::with_lodsd(64, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F248AD", DecoderOptions::NONE, Instruction::with_lodsq(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F248AD", DecoderOptions::NONE, Instruction::with_lodsq(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "67F26C", DecoderOptions::NONE, Instruction::with_insb(16, RepPrefixKind::Repne).value()},
		{64, "67F26C", DecoderOptions::NONE, Instruction::with_insb(32, RepPrefixKind::Repne).value()},
		{64, "F26C", DecoderOptions::NONE, Instruction::with_insb(64, RepPrefixKind::Repne).value()},
		{32, "6667F26D", DecoderOptions::NONE, Instruction::with_insw(16, RepPrefixKind::Repne).value()},
		{64, "6667F26D", DecoderOptions::NONE, Instruction::with_insw(32, RepPrefixKind::Repne).value()},
		{64, "66F26D", DecoderOptions::NONE, Instruction::with_insw(64, RepPrefixKind::Repne).value()},
		{32, "67F26D", DecoderOptions::NONE, Instruction::with_insd(16, RepPrefixKind::Repne).value()},
		{64, "67F26D", DecoderOptions::NONE, Instruction::with_insd(32, RepPrefixKind::Repne).value()},
		{64, "F26D", DecoderOptions::NONE, Instruction::with_insd(64, RepPrefixKind::Repne).value()},
		{32, "67F2AA", DecoderOptions::NONE, Instruction::with_stosb(16, RepPrefixKind::Repne).value()},
		{64, "67F2AA", DecoderOptions::NONE, Instruction::with_stosb(32, RepPrefixKind::Repne).value()},
		{64, "F2AA", DecoderOptions::NONE, Instruction::with_stosb(64, RepPrefixKind::Repne).value()},
		{32, "6667F2AB", DecoderOptions::NONE, Instruction::with_stosw(16, RepPrefixKind::Repne).value()},
		{64, "6667F2AB", DecoderOptions::NONE, Instruction::with_stosw(32, RepPrefixKind::Repne).value()},
		{64, "66F2AB", DecoderOptions::NONE, Instruction::with_stosw(64, RepPrefixKind::Repne).value()},
		{32, "67F2AB", DecoderOptions::NONE, Instruction::with_stosd(16, RepPrefixKind::Repne).value()},
		{64, "67F2AB", DecoderOptions::NONE, Instruction::with_stosd(32, RepPrefixKind::Repne).value()},
		{64, "F2AB", DecoderOptions::NONE, Instruction::with_stosd(64, RepPrefixKind::Repne).value()},
		{64, "67F248AB", DecoderOptions::NONE, Instruction::with_stosq(32, RepPrefixKind::Repne).value()},
		{64, "F248AB", DecoderOptions::NONE, Instruction::with_stosq(64, RepPrefixKind::Repne).value()},
		{32, "6467F2A6", DecoderOptions::NONE, Instruction::with_cmpsb(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F2A6", DecoderOptions::NONE, Instruction::with_cmpsb(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F2A6", DecoderOptions::NONE, Instruction::with_cmpsb(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "646667F2A7", DecoderOptions::NONE, Instruction::with_cmpsw(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "646667F2A7", DecoderOptions::NONE, Instruction::with_cmpsw(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6466F2A7", DecoderOptions::NONE, Instruction::with_cmpsw(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "6467F2A7", DecoderOptions::NONE, Instruction::with_cmpsd(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F2A7", DecoderOptions::NONE, Instruction::with_cmpsd(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F2A7", DecoderOptions::NONE, Instruction::with_cmpsd(64, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F248A7", DecoderOptions::NONE, Instruction::with_cmpsq(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F248A7", DecoderOptions::NONE, Instruction::with_cmpsq(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "6467F2A4", DecoderOptions::NONE, Instruction::with_movsb(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F2A4", DecoderOptions::NONE, Instruction::with_movsb(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F2A4", DecoderOptions::NONE, Instruction::with_movsb(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "646667F2A5", DecoderOptions::NONE, Instruction::with_movsw(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "646667F2A5", DecoderOptions::NONE, Instruction::with_movsw(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6466F2A5", DecoderOptions::NONE, Instruction::with_movsw(64, Register::FS, RepPrefixKind::Repne).value()},
		{32, "6467F2A5", DecoderOptions::NONE, Instruction::with_movsd(16, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F2A5", DecoderOptions::NONE, Instruction::with_movsd(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F2A5", DecoderOptions::NONE, Instruction::with_movsd(64, Register::FS, RepPrefixKind::Repne).value()},
		{64, "6467F248A5", DecoderOptions::NONE, Instruction::with_movsq(32, Register::FS, RepPrefixKind::Repne).value()},
		{64, "64F248A5", DecoderOptions::NONE, Instruction::with_movsq(64, Register::FS, RepPrefixKind::Repne).value()},

		{32, "67F36E", DecoderOptions::NONE, Instruction::with_rep_outsb(16).value()},
		{64, "67F36E", DecoderOptions::NONE, Instruction::with_rep_outsb(32).value()},
		{64, "F36E", DecoderOptions::NONE, Instruction::with_rep_outsb(64).value()},
		{32, "6667F36F", DecoderOptions::NONE, Instruction::with_rep_outsw(16).value()},
		{64, "6667F36F", DecoderOptions::NONE, Instruction::with_rep_outsw(32).value()},
		{64, "66F36F", DecoderOptions::NONE, Instruction::with_rep_outsw(64).value()},
		{32, "67F36F", DecoderOptions::NONE, Instruction::with_rep_outsd(16).value()},
		{64, "67F36F", DecoderOptions::NONE, Instruction::with_rep_outsd(32).value()},
		{64, "F36F", DecoderOptions::NONE, Instruction::with_rep_outsd(64).value()},
		{32, "67F3AE", DecoderOptions::NONE, Instruction::with_repe_scasb(16).value()},
		{64, "67F3AE", DecoderOptions::NONE, Instruction::with_repe_scasb(32).value()},
		{64, "F3AE", DecoderOptions::NONE, Instruction::with_repe_scasb(64).value()},
		{32, "6667F3AF", DecoderOptions::NONE, Instruction::with_repe_scasw(16).value()},
		{64, "6667F3AF", DecoderOptions::NONE, Instruction::with_repe_scasw(32).value()},
		{64, "66F3AF", DecoderOptions::NONE, Instruction::with_repe_scasw(64).value()},
		{32, "67F3AF", DecoderOptions::NONE, Instruction::with_repe_scasd(16).value()},
		{64, "67F3AF", DecoderOptions::NONE, Instruction::with_repe_scasd(32).value()},
		{64, "F3AF", DecoderOptions::NONE, Instruction::with_repe_scasd(64).value()},
		{64, "67F348AF", DecoderOptions::NONE, Instruction::with_repe_scasq(32).value()},
		{64, "F348AF", DecoderOptions::NONE, Instruction::with_repe_scasq(64).value()},
		{32, "67F2AE", DecoderOptions::NONE, Instruction::with_repne_scasb(16).value()},
		{64, "67F2AE", DecoderOptions::NONE, Instruction::with_repne_scasb(32).value()},
		{64, "F2AE", DecoderOptions::NONE, Instruction::with_repne_scasb(64).value()},
		{32, "6667F2AF", DecoderOptions::NONE, Instruction::with_repne_scasw(16).value()},
		{64, "6667F2AF", DecoderOptions::NONE, Instruction::with_repne_scasw(32).value()},
		{64, "66F2AF", DecoderOptions::NONE, Instruction::with_repne_scasw(64).value()},
		{32, "67F2AF", DecoderOptions::NONE, Instruction::with_repne_scasd(16).value()},
		{64, "67F2AF", DecoderOptions::NONE, Instruction::with_repne_scasd(32).value()},
		{64, "F2AF", DecoderOptions::NONE, Instruction::with_repne_scasd(64).value()},
		{64, "67F248AF", DecoderOptions::NONE, Instruction::with_repne_scasq(32).value()},
		{64, "F248AF", DecoderOptions::NONE, Instruction::with_repne_scasq(64).value()},
		{32, "67F3AC", DecoderOptions::NONE, Instruction::with_rep_lodsb(16).value()},
		{64, "67F3AC", DecoderOptions::NONE, Instruction::with_rep_lodsb(32).value()},
		{64, "F3AC", DecoderOptions::NONE, Instruction::with_rep_lodsb(64).value()},
		{32, "6667F3AD", DecoderOptions::NONE, Instruction::with_rep_lodsw(16).value()},
		{64, "6667F3AD", DecoderOptions::NONE, Instruction::with_rep_lodsw(32).value()},
		{64, "66F3AD", DecoderOptions::NONE, Instruction::with_rep_lodsw(64).value()},
		{32, "67F3AD", DecoderOptions::NONE, Instruction::with_rep_lodsd(16).value()},
		{64, "67F3AD", DecoderOptions::NONE, Instruction::with_rep_lodsd(32).value()},
		{64, "F3AD", DecoderOptions::NONE, Instruction::with_rep_lodsd(64).value()},
		{64, "67F348AD", DecoderOptions::NONE, Instruction::with_rep_lodsq(32).value()},
		{64, "F348AD", DecoderOptions::NONE, Instruction::with_rep_lodsq(64).value()},
		{32, "67F36C", DecoderOptions::NONE, Instruction::with_rep_insb(16).value()},
		{64, "67F36C", DecoderOptions::NONE, Instruction::with_rep_insb(32).value()},
		{64, "F36C", DecoderOptions::NONE, Instruction::with_rep_insb(64).value()},
		{32, "6667F36D", DecoderOptions::NONE, Instruction::with_rep_insw(16).value()},
		{64, "6667F36D", DecoderOptions::NONE, Instruction::with_rep_insw(32).value()},
		{64, "66F36D", DecoderOptions::NONE, Instruction::with_rep_insw(64).value()},
		{32, "67F36D", DecoderOptions::NONE, Instruction::with_rep_insd(16).value()},
		{64, "67F36D", DecoderOptions::NONE, Instruction::with_rep_insd(32).value()},
		{64, "F36D", DecoderOptions::NONE, Instruction::with_rep_insd(64).value()},
		{32, "67F3AA", DecoderOptions::NONE, Instruction::with_rep_stosb(16).value()},
		{64, "67F3AA", DecoderOptions::NONE, Instruction::with_rep_stosb(32).value()},
		{64, "F3AA", DecoderOptions::NONE, Instruction::with_rep_stosb(64).value()},
		{32, "6667F3AB", DecoderOptions::NONE, Instruction::with_rep_stosw(16).value()},
		{64, "6667F3AB", DecoderOptions::NONE, Instruction::with_rep_stosw(32).value()},
		{64, "66F3AB", DecoderOptions::NONE, Instruction::with_rep_stosw(64).value()},
		{32, "67F3AB", DecoderOptions::NONE, Instruction::with_rep_stosd(16).value()},
		{64, "67F3AB", DecoderOptions::NONE, Instruction::with_rep_stosd(32).value()},
		{64, "F3AB", DecoderOptions::NONE, Instruction::with_rep_stosd(64).value()},
		{64, "67F348AB", DecoderOptions::NONE, Instruction::with_rep_stosq(32).value()},
		{64, "F348AB", DecoderOptions::NONE, Instruction::with_rep_stosq(64).value()},
		{32, "67F3A6", DecoderOptions::NONE, Instruction::with_repe_cmpsb(16).value()},
		{64, "67F3A6", DecoderOptions::NONE, Instruction::with_repe_cmpsb(32).value()},
		{64, "F3A6", DecoderOptions::NONE, Instruction::with_repe_cmpsb(64).value()},
		{32, "6667F3A7", DecoderOptions::NONE, Instruction::with_repe_cmpsw(16).value()},
		{64, "6667F3A7", DecoderOptions::NONE, Instruction::with_repe_cmpsw(32).value()},
		{64, "66F3A7", DecoderOptions::NONE, Instruction::with_repe_cmpsw(64).value()},
		{32, "67F3A7", DecoderOptions::NONE, Instruction::with_repe_cmpsd(16).value()},
		{64, "67F3A7", DecoderOptions::NONE, Instruction::with_repe_cmpsd(32).value()},
		{64, "F3A7", DecoderOptions::NONE, Instruction::with_repe_cmpsd(64).value()},
		{64, "67F348A7", DecoderOptions::NONE, Instruction::with_repe_cmpsq(32).value()},
		{64, "F348A7", DecoderOptions::NONE, Instruction::with_repe_cmpsq(64).value()},
		{32, "67F2A6", DecoderOptions::NONE, Instruction::with_repne_cmpsb(16).value()},
		{64, "67F2A6", DecoderOptions::NONE, Instruction::with_repne_cmpsb(32).value()},
		{64, "F2A6", DecoderOptions::NONE, Instruction::with_repne_cmpsb(64).value()},
		{32, "6667F2A7", DecoderOptions::NONE, Instruction::with_repne_cmpsw(16).value()},
		{64, "6667F2A7", DecoderOptions::NONE, Instruction::with_repne_cmpsw(32).value()},
		{64, "66F2A7", DecoderOptions::NONE, Instruction::with_repne_cmpsw(64).value()},
		{32, "67F2A7", DecoderOptions::NONE, Instruction::with_repne_cmpsd(16).value()},
		{64, "67F2A7", DecoderOptions::NONE, Instruction::with_repne_cmpsd(32).value()},
		{64, "F2A7", DecoderOptions::NONE, Instruction::with_repne_cmpsd(64).value()},
		{64, "67F248A7", DecoderOptions::NONE, Instruction::with_repne_cmpsq(32).value()},
		{64, "F248A7", DecoderOptions::NONE, Instruction::with_repne_cmpsq(64).value()},
		{32, "67F3A4", DecoderOptions::NONE, Instruction::with_rep_movsb(16).value()},
		{64, "67F3A4", DecoderOptions::NONE, Instruction::with_rep_movsb(32).value()},
		{64, "F3A4", DecoderOptions::NONE, Instruction::with_rep_movsb(64).value()},
		{32, "6667F3A5", DecoderOptions::NONE, Instruction::with_rep_movsw(16).value()},
		{64, "6667F3A5", DecoderOptions::NONE, Instruction::with_rep_movsw(32).value()},
		{64, "66F3A5", DecoderOptions::NONE, Instruction::with_rep_movsw(64).value()},
		{32, "67F3A5", DecoderOptions::NONE, Instruction::with_rep_movsd(16).value()},
		{64, "67F3A5", DecoderOptions::NONE, Instruction::with_rep_movsd(32).value()},
		{64, "F3A5", DecoderOptions::NONE, Instruction::with_rep_movsd(64).value()},
		{64, "67F348A5", DecoderOptions::NONE, Instruction::with_rep_movsq(32).value()},
		{64, "F348A5", DecoderOptions::NONE, Instruction::with_rep_movsq(64).value()},
	};
	with_test_core(tests);
}

TEST_CASE("encoder/try_with_test_vex") {
	const std::vector<WithTest> tests = {
		{64, "C5E814CB", DecoderOptions::NONE, Instruction::with3(Code::VEX_Vunpcklps_xmm_xmm_xmmm128, Register::XMM1, Register::XMM2, Register::XMM3).value()},
		{64, "64C5E8148C7501EFCDAB", DecoderOptions::NONE, Instruction::with3(Code::VEX_Vunpcklps_xmm_xmm_xmmm128, Register::XMM1, Register::XMM2, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS)).value()},
		{64, "64C4E261908C7501EFCDAB", DecoderOptions::NONE, Instruction::with3(Code::VEX_Vpgatherdd_xmm_vm32x_xmm, Register::XMM1, MemoryOperand(Register::RBP, Register::XMM6, 2, -0x5432'10FF, 8, false, Register::FS), Register::XMM3).value()},
		{64, "64C4E2692E9C7501EFCDAB", DecoderOptions::NONE, Instruction::with3(Code::VEX_Vmaskmovps_m128_xmm_xmm, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), Register::XMM2, Register::XMM3).value()},
		{64, "C4E3694ACB40", DecoderOptions::NONE, Instruction::with4(Code::VEX_Vblendvps_xmm_xmm_xmmm128_xmm, Register::XMM1, Register::XMM2, Register::XMM3, Register::XMM4).value()},
		{64, "64C4E3E95C8C7501EFCDAB30", DecoderOptions::NONE, Instruction::with4(Code::VEX_Vfmaddsubps_xmm_xmm_xmm_xmmm128, Register::XMM1, Register::XMM2, Register::XMM3, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS)).value()},
		{64, "64C4E3694A8C7501EFCDAB40", DecoderOptions::NONE, Instruction::with4(Code::VEX_Vblendvps_xmm_xmm_xmmm128_xmm, Register::XMM1, Register::XMM2, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), Register::XMM4).value()},
		{64, "C4E36948CB40", DecoderOptions::NONE, Instruction::with5(Code::VEX_Vpermil2ps_xmm_xmm_xmmm128_xmm_imm4, Register::XMM1, Register::XMM2, Register::XMM3, Register::XMM4, 0x0).value()},
		{64, "C4E36948CB40", DecoderOptions::NONE, Instruction::with5(Code::VEX_Vpermil2ps_xmm_xmm_xmmm128_xmm_imm4, Register::XMM1, Register::XMM2, Register::XMM3, Register::XMM4, 0x0).value()},
		{64, "64C4E3E9488C7501EFCDAB31", DecoderOptions::NONE, Instruction::with5(Code::VEX_Vpermil2ps_xmm_xmm_xmm_xmmm128_imm4, Register::XMM1, Register::XMM2, Register::XMM3, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x1).value()},
		{64, "64C4E3E9488C7501EFCDAB31", DecoderOptions::NONE, Instruction::with5(Code::VEX_Vpermil2ps_xmm_xmm_xmm_xmmm128_imm4, Register::XMM1, Register::XMM2, Register::XMM3, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0x1).value()},
		{64, "64C4E369488C7501EFCDAB41", DecoderOptions::NONE, Instruction::with5(Code::VEX_Vpermil2ps_xmm_xmm_xmmm128_xmm_imm4, Register::XMM1, Register::XMM2, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), Register::XMM4, 0x1).value()},
		{64, "64C4E369488C7501EFCDAB41", DecoderOptions::NONE, Instruction::with5(Code::VEX_Vpermil2ps_xmm_xmm_xmmm128_xmm_imm4, Register::XMM1, Register::XMM2, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), Register::XMM4, 0x1).value()},
		{32, "6467C5F9F7D3", DecoderOptions::NONE, Instruction::with_vmaskmovdqu(16, Register::XMM2, Register::XMM3, Register::FS).value()},
		{64, "6467C5F9F7D3", DecoderOptions::NONE, Instruction::with_vmaskmovdqu(32, Register::XMM2, Register::XMM3, Register::FS).value()},
		{64, "64C5F9F7D3", DecoderOptions::NONE, Instruction::with_vmaskmovdqu(64, Register::XMM2, Register::XMM3, Register::FS).value()},
	};
	with_test_core(tests);
}

TEST_CASE("encoder/try_with_test_evex") {
	const std::vector<WithTest> tests = {
		{64, "62F1F50873D2A5", DecoderOptions::NONE, Instruction::with3(Code::EVEX_Vpsrlq_xmm_k1z_xmmm128b64_imm8, Register::XMM1, Register::XMM2, 0xA5).value()},
		{64, "6462F1F50873947501EFCDABA5", DecoderOptions::NONE, Instruction::with3(Code::EVEX_Vpsrlq_xmm_k1z_xmmm128b64_imm8, Register::XMM1, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0xA5).value()},
		{64, "62F16D08C4CBA5", DecoderOptions::NONE, Instruction::with4(Code::EVEX_Vpinsrw_xmm_xmm_r32m16_imm8, Register::XMM1, Register::XMM2, Register::EBX, 0xA5).value()},
		{64, "62F16D08C4CBA5", DecoderOptions::NONE, Instruction::with4(Code::EVEX_Vpinsrw_xmm_xmm_r32m16_imm8, Register::XMM1, Register::XMM2, Register::EBX, 0xA5).value()},
		{64, "6462F16D08C48C7501EFCDABA5", DecoderOptions::NONE, Instruction::with4(Code::EVEX_Vpinsrw_xmm_xmm_r32m16_imm8, Register::XMM1, Register::XMM2, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0xA5).value()},
		{64, "6462F16D08C48C7501EFCDABA5", DecoderOptions::NONE, Instruction::with4(Code::EVEX_Vpinsrw_xmm_xmm_r32m16_imm8, Register::XMM1, Register::XMM2, MemoryOperand(Register::RBP, Register::RSI, 2, -0x5432'10FF, 8, false, Register::FS), 0xA5).value()},
	};
	with_test_core(tests);
}

TEST_CASE("encoder/with_declare_xxx_fails_if_invalid_length") {
	const std::vector<std::uint8_t> bytes(24);
	const std::vector<std::uint16_t> words(24);
	const std::vector<std::uint32_t> dwords(24);
	const std::vector<std::uint64_t> qwords(24);
	const std::vector<std::function<Result<Instruction>()>> tests = {
		[&] { return Instruction::with_declare_byte(bytes.data(), 0); },
		[&] { return Instruction::with_declare_byte(bytes.data(), 17); },

		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 0); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 1); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 3); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 5); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 7); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 9); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 11); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 13); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 15); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 17); },
		[&] { return Instruction::with_declare_word_slice_u8(bytes.data(), 18); },
		[&] { return Instruction::with_declare_word(words.data(), 0); },
		[&] { return Instruction::with_declare_word(words.data(), 9); },

		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 0); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 1); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 2); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 3); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 5); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 6); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 7); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 9); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 10); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 11); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 13); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 14); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 15); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 17); },
		[&] { return Instruction::with_declare_dword_slice_u8(bytes.data(), 20); },
		[&] { return Instruction::with_declare_dword(dwords.data(), 0); },
		[&] { return Instruction::with_declare_dword(dwords.data(), 5); },

		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 0); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 1); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 2); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 3); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 4); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 5); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 6); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 7); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 9); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 10); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 11); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 12); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 13); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 14); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 15); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 17); },
		[&] { return Instruction::with_declare_qword_slice_u8(bytes.data(), 24); },
		[&] { return Instruction::with_declare_qword(qwords.data(), 0); },
		[&] { return Instruction::with_declare_qword(qwords.data(), 3); },
	};
	for (const auto& f : tests)
		CHECK(f().is_err());
}

TEST_CASE("encoder/get_set_op_kind_panics_if_invalid_input") {
	Instruction instr = Instruction::with2(Code::Adc_EAX_imm32, Register::EAX, std::numeric_limits<std::uint32_t>::max()).value();

	(void)instr.op_kind(0);
	(void)instr.op_kind(1);
	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++)
		(void)instr.op_kind(i);
#ifndef NDEBUG
	CHECK(aborts([&] { (void)instr.op_kind(IcedConstants::MAX_OP_COUNT); }));
#else
	CHECK_EQ(instr.op_kind(IcedConstants::MAX_OP_COUNT), OpKind::Register);
#endif

	instr.set_op_kind(0, OpKind::Register);
	instr.set_op_kind(1, OpKind::Immediate32);
	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++)
		instr.set_op_kind(i, OpKind::Immediate8);
	CHECK_DEBUG_PANICS(instr.set_op_kind(IcedConstants::MAX_OP_COUNT, OpKind::Register));
}

TEST_CASE("encoder/get_set_op_kind_fails_if_invalid_input") {
	Instruction instr = Instruction::with2(Code::Adc_EAX_imm32, Register::EAX, std::numeric_limits<std::uint32_t>::max()).value();

	(void)instr.try_op_kind(0).value();
	(void)instr.try_op_kind(1).value();
	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++)
		(void)instr.try_op_kind(i).value();
	CHECK(instr.try_op_kind(IcedConstants::MAX_OP_COUNT).is_err());

	instr.try_set_op_kind(0, OpKind::Register).value();
	instr.try_set_op_kind(1, OpKind::Immediate32).value();
	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++)
		instr.try_set_op_kind(i, OpKind::Immediate8).value();
	CHECK(instr.try_set_op_kind(IcedConstants::MAX_OP_COUNT, OpKind::Register).is_err());
}

TEST_CASE("encoder/try_immediate_fails_if_invalid_input") {
	const Instruction instr = Instruction::with2(Code::Adc_EAX_imm32, Register::EAX, std::numeric_limits<std::uint32_t>::max()).value();

	CHECK(instr.try_immediate(0).is_err());
	CHECK(instr.try_immediate(1).is_ok());
	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++) {
		if (i == 4 && instr.op4_kind() == OpKind::Immediate8)
			continue;
		CHECK(instr.try_immediate(i).is_err());
	}
}

TEST_CASE("encoder/get_set_immediate_panics_if_invalid_input") {
	Instruction instr = Instruction::with2(Code::Adc_EAX_imm32, Register::EAX, std::numeric_limits<std::uint32_t>::max()).value();

	CHECK_DEBUG_PANICS(instr.immediate(0));
	(void)instr.immediate(1);
	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++) {
		if (i == 4 && instr.op4_kind() == OpKind::Immediate8)
			continue;
		CHECK_DEBUG_PANICS(instr.immediate(i));
	}
	CHECK_DEBUG_PANICS(instr.immediate(IcedConstants::MAX_OP_COUNT));

	{
		Instruction instr2 = instr;
		CHECK_DEBUG_PANICS(instr2.set_immediate_i32(0, 0));
	}
	{
		Instruction instr2 = instr;
		CHECK_DEBUG_PANICS(instr2.set_immediate_i64(0, 0));
	}
	{
		Instruction instr2 = instr;
		CHECK_DEBUG_PANICS(instr2.set_immediate_u32(0, 0));
	}
	{
		Instruction instr2 = instr;
		CHECK_DEBUG_PANICS(instr2.set_immediate_u64(0, 0));
	}

	instr.set_immediate_i32(1, 0);
	instr.set_immediate_i64(1, 0);
	instr.set_immediate_u32(1, 0);
	instr.set_immediate_u64(1, 0);

	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++) {
		if (i == 4 && instr.op4_kind() == OpKind::Immediate8)
			continue;
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_immediate_i32(i, 0));
		}
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_immediate_i64(i, 0));
		}
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_immediate_u32(i, 0));
		}
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_immediate_u64(i, 0));
		}
	}
	{
		Instruction instr2 = instr;
		CHECK_DEBUG_PANICS(instr2.set_immediate_i32(IcedConstants::MAX_OP_COUNT, 0));
	}
	{
		Instruction instr2 = instr;
		CHECK_DEBUG_PANICS(instr2.set_immediate_i64(IcedConstants::MAX_OP_COUNT, 0));
	}
	{
		Instruction instr2 = instr;
		CHECK_DEBUG_PANICS(instr2.set_immediate_u32(IcedConstants::MAX_OP_COUNT, 0));
	}
	{
		Instruction instr2 = instr;
		CHECK_DEBUG_PANICS(instr2.set_immediate_u64(IcedConstants::MAX_OP_COUNT, 0));
	}
}

TEST_CASE("encoder/get_set_immediate_fails_if_invalid_input") {
	Instruction instr = Instruction::with2(Code::Adc_EAX_imm32, Register::EAX, std::numeric_limits<std::uint32_t>::max()).value();

	CHECK_DEBUG_PANICS(instr.immediate(0));
	(void)instr.immediate(1);
	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++) {
		if (i == 4 && instr.op4_kind() == OpKind::Immediate8)
			continue;
		CHECK_DEBUG_PANICS(instr.immediate(i));
	}
	CHECK_DEBUG_PANICS(instr.immediate(IcedConstants::MAX_OP_COUNT));

	CHECK(instr.try_set_immediate_i32(0, 0).is_err());
	CHECK(instr.try_set_immediate_i64(0, 0).is_err());
	CHECK(instr.try_set_immediate_u32(0, 0).is_err());
	CHECK(instr.try_set_immediate_u64(0, 0).is_err());

	instr.try_set_immediate_i32(1, 0).value();
	instr.try_set_immediate_i64(1, 0).value();
	instr.try_set_immediate_u32(1, 0).value();
	instr.try_set_immediate_u64(1, 0).value();

	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++) {
		if (i == 4 && instr.op4_kind() == OpKind::Immediate8)
			continue;
		CHECK(instr.try_set_immediate_i32(i, 0).is_err());
		CHECK(instr.try_set_immediate_i64(i, 0).is_err());
		CHECK(instr.try_set_immediate_u32(i, 0).is_err());
		CHECK(instr.try_set_immediate_u64(i, 0).is_err());
	}
	CHECK(instr.try_set_immediate_i32(IcedConstants::MAX_OP_COUNT, 0).is_err());
	CHECK(instr.try_set_immediate_i64(IcedConstants::MAX_OP_COUNT, 0).is_err());
	CHECK(instr.try_set_immediate_u32(IcedConstants::MAX_OP_COUNT, 0).is_err());
	CHECK(instr.try_set_immediate_u64(IcedConstants::MAX_OP_COUNT, 0).is_err());
}

TEST_CASE("encoder/try_get_set_immediate_fails_if_invalid_input") {
	Instruction instr = Instruction::with2(Code::Adc_EAX_imm32, Register::EAX, std::numeric_limits<std::uint32_t>::max()).value();

	CHECK(instr.try_immediate(0).is_err());
	(void)instr.try_immediate(1).value();
	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++) {
		if (i == 4 && instr.op4_kind() == OpKind::Immediate8)
			continue;
		CHECK(instr.try_immediate(i).is_err());
	}
	CHECK(instr.try_immediate(IcedConstants::MAX_OP_COUNT).is_err());

	CHECK(instr.try_set_immediate_i32(0, 0).is_err());
	CHECK(instr.try_set_immediate_i64(0, 0).is_err());
	CHECK(instr.try_set_immediate_u32(0, 0).is_err());
	CHECK(instr.try_set_immediate_u64(0, 0).is_err());

	instr.try_set_immediate_i32(1, 0).value();
	instr.try_set_immediate_i64(1, 0).value();
	instr.try_set_immediate_u32(1, 0).value();
	instr.try_set_immediate_u64(1, 0).value();

	for (std::uint32_t i = 2; i < IcedConstants::MAX_OP_COUNT; i++) {
		if (i == 4 && instr.op4_kind() == OpKind::Immediate8)
			continue;
		CHECK(instr.try_set_immediate_i32(i, 0).is_err());
		CHECK(instr.try_set_immediate_i64(i, 0).is_err());
		CHECK(instr.try_set_immediate_u32(i, 0).is_err());
		CHECK(instr.try_set_immediate_u64(i, 0).is_err());
	}
	CHECK(instr.try_set_immediate_i32(IcedConstants::MAX_OP_COUNT, 0).is_err());
	CHECK(instr.try_set_immediate_i64(IcedConstants::MAX_OP_COUNT, 0).is_err());
	CHECK(instr.try_set_immediate_u32(IcedConstants::MAX_OP_COUNT, 0).is_err());
	CHECK(instr.try_set_immediate_u64(IcedConstants::MAX_OP_COUNT, 0).is_err());
}

TEST_CASE("encoder/get_set_register_panics_if_invalid_input") {
	Instruction instr = Instruction::with2(Code::Adc_EAX_imm32, Register::EAX, std::numeric_limits<std::uint32_t>::max()).value();

	for (std::uint32_t i = 0; i < IcedConstants::MAX_OP_COUNT; i++)
		(void)instr.op_register(i);
	CHECK_DEBUG_PANICS(instr.op_register(IcedConstants::MAX_OP_COUNT));

	for (std::uint32_t i = 0; i < IcedConstants::MAX_OP_COUNT; i++) {
		if (i == 4 && instr.op4_kind() == OpKind::Immediate8)
			continue;
		instr.set_op_register(i, Register::EAX);
	}
	CHECK_DEBUG_PANICS(instr.set_op_register(IcedConstants::MAX_OP_COUNT, Register::EAX));
}

TEST_CASE("encoder/get_set_register_fails_if_invalid_input") {
	Instruction instr = Instruction::with2(Code::Adc_EAX_imm32, Register::EAX, std::numeric_limits<std::uint32_t>::max()).value();

	for (std::uint32_t i = 0; i < IcedConstants::MAX_OP_COUNT; i++)
		(void)instr.try_op_register(i).value();
	CHECK(instr.try_op_register(IcedConstants::MAX_OP_COUNT).is_err());

	for (std::uint32_t i = 0; i < IcedConstants::MAX_OP_COUNT; i++) {
		if (i == 4 && instr.op4_kind() == OpKind::Immediate8)
			CHECK(instr.try_set_op_register(i, Register::EAX).is_err());
		else
			instr.try_set_op_register(i, Register::EAX).value();
	}
	CHECK(instr.try_set_op_register(IcedConstants::MAX_OP_COUNT, Register::EAX).is_err());
}

TEST_CASE("encoder/set_declare_xxx_value_panics_if_invalid_input") {
	const std::uint8_t zero_bytes[1] = {0};
	const std::uint16_t zero_words[1] = {0};
	const std::uint32_t zero_dwords[1] = {0};
	const std::uint64_t zero_qwords[1] = {0};
	{
		Instruction instr = Instruction::with_declare_byte(zero_bytes, 1).value();
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_declare_byte_value_i8(16, 0));
		}
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_declare_byte_value(16, 0));
		}
		for (std::size_t i = 0; i < 16; i++) {
			instr.set_declare_byte_value_i8(i, 0);
			instr.set_declare_byte_value(i, 0);
		}
	}
	{
		Instruction instr = Instruction::with_declare_word(zero_words, 1).value();
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_declare_word_value_i16(8, 0));
		}
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_declare_word_value(8, 0));
		}
		for (std::size_t i = 0; i < 8; i++) {
			instr.set_declare_word_value_i16(i, 0);
			instr.set_declare_word_value(i, 0);
		}
	}
	{
		Instruction instr = Instruction::with_declare_dword(zero_dwords, 1).value();
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_declare_dword_value_i32(4, 0));
		}
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_declare_dword_value(4, 0));
		}
		for (std::size_t i = 0; i < 4; i++) {
			instr.set_declare_dword_value_i32(i, 0);
			instr.set_declare_dword_value(i, 0);
		}
	}
	{
		Instruction instr = Instruction::with_declare_qword(zero_qwords, 1).value();
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_declare_qword_value_i64(2, 0));
		}
		{
			Instruction instr2 = instr;
			CHECK_DEBUG_PANICS(instr2.set_declare_qword_value(2, 0));
		}
		for (std::size_t i = 0; i < 2; i++) {
			instr.set_declare_qword_value_i64(i, 0);
			instr.set_declare_qword_value(i, 0);
		}
	}
}

TEST_CASE("encoder/set_declare_xxx_value_fails_if_invalid_input") {
	const std::uint8_t zero_bytes[1] = {0};
	const std::uint16_t zero_words[1] = {0};
	const std::uint32_t zero_dwords[1] = {0};
	const std::uint64_t zero_qwords[1] = {0};
	{
		Instruction instr = Instruction::with_declare_byte(zero_bytes, 1).value();
		CHECK(instr.try_set_declare_byte_value_i8(16, 0).is_err());
		CHECK(instr.try_set_declare_byte_value(16, 0).is_err());
		for (std::size_t i = 0; i < 16; i++) {
			instr.try_set_declare_byte_value_i8(i, 0).value();
			instr.try_set_declare_byte_value(i, 0).value();
		}
	}
	{
		Instruction instr = Instruction::with_declare_word(zero_words, 1).value();
		CHECK(instr.try_set_declare_word_value_i16(8, 0).is_err());
		CHECK(instr.try_set_declare_word_value(8, 0).is_err());
		for (std::size_t i = 0; i < 8; i++) {
			instr.try_set_declare_word_value_i16(i, 0).value();
			instr.try_set_declare_word_value(i, 0).value();
		}
	}
	{
		Instruction instr = Instruction::with_declare_dword(zero_dwords, 1).value();
		CHECK(instr.try_set_declare_dword_value_i32(4, 0).is_err());
		CHECK(instr.try_set_declare_dword_value(4, 0).is_err());
		for (std::size_t i = 0; i < 4; i++) {
			instr.try_set_declare_dword_value_i32(i, 0).value();
			instr.try_set_declare_dword_value(i, 0).value();
		}
	}
	{
		Instruction instr = Instruction::with_declare_qword(zero_qwords, 1).value();
		CHECK(instr.try_set_declare_qword_value_i64(2, 0).is_err());
		CHECK(instr.try_set_declare_qword_value(2, 0).is_err());
		for (std::size_t i = 0; i < 2; i++) {
			instr.try_set_declare_qword_value_i64(i, 0).value();
			instr.try_set_declare_qword_value(i, 0).value();
		}
	}
}

TEST_CASE("encoder/get_declare_xxx_value_panics_if_invalid_input") {
	const std::uint8_t zero_bytes[1] = {0};
	const std::uint16_t zero_words[1] = {0};
	const std::uint32_t zero_dwords[1] = {0};
	const std::uint64_t zero_qwords[1] = {0};
	{
		const Instruction instr = Instruction::with_declare_byte(zero_bytes, 1).value();
		CHECK_DEBUG_PANICS(instr.get_declare_byte_value(16));
		for (std::size_t i = 0; i < 16; i++)
			(void)instr.get_declare_byte_value(i);
	}
	{
		const Instruction instr = Instruction::with_declare_word(zero_words, 1).value();
		CHECK_DEBUG_PANICS(instr.get_declare_word_value(8));
		for (std::size_t i = 0; i < 8; i++)
			(void)instr.get_declare_word_value(i);
	}
	{
		const Instruction instr = Instruction::with_declare_dword(zero_dwords, 1).value();
		CHECK_DEBUG_PANICS(instr.get_declare_dword_value(4));
		for (std::size_t i = 0; i < 4; i++)
			(void)instr.get_declare_dword_value(i);
	}
	{
		const Instruction instr = Instruction::with_declare_qword(zero_qwords, 1).value();
		CHECK_DEBUG_PANICS(instr.get_declare_qword_value(2));
		for (std::size_t i = 0; i < 2; i++)
			(void)instr.get_declare_qword_value(i);
	}
}

TEST_CASE("encoder/get_declare_xxx_value_fails_if_invalid_input") {
	const std::uint8_t zero_bytes[1] = {0};
	const std::uint16_t zero_words[1] = {0};
	const std::uint32_t zero_dwords[1] = {0};
	const std::uint64_t zero_qwords[1] = {0};
	{
		const Instruction instr = Instruction::with_declare_byte(zero_bytes, 1).value();
		CHECK(instr.try_get_declare_byte_value(16).is_err());
		for (std::size_t i = 0; i < 16; i++)
			(void)instr.try_get_declare_byte_value(i).value();
	}
	{
		const Instruction instr = Instruction::with_declare_word(zero_words, 1).value();
		CHECK(instr.try_get_declare_word_value(8).is_err());
		for (std::size_t i = 0; i < 8; i++)
			(void)instr.try_get_declare_word_value(i).value();
	}
	{
		const Instruction instr = Instruction::with_declare_dword(zero_dwords, 1).value();
		CHECK(instr.try_get_declare_dword_value(4).is_err());
		for (std::size_t i = 0; i < 4; i++)
			(void)instr.try_get_declare_dword_value(i).value();
	}
	{
		const Instruction instr = Instruction::with_declare_qword(zero_qwords, 1).value();
		CHECK(instr.try_get_declare_qword_value(2).is_err());
		for (std::size_t i = 0; i < 2; i++)
			(void)instr.try_get_declare_qword_value(i).value();
	}
}

TEST_CASE("encoder/encode_invalid_reg_op_size") {
	const std::vector<std::pair<std::uint32_t, Instruction>> tests = {
		{16, Instruction::with2(Code::Movdir64b_r16_m512, Register::CX, MemoryOperand::with_base(Register::EBX)).value()},
		{32, Instruction::with2(Code::Movdir64b_r16_m512, Register::CX, MemoryOperand::with_base(Register::EBX)).value()},

		{16, Instruction::with2(Code::Movdir64b_r32_m512, Register::ECX, MemoryOperand::with_base(Register::BX)).value()},
		{32, Instruction::with2(Code::Movdir64b_r32_m512, Register::ECX, MemoryOperand::with_base(Register::BX)).value()},
		{64, Instruction::with2(Code::Movdir64b_r32_m512, Register::ECX, MemoryOperand::with_base(Register::RBX)).value()},

		{64, Instruction::with2(Code::Movdir64b_r64_m512, Register::RCX, MemoryOperand::with_base(Register::EBX)).value()},

		{16, Instruction::with2(Code::Enqcmds_r16_m512, Register::CX, MemoryOperand::with_base(Register::EBX)).value()},
		{32, Instruction::with2(Code::Enqcmds_r16_m512, Register::CX, MemoryOperand::with_base(Register::EBX)).value()},

		{16, Instruction::with2(Code::Enqcmds_r32_m512, Register::ECX, MemoryOperand::with_base(Register::BX)).value()},
		{32, Instruction::with2(Code::Enqcmds_r32_m512, Register::ECX, MemoryOperand::with_base(Register::BX)).value()},
		{64, Instruction::with2(Code::Enqcmds_r32_m512, Register::ECX, MemoryOperand::with_base(Register::RBX)).value()},

		{64, Instruction::with2(Code::Enqcmds_r64_m512, Register::RCX, MemoryOperand::with_base(Register::EBX)).value()},

		{16, Instruction::with2(Code::Enqcmd_r16_m512, Register::CX, MemoryOperand::with_base(Register::EBX)).value()},
		{32, Instruction::with2(Code::Enqcmd_r16_m512, Register::CX, MemoryOperand::with_base(Register::EBX)).value()},

		{16, Instruction::with2(Code::Enqcmd_r32_m512, Register::ECX, MemoryOperand::with_base(Register::BX)).value()},
		{32, Instruction::with2(Code::Enqcmd_r32_m512, Register::ECX, MemoryOperand::with_base(Register::BX)).value()},
		{64, Instruction::with2(Code::Enqcmd_r32_m512, Register::ECX, MemoryOperand::with_base(Register::RBX)).value()},

		{64, Instruction::with2(Code::Enqcmd_r64_m512, Register::RCX, MemoryOperand::with_base(Register::EBX)).value()},
	};
	for (const auto& [bitness, instr] : tests) {
		Encoder encoder(bitness);
		auto result = encoder.encode(instr, 0);
		REQUIRE_MSG(result.is_err(), "It should fail to encode an invalid instruction");
		const std::string error_message = result.error().message();
		CHECK(error_message.find("Register operand size must equal memory addressing mode (16/32/64)") != std::string::npos);
	}
}

namespace {
void create_fails_if_invalid_bitness_core(const std::vector<Result<Instruction> (*)(std::uint32_t)>& tests) {
	for (const auto& f : tests)
		CHECK(f(128).is_err());
}
} // namespace

TEST_CASE("encoder/create_fails_if_invalid_bitness") {
	const std::vector<Result<Instruction> (*)(std::uint32_t)> tests = {
		[](std::uint32_t bitness) { return Instruction::with_xbegin(bitness, 0x8000'0000'3412'A550); },
		[](std::uint32_t bitness) { return Instruction::with_outsb(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_outsw(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_outsd(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_scasb(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_scasw(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_scasd(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_scasq(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_lodsb(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_lodsw(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_lodsd(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_lodsq(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_insb(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_insw(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_insd(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_stosb(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_stosw(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_stosd(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_stosq(bitness, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_cmpsb(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_cmpsw(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_cmpsd(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_cmpsq(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_movsb(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_movsw(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_movsd(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_movsq(bitness, Register::FS, RepPrefixKind::None); },
		[](std::uint32_t bitness) { return Instruction::with_maskmovq(bitness, Register::MM2, Register::MM3, Register::FS); },
		[](std::uint32_t bitness) { return Instruction::with_maskmovdqu(bitness, Register::XMM2, Register::XMM3, Register::FS); },
		[](std::uint32_t bitness) { return Instruction::with_rep_outsb(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_outsw(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_outsd(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repe_scasb(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repe_scasw(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repe_scasd(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repe_scasq(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repne_scasb(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repne_scasw(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repne_scasd(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repne_scasq(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_lodsb(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_lodsw(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_lodsd(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_lodsq(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_insb(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_insw(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_insd(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_stosb(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_stosw(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_stosd(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_stosq(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repe_cmpsb(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repe_cmpsw(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repe_cmpsd(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repe_cmpsq(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repne_cmpsb(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repne_cmpsw(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repne_cmpsd(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_repne_cmpsq(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_movsb(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_movsw(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_movsd(bitness); },
		[](std::uint32_t bitness) { return Instruction::with_rep_movsq(bitness); },
	};
	create_fails_if_invalid_bitness_core(tests);
}

TEST_CASE("encoder/create_fails_if_invalid_bitness_vex") {
	const std::vector<Result<Instruction> (*)(std::uint32_t)> tests = {
		[](std::uint32_t bitness) { return Instruction::with_vmaskmovdqu(bitness, Register::XMM2, Register::XMM3, Register::FS); },
	};
	create_fails_if_invalid_bitness_core(tests);
}

TEST_CASE("encoder/encoding_instruction_requiring_opmask_fails_if_no_opmask") {
	const Instruction instr =
		Instruction::with2(Code::EVEX_Vpgatherdd_xmm_k1_vm32x, Register::XMM1, MemoryOperand::with_base_index_scale(Register::RDX, Register::XMM3, 4)).value();
	CHECK(!instr.has_op_mask());
	Encoder encoder(64);
	auto result = encoder.encode(instr, 0);
	REQUIRE_MSG(result.is_err(), "It should fail to encode an invalid instruction");
	CHECK_EQ(std::string(result.error().message()), std::string("The instruction must use an opmask register"));
}

TEST_CASE("encoder/try_create_imm_works") {
	// OpKind::Immediate8
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x80), static_cast<std::int32_t>(0xFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm8_imm8, Register::CL, imm).value();
		CHECK_EQ(instr.immediate8(), static_cast<std::uint8_t>(imm));
	}
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x81), static_cast<std::int32_t>(0x100)}) {
		CHECK(Instruction::with2(Code::Add_rm8_imm8, Register::CL, imm).is_err());
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x80), static_cast<std::int64_t>(0xFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm8_imm8, Register::CL, imm).value();
		CHECK_EQ(instr.immediate8(), static_cast<std::uint8_t>(imm));
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x81), static_cast<std::int64_t>(0x100)}) {
		CHECK(Instruction::with2(Code::Add_rm8_imm8, Register::CL, imm).is_err());
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(0xFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm8_imm8, Register::CL, imm).value();
		CHECK_EQ(static_cast<std::uint32_t>(instr.immediate8()), imm);
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0x100), static_cast<std::uint32_t>(0xFFFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm8_imm8, Register::CL, imm).is_err());
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0), static_cast<std::uint64_t>(0xFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm8_imm8, Register::CL, imm).value();
		CHECK_EQ(static_cast<std::uint64_t>(instr.immediate8()), imm);
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0x100), static_cast<std::uint64_t>(0xFFFF'FFFF), static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm8_imm8, Register::CL, imm).is_err());
	}

	// OpKind::Immediate8_2nd
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x80), static_cast<std::int32_t>(0xFF)}) {
		const Instruction instr = Instruction::with2(Code::Enterq_imm16_imm8, 0, imm).value();
		CHECK_EQ(instr.immediate8_2nd(), static_cast<std::uint8_t>(imm));
	}
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x81), static_cast<std::int32_t>(0x100)}) {
		CHECK(Instruction::with2(Code::Enterq_imm16_imm8, 0, imm).is_err());
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(0xFF)}) {
		const Instruction instr = Instruction::with2(Code::Enterq_imm16_imm8, 0U, imm).value();
		CHECK_EQ(static_cast<std::uint32_t>(instr.immediate8_2nd()), imm);
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0x100), static_cast<std::uint32_t>(0xFFFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Enterq_imm16_imm8, 0U, imm).is_err());
	}

	// OpKind::Immediate8to16
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x80), static_cast<std::int32_t>(0x7F)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm16_imm8, Register::CX, imm).value();
		CHECK_EQ(static_cast<std::int32_t>(instr.immediate8to16()), imm);
	}
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x81), static_cast<std::int32_t>(0x80)}) {
		CHECK(Instruction::with2(Code::Add_rm16_imm8, Register::CX, imm).is_err());
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x80), static_cast<std::int64_t>(0x7F)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm16_imm8, Register::CX, imm).value();
		CHECK_EQ(static_cast<std::int64_t>(instr.immediate8to16()), imm);
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x81), static_cast<std::int64_t>(0x80)}) {
		CHECK(Instruction::with2(Code::Add_rm16_imm8, Register::CX, imm).is_err());
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(0x7F), static_cast<std::uint32_t>(0xFF80), static_cast<std::uint32_t>(0xFFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm16_imm8, Register::CX, imm).value();
		CHECK_EQ(static_cast<std::uint32_t>(static_cast<std::uint16_t>(instr.immediate8to16())), imm);
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0x80), static_cast<std::uint32_t>(0xFF7F), static_cast<std::uint32_t>(0x0001'0000), static_cast<std::uint32_t>(0xFFFF'FFFF), static_cast<std::uint32_t>(0x0001'FF80), static_cast<std::uint32_t>(0x0001'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm16_imm8, Register::CX, imm).is_err());
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0), static_cast<std::uint64_t>(0x7F), static_cast<std::uint64_t>(0xFF80), static_cast<std::uint64_t>(0xFFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm16_imm8, Register::CX, imm).value();
		CHECK_EQ(static_cast<std::uint64_t>(static_cast<std::uint16_t>(instr.immediate8to16())), imm);
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0x80), static_cast<std::uint64_t>(0xFF7F), static_cast<std::uint64_t>(0x0001'0000), static_cast<std::uint64_t>(0xFFFF'FFFF), static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FFFF), static_cast<std::uint64_t>(0x0001'FF80), static_cast<std::uint64_t>(0x0001'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm16_imm8, Register::CX, imm).is_err());
	}

	// OpKind::Immediate8to32
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x80), static_cast<std::int32_t>(0x7F)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm32_imm8, Register::ECX, imm).value();
		CHECK_EQ(instr.immediate8to32(), imm);
	}
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x81), static_cast<std::int32_t>(0x80)}) {
		CHECK(Instruction::with2(Code::Add_rm32_imm8, Register::ECX, imm).is_err());
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x80), static_cast<std::int64_t>(0x7F)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm32_imm8, Register::ECX, imm).value();
		CHECK_EQ(static_cast<std::int64_t>(instr.immediate8to32()), imm);
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x81), static_cast<std::int64_t>(0x80)}) {
		CHECK(Instruction::with2(Code::Add_rm32_imm8, Register::ECX, imm).is_err());
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(0x7F), static_cast<std::uint32_t>(0xFFFF'FF80), static_cast<std::uint32_t>(0xFFFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm32_imm8, Register::ECX, imm).value();
		CHECK_EQ(static_cast<std::uint32_t>(instr.immediate8to32()), imm);
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0x80), static_cast<std::uint32_t>(0xFFFF'FF7F)}) {
		CHECK(Instruction::with2(Code::Add_rm32_imm8, Register::ECX, imm).is_err());
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0), static_cast<std::uint64_t>(0x7F), static_cast<std::uint64_t>(0xFFFF'FF80), static_cast<std::uint64_t>(0xFFFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm32_imm8, Register::ECX, imm).value();
		CHECK_EQ(static_cast<std::uint64_t>(static_cast<std::uint32_t>(instr.immediate8to32())), imm);
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0x80), static_cast<std::uint64_t>(0xFFFF'FF7F), static_cast<std::uint64_t>(0x0001'0000'0000), static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FFFF), static_cast<std::uint64_t>(0x0001'FFFF'FF80), static_cast<std::uint64_t>(0x0001'FFFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm32_imm8, Register::ECX, imm).is_err());
	}

	// OpKind::Immediate8to64
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x80), static_cast<std::int32_t>(0x7F)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm64_imm8, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate8to64(), static_cast<std::int64_t>(imm));
	}
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x81), static_cast<std::int32_t>(0x80)}) {
		CHECK(Instruction::with2(Code::Add_rm64_imm8, Register::RCX, imm).is_err());
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x80), static_cast<std::int64_t>(0x7F)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm64_imm8, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate8to64(), imm);
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x81), static_cast<std::int64_t>(0x80)}) {
		CHECK(Instruction::with2(Code::Add_rm64_imm8, Register::RCX, imm).is_err());
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(0x7F)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm64_imm8, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate8to64(), static_cast<std::int64_t>(imm));
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0x80), static_cast<std::uint32_t>(0xFFFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm64_imm8, Register::RCX, imm).is_err());
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0), static_cast<std::uint64_t>(0x7F), static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FF80), static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm64_imm8, Register::RCX, imm).value();
		CHECK_EQ(static_cast<std::uint64_t>(instr.immediate8to64()), imm);
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0x80), static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FF7F)}) {
		CHECK(Instruction::with2(Code::Add_rm64_imm8, Register::RCX, imm).is_err());
	}

	// OpKind::Immediate32to64
	for (const std::int32_t imm : {static_cast<std::int32_t>((-INT64_C(0x8000'0000))), static_cast<std::int32_t>(0x7FFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm64_imm32, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate32to64(), static_cast<std::int64_t>(imm));
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>((-INT64_C(0x8000'0000))), static_cast<std::int64_t>(0x7FFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm64_imm32, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate32to64(), imm);
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>((-INT64_C(0x8000'0001))), static_cast<std::int64_t>(0x8000'0000)}) {
		CHECK(Instruction::with2(Code::Add_rm64_imm32, Register::RCX, imm).is_err());
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(0x7FFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm64_imm32, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate32to64(), static_cast<std::int64_t>(imm));
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0x8000'0000), static_cast<std::uint32_t>(0xFFFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm64_imm32, Register::RCX, imm).is_err());
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0), static_cast<std::uint64_t>(0x7FFF'FFFF), static_cast<std::uint64_t>(0xFFFF'FFFF'8000'0000), static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm64_imm32, Register::RCX, imm).value();
		CHECK_EQ(static_cast<std::uint64_t>(instr.immediate32to64()), imm);
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0x8000'0000), static_cast<std::uint64_t>(0x0001'0000'0000), static_cast<std::uint64_t>(0xFFFF'FFFF'7FFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm64_imm32, Register::RCX, imm).is_err());
	}

	// OpKind::Immediate16
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x8000), static_cast<std::int32_t>(0xFFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm16_imm16, Register::CX, imm).value();
		CHECK_EQ(instr.immediate16(), static_cast<std::uint16_t>(imm));
	}
	for (const std::int32_t imm : {static_cast<std::int32_t>(-0x8001), static_cast<std::int32_t>(0x0001'0000)}) {
		CHECK(Instruction::with2(Code::Add_rm16_imm16, Register::CX, imm).is_err());
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x8000), static_cast<std::int64_t>(0xFFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm16_imm16, Register::CX, imm).value();
		CHECK_EQ(instr.immediate16(), static_cast<std::uint16_t>(imm));
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(-0x8001), static_cast<std::int64_t>(0x0001'0000)}) {
		CHECK(Instruction::with2(Code::Add_rm16_imm16, Register::CX, imm).is_err());
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(0xFFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm16_imm16, Register::CX, imm).value();
		CHECK_EQ(static_cast<std::uint32_t>(instr.immediate16()), imm);
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0x0001'0000), static_cast<std::uint32_t>(0xFFFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm16_imm16, Register::CX, imm).is_err());
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0), static_cast<std::uint64_t>(0xFFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm16_imm16, Register::CX, imm).value();
		CHECK_EQ(static_cast<std::uint64_t>(instr.immediate16()), imm);
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0x0001'0000), static_cast<std::uint64_t>(0xFFFF'FFFF), static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm16_imm16, Register::CX, imm).is_err());
	}

	// OpKind::Immediate32
	for (const std::int32_t imm : {static_cast<std::int32_t>((-INT64_C(0x8000'0000))), static_cast<std::int32_t>(0x7FFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm32_imm32, Register::ECX, imm).value();
		CHECK_EQ(instr.immediate32(), static_cast<std::uint32_t>(imm));
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>((-INT64_C(0x8000'0000))), static_cast<std::int64_t>(0xFFFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm32_imm32, Register::ECX, imm).value();
		CHECK_EQ(instr.immediate32(), static_cast<std::uint32_t>(imm));
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>((-INT64_C(0x8000'0001))), static_cast<std::int64_t>(0x0001'0000'0000)}) {
		CHECK(Instruction::with2(Code::Add_rm32_imm32, Register::ECX, imm).is_err());
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(0xFFFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm32_imm32, Register::ECX, imm).value();
		CHECK_EQ(instr.immediate32(), imm);
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0), static_cast<std::uint64_t>(0xFFFF'FFFF)}) {
		const Instruction instr = Instruction::with2(Code::Add_rm32_imm32, Register::ECX, imm).value();
		CHECK_EQ(static_cast<std::uint64_t>(instr.immediate32()), imm);
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(0x0001'0000'0000), static_cast<std::uint64_t>(0xFFFF'FFFF'FFFF'FFFF)}) {
		CHECK(Instruction::with2(Code::Add_rm32_imm32, Register::ECX, imm).is_err());
	}

	// OpKind::Immediate64
	for (const std::int32_t imm : {static_cast<std::int32_t>(std::numeric_limits<std::int32_t>::min()), static_cast<std::int32_t>(std::numeric_limits<std::int32_t>::max())}) {
		const Instruction instr = Instruction::with2(Code::Mov_r64_imm64, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate64(), static_cast<std::uint64_t>(imm));
	}
	for (const std::int64_t imm : {static_cast<std::int64_t>(std::numeric_limits<std::int64_t>::min()), static_cast<std::int64_t>(std::numeric_limits<std::int64_t>::max())}) {
		const Instruction instr = Instruction::with2(Code::Mov_r64_imm64, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate64(), static_cast<std::uint64_t>(imm));
	}
	for (const std::uint32_t imm : {static_cast<std::uint32_t>(std::uint32_t{0}), static_cast<std::uint32_t>(std::numeric_limits<std::uint32_t>::max())}) {
		const Instruction instr = Instruction::with2(Code::Mov_r64_imm64, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate64(), static_cast<std::uint64_t>(imm));
	}
	for (const std::uint64_t imm : {static_cast<std::uint64_t>(std::uint64_t{0}), static_cast<std::uint64_t>(std::numeric_limits<std::uint64_t>::max())}) {
		const Instruction instr = Instruction::with2(Code::Mov_r64_imm64, Register::RCX, imm).value();
		CHECK_EQ(instr.immediate64(), imm);
	}
}

TEST_CASE("encoder/encode_invalid_len_dw_dd_dq") {
	Encoder encoder(64);

	Instruction dw = Instruction::try_with_declare_word_1(1).value();
	dw.set_declare_data_len(8);
	CHECK_EQ(encoder.encode(dw, 0).value(), static_cast<std::size_t>(16));
	dw.set_declare_data_len(8 + 1);
	CHECK(encoder.encode(dw, 0).is_err());

	Instruction dd = Instruction::try_with_declare_dword_1(1).value();
	dd.set_declare_data_len(4);
	CHECK_EQ(encoder.encode(dd, 0).value(), static_cast<std::size_t>(16));
	dd.set_declare_data_len(4 + 1);
	CHECK(encoder.encode(dd, 0).is_err());

	Instruction dq = Instruction::try_with_declare_qword_1(1).value();
	dq.set_declare_data_len(2);
	CHECK_EQ(encoder.encode(dq, 0).value(), static_cast<std::size_t>(16));
	dq.set_declare_data_len(2 + 1);
	CHECK(encoder.encode(dq, 0).is_err());
}

} // namespace iced_x86::tests
