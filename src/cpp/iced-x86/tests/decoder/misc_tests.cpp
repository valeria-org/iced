// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's decoder/tests/misc_tests.rs

#include "test_framework.hpp"
#include "test_utils.hpp"
#include "test_utils/abort_utils.hpp"
#include "test_utils/decoder_test_utils.hpp"
#include "test_utils/from_str_conv.hpp"

#include "iced_x86/decoder.hpp"
#include "iced_x86/iced_constants.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace iced_x86::tests {

namespace {

const std::uint8_t NOP_BYTES[] = {0x90};
const std::uint8_t POSITION_BYTES[] = {0x23, 0x18, 0x48, 0x89, 0xCE};

void decoder_new_panics(std::uint32_t bitness) { Decoder decoder(bitness, NOP_BYTES, sizeof(NOP_BYTES), DecoderOptions::NONE); }

void decoder_try_new_fails(std::uint32_t bitness) { CHECK(Decoder::try_new(bitness, NOP_BYTES, sizeof(NOP_BYTES), DecoderOptions::NONE).is_err()); }

void verify_constant_offsets(const ConstantOffsets& expected, const ConstantOffsets& actual) {
	CHECK_EQ(actual.immediate_offset(), expected.immediate_offset());
	CHECK_EQ(actual.immediate_size(), expected.immediate_size());
	CHECK_EQ(actual.immediate_offset2(), expected.immediate_offset2());
	CHECK_EQ(actual.immediate_size2(), expected.immediate_size2());
	CHECK_EQ(actual.displacement_offset(), expected.displacement_offset());
	CHECK_EQ(actual.displacement_size(), expected.displacement_size());
}

} // namespace

TEST_CASE("decoder/decoder_new_panics_0") { CHECK(aborts([] { decoder_new_panics(0); })); }
TEST_CASE("decoder/decoder_new_panics_128") { CHECK(aborts([] { decoder_new_panics(128); })); }

TEST_CASE("decoder/decoder_try_new_fails_0") { decoder_try_new_fails(0); }
TEST_CASE("decoder/decoder_try_new_fails_128") { decoder_try_new_fails(128); }

TEST_CASE("decoder/decoder_try_new_succeeds_16") {
	auto decoder = Decoder::try_new(16, NOP_BYTES, sizeof(NOP_BYTES), DecoderOptions::NONE);
	REQUIRE(decoder.is_ok());
	CHECK_EQ(decoder.value().decode().code(), Code::Nopw);
}

TEST_CASE("decoder/decoder_try_new_succeeds_32") {
	auto decoder = Decoder::try_new(32, NOP_BYTES, sizeof(NOP_BYTES), DecoderOptions::NONE);
	REQUIRE(decoder.is_ok());
	CHECK_EQ(decoder.value().decode().code(), Code::Nopd);
}

TEST_CASE("decoder/decoder_try_new_succeeds_64") {
	auto decoder = Decoder::try_new(64, NOP_BYTES, sizeof(NOP_BYTES), DecoderOptions::NONE);
	REQUIRE(decoder.is_ok());
	CHECK_EQ(decoder.value().decode().code(), Code::Nopd);
}

TEST_CASE("decoder/decode_multiple_instrs_with_one_instance") {
	auto tests = decoder_tests(true, true);

	using Key = std::pair<std::uint32_t, std::uint32_t>;
	std::map<Key, std::vector<std::uint8_t>> bytes_map;
	std::map<Key, Decoder> map;

	for (const auto& tc : tests) {
		Key key(tc.bitness(), tc.decoder_options());
		auto& vec = bytes_map[key];
		auto bytes = to_vec_u8(tc.hex_bytes());
		vec.insert(vec.end(), bytes.begin(), bytes.end());
	}

	for (const auto& tc : tests) {
		Key key(tc.bitness(), tc.decoder_options());
		const auto& vec = bytes_map.at(key);
		if (map.find(key) == map.end())
			map.emplace(key, Decoder(tc.bitness(), vec.data(), vec.size(), tc.decoder_options()));
	}

	Instruction instr2;
	for (const auto& tc : tests) {
		auto bytes = to_vec_u8(tc.hex_bytes());
		auto created = create_decoder(tc.bitness(), bytes, tc.ip(), tc.decoder_options());
		Decoder& decoder = created.decoder;
		Key key(tc.bitness(), tc.decoder_options());
		Decoder& decoder_all = map.at(key);
		std::uint64_t ip = decoder.ip();
		decoder_all.set_ip(ip);

		std::size_t position = decoder_all.position();
		Instruction instr1 = decoder.decode();
		decoder_all.decode_out(instr2);
		ConstantOffsets co1 = decoder.get_constant_offsets(instr1);
		ConstantOffsets co2 = decoder_all.get_constant_offsets(instr2);
		if (instr1.is_invalid()) {
			// decoder_all has a bigger buffer and can decode more bytes.
			// It sometimes happens that instr1 is invalid but the full buffer is a valid instruction.
			// In that case, just ignore it.
			REQUIRE(decoder_all.try_set_position(position + bytes.size()).is_ok());
			if (!instr2.is_invalid())
				continue;
			std::size_t len = std::min<std::size_t>(bytes.size(), IcedConstants::MAX_INSTRUCTION_LENGTH);
			instr2.set_len(len);
			instr2.set_next_ip(ip + len);
		}
		CHECK_MSG(instr1.code() == instr2.code(), tc.hex_bytes());
		CHECK_MSG(instr1.eq_all_bits(instr2), tc.hex_bytes());
		CHECK_MSG(instr2.eq_all_bits(instr1), tc.hex_bytes());
		verify_constant_offsets(co1, co2);
	}
}

TEST_CASE("decoder/position") {
	constexpr std::uint32_t BITNESS = 64;
	const auto& bytes = POSITION_BYTES;
	auto decoder = Decoder::with_ip(BITNESS, bytes, sizeof(bytes), get_default_ip(BITNESS), DecoderOptions::NONE);

	CHECK(decoder.can_decode());
	CHECK_EQ(decoder.position(), 0U);
	CHECK_EQ(decoder.max_position(), sizeof(bytes));

	Instruction instr_a1 = decoder.decode();
	CHECK_EQ(instr_a1.code(), Code::And_r32_rm32);

	CHECK(decoder.can_decode());
	CHECK_EQ(decoder.position(), 2U);
	CHECK_EQ(decoder.max_position(), sizeof(bytes));

	Instruction instr_b1 = decoder.decode();
	CHECK_EQ(instr_b1.code(), Code::Mov_rm64_r64);

	CHECK(!decoder.can_decode());
	CHECK_EQ(decoder.position(), 5U);
	CHECK_EQ(decoder.max_position(), sizeof(bytes));

	decoder.set_ip(get_default_ip(BITNESS) + 2);
	CHECK_EQ(decoder.position(), 5U);
	REQUIRE(decoder.try_set_position(2).is_ok());
	CHECK(decoder.can_decode());
	CHECK_EQ(decoder.position(), 2U);
	CHECK_EQ(decoder.max_position(), sizeof(bytes));

	Instruction instr_b2 = decoder.decode();
	CHECK_EQ(instr_b2.code(), Code::Mov_rm64_r64);

	decoder.set_ip(get_default_ip(BITNESS));
	CHECK_EQ(decoder.position(), 5U);
	REQUIRE(decoder.try_set_position(0).is_ok());
	CHECK(decoder.can_decode());
	CHECK_EQ(decoder.position(), 0U);
	CHECK_EQ(decoder.max_position(), sizeof(bytes));

	Instruction instr_a2 = decoder.decode();
	CHECK_EQ(instr_a2.code(), Code::And_r32_rm32);

	CHECK(instr_a1.eq_all_bits(instr_a2));
	CHECK(instr_b1.eq_all_bits(instr_b2));
}

TEST_CASE("decoder/set_position_valid_position") {
	const auto& bytes = POSITION_BYTES;
	Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
	for (std::size_t i = 0; i < sizeof(bytes) + 1; i++) {
		REQUIRE(decoder.set_position(i).is_ok());
		CHECK_EQ(decoder.position(), i);
	}
	for (std::size_t i = sizeof(bytes) + 1; i-- > 0;) {
		REQUIRE(decoder.set_position(i).is_ok());
		CHECK_EQ(decoder.position(), i);
	}
	Decoder decoder2(64, nullptr, 0, DecoderOptions::NONE);
	REQUIRE(decoder2.set_position(0).is_ok());
	CHECK_EQ(decoder2.position(), 0U);
}

TEST_CASE("decoder/try_set_position_valid_position") {
	const auto& bytes = POSITION_BYTES;
	Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
	for (std::size_t i = 0; i < sizeof(bytes) + 1; i++) {
		REQUIRE(decoder.try_set_position(i).is_ok());
		CHECK_EQ(decoder.position(), i);
	}
	for (std::size_t i = sizeof(bytes) + 1; i-- > 0;) {
		REQUIRE(decoder.try_set_position(i).is_ok());
		CHECK_EQ(decoder.position(), i);
	}
	Decoder decoder2(64, nullptr, 0, DecoderOptions::NONE);
	REQUIRE(decoder2.try_set_position(0).is_ok());
	CHECK_EQ(decoder2.position(), 0U);
}

TEST_CASE("decoder/set_position_panics_if_invalid") {
	CHECK(aborts([] {
		const auto& bytes = POSITION_BYTES;
		Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
		decoder.set_position(sizeof(bytes) + 1).value();
	}));
}

TEST_CASE("decoder/try_set_position_fails_if_invalid") {
	const auto& bytes = POSITION_BYTES;
	Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
	CHECK(decoder.try_set_position(sizeof(bytes) + 1).is_err());
}

// Rust: `for instr in decoder` (into_iter())
TEST_CASE("decoder/decoder_for_loop_into_iter") {
	const auto& bytes = POSITION_BYTES;
	Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
	std::vector<Instruction> instrs;
	for (const Instruction& instr : decoder)
		instrs.push_back(instr);
	REQUIRE_EQ(instrs.size(), 2U);
	CHECK_EQ(instrs[0].code(), Code::And_r32_rm32);
	CHECK_EQ(instrs[1].code(), Code::Mov_rm64_r64);
}

// Rust: `for instr in &mut decoder`
TEST_CASE("decoder/decoder_for_loop_ref_mut_decoder") {
	const auto& bytes = POSITION_BYTES;
	auto decoder = Decoder::with_ip(64, bytes, sizeof(bytes), 0x1234'5678'9ABC'DEF0ULL, DecoderOptions::NONE);
	std::vector<Instruction> instrs;
	for (auto instr : decoder)
		instrs.push_back(instr);
	CHECK_EQ(decoder.ip(), 0x1234'5678'9ABC'DEF5ULL);
	CHECK(!decoder.can_decode());
	CHECK_EQ(decoder.position(), 5U);
	REQUIRE_EQ(instrs.size(), 2U);
	CHECK_EQ(instrs[0].code(), Code::And_r32_rm32);
	CHECK_EQ(instrs[1].code(), Code::Mov_rm64_r64);
}

// Rust: `decoder.iter()`
TEST_CASE("decoder/decoder_for_loop_decoder_iter") {
	const auto& bytes = POSITION_BYTES;
	auto decoder = Decoder::with_ip(64, bytes, sizeof(bytes), 0x1234'5678'9ABC'DEF0ULL, DecoderOptions::NONE);
	std::vector<Instruction> instrs;
	for (auto it = decoder.begin(), end = decoder.end(); it != end; ++it)
		instrs.push_back(*it);
	CHECK_EQ(decoder.ip(), 0x1234'5678'9ABC'DEF5ULL);
	CHECK(!decoder.can_decode());
	CHECK_EQ(decoder.position(), 5U);
	REQUIRE_EQ(instrs.size(), 2U);
	CHECK_EQ(instrs[0].code(), Code::And_r32_rm32);
	CHECK_EQ(instrs[1].code(), Code::Mov_rm64_r64);
	CHECK(decoder.begin() == decoder.end());
}

TEST_CASE("decoder/decode_ip_xxxxxxxxffffffff") {
	auto decoder = Decoder::with_ip(64, NOP_BYTES, sizeof(NOP_BYTES), 0x1234'5678'FFFF'FFFFULL, DecoderOptions::NONE);
	(void)decoder.decode();
	CHECK_EQ(decoder.ip(), 0x1234'5679'0000'0000ULL);
}

TEST_CASE("decoder/decode_with_too_few_bytes_left") {
	for (const auto& tc : decoder_tests(true, false)) {
		auto bytes = to_vec_u8(tc.hex_bytes());
		for (std::size_t i = 0; i + 1 < bytes.size(); i++) {
			auto decoder = Decoder::with_ip(tc.bitness(), bytes.data(), i, 0x1000, tc.decoder_options());
			Instruction instr = decoder.decode();
			CHECK_EQ(decoder.ip(), 0x1000 + static_cast<std::uint64_t>(i));
			CHECK_MSG(instr.code() == Code::INVALID, tc.hex_bytes());
			CHECK_EQ(decoder.last_error(), DecoderError::NoMoreBytes);
		}
	}
}

// Rust creates the instructions with `Instruction::with2()` (encoder feature). They're decoded here instead so the test
// doesn't depend on the encoder: `mov rax,rcx` and `mov rax,rdx` (Code::Mov_r64_rm64)
TEST_CASE("decoder/instruction_operator_eq_neq") {
	static const std::uint8_t BYTES[] = {0x48, 0x8B, 0xC1, 0x48, 0x8B, 0xC2};
	Decoder decoder(64, BYTES, sizeof(BYTES), DecoderOptions::NONE);
	auto instr1a = decoder.decode();
	auto instr1b = instr1a;
	REQUIRE_EQ(decoder.position(), 3U);
	REQUIRE(decoder.set_position(0).is_ok());
	decoder.set_ip(0);
	CHECK(decoder.decode() == instr1a);
	auto instr2 = decoder.decode();
	CHECK_EQ(instr1a.code(), Code::Mov_r64_rm64);
	CHECK_EQ(instr2.code(), Code::Mov_r64_rm64);
	CHECK_EQ(instr1a.op1_register(), Register::RCX);
	CHECK_EQ(instr2.op1_register(), Register::RDX);
	CHECK_EQ(instr1a == instr1b, true);
	CHECK_EQ(instr1a == instr2, false);
	CHECK_EQ(instr1a != instr2, true);
	CHECK_EQ(instr1a != instr1b, false);
}

// Tests the vector overloads and that `decode()` and `decode_out()` return the same instruction
TEST_CASE("decoder/decode_vs_decode_out") {
	std::vector<std::uint8_t> bytes(std::begin(POSITION_BYTES), std::end(POSITION_BYTES));
	Decoder decoder1(64, bytes, DecoderOptions::NONE);
	auto decoder2 = Decoder::with_ip(64, bytes, 0, DecoderOptions::NONE);
	auto decoder3 = Decoder::try_new(64, bytes, DecoderOptions::NONE);
	auto decoder4 = Decoder::try_with_ip(64, bytes, 0, DecoderOptions::NONE);
	REQUIRE(decoder3.is_ok());
	REQUIRE(decoder4.is_ok());
	while (decoder1.can_decode()) {
		Instruction instr1 = decoder1.decode();
		Instruction instr2;
		decoder2.decode_out(instr2);
		Instruction instr3 = decoder3.value().decode();
		Instruction instr4 = decoder4.value().decode();
		CHECK(instr1.eq_all_bits(instr2));
		CHECK(instr1.eq_all_bits(instr3));
		CHECK(instr1.eq_all_bits(instr4));
	}
	CHECK(!decoder2.can_decode());
	CHECK(!decoder3.value().can_decode());
	CHECK(!decoder4.value().can_decode());
}

// C++ only: C array / std::array overloads. The decoder doesn't copy the data so temporaries are rejected (deleted overloads).
static_assert(std::is_constructible_v<Decoder, std::uint32_t, const std::uint8_t (&)[4], std::uint32_t>, "");
static_assert(std::is_constructible_v<Decoder, std::uint32_t, std::uint8_t (&)[4], std::uint32_t>, "");
static_assert(!std::is_constructible_v<Decoder, std::uint32_t, std::uint8_t (&&)[4], std::uint32_t>, "");
static_assert(!std::is_constructible_v<Decoder, std::uint32_t, const std::uint8_t (&&)[4], std::uint32_t>, "");
static_assert(std::is_constructible_v<Decoder, std::uint32_t, const std::array<std::uint8_t, 4>&, std::uint32_t>, "");
static_assert(std::is_constructible_v<Decoder, std::uint32_t, std::array<std::uint8_t, 4>&, std::uint32_t>, "");
static_assert(!std::is_constructible_v<Decoder, std::uint32_t, std::array<std::uint8_t, 4>, std::uint32_t>, "");
static_assert(!std::is_constructible_v<Decoder, std::uint32_t, const std::array<std::uint8_t, 4>, std::uint32_t>, "");
static_assert(!std::is_constructible_v<Decoder, std::uint32_t, std::vector<std::uint8_t>, std::uint32_t>, "");

namespace {
// `true` if `Decoder::xxx(64, {0x90, 0x90}, ...)` (a temporary array) compiles
template <typename D, typename = void>
struct CanCreateFromBracedList : std::false_type {};
template <typename D>
struct CanCreateFromBracedList<D, std::void_t<decltype(D(64, {std::uint8_t{0x90}, std::uint8_t{0x90}}, 0))>> : std::true_type {};
template <typename D, typename = void>
struct CanWithIpFromBracedList : std::false_type {};
template <typename D>
struct CanWithIpFromBracedList<D, std::void_t<decltype(D::with_ip(64, {std::uint8_t{0x90}, std::uint8_t{0x90}}, 0, 0))>> : std::true_type {};
template <typename D, typename = void>
struct CanTryNewFromBracedList : std::false_type {};
template <typename D>
struct CanTryNewFromBracedList<D, std::void_t<decltype(D::try_new(64, {std::uint8_t{0x90}, std::uint8_t{0x90}}, 0))>> : std::true_type {};
template <typename D, typename = void>
struct CanTryWithIpFromBracedList : std::false_type {};
template <typename D>
struct CanTryWithIpFromBracedList<D, std::void_t<decltype(D::try_with_ip(64, {std::uint8_t{0x90}, std::uint8_t{0x90}}, 0, 0))>> : std::true_type {};
template <typename D, typename = void>
struct CanWithIpFromTempArray : std::false_type {};
template <typename D>
struct CanWithIpFromTempArray<D, std::void_t<decltype(D::with_ip(64, std::array<std::uint8_t, 2>{}, 0, 0))>> : std::true_type {};
template <typename D, typename = void>
struct CanWithIpFromArray : std::false_type {};
template <typename D>
struct CanWithIpFromArray<D, std::void_t<decltype(D::with_ip(64, std::declval<const std::array<std::uint8_t, 2>&>(), 0, 0))>> : std::true_type {};
} // namespace
static_assert(!CanCreateFromBracedList<Decoder>::value, "");
static_assert(!CanWithIpFromBracedList<Decoder>::value, "");
static_assert(!CanTryNewFromBracedList<Decoder>::value, "");
static_assert(!CanTryWithIpFromBracedList<Decoder>::value, "");
static_assert(!CanWithIpFromTempArray<Decoder>::value, "");
static_assert(CanWithIpFromArray<Decoder>::value, "");

TEST_CASE("decoder/c_array_and_std_array_overloads") {
	static const std::uint8_t bytes[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x90, 0xF0, 0x01, 0xCE};
	std::array<std::uint8_t, sizeof(bytes)> std_array{};
	std::copy(std::begin(bytes), std::end(bytes), std_array.begin());
	const std::uint64_t ip = 0x1234'5678'9ABC'DEF0;

	std::vector<Decoder> decoders;
	decoders.push_back(Decoder::with_ip(64, bytes, sizeof(bytes), ip, DecoderOptions::NO_INVALID_CHECK));
	decoders.push_back(Decoder(64, bytes, DecoderOptions::NO_INVALID_CHECK));
	decoders.back().set_ip(ip);
	decoders.push_back(Decoder(64, std_array, DecoderOptions::NO_INVALID_CHECK));
	decoders.back().set_ip(ip);
	decoders.push_back(Decoder::with_ip(64, bytes, ip, DecoderOptions::NO_INVALID_CHECK));
	decoders.push_back(Decoder::with_ip(64, std_array, ip, DecoderOptions::NO_INVALID_CHECK));
	decoders.push_back(*Decoder::try_new(64, bytes, DecoderOptions::NO_INVALID_CHECK));
	decoders.back().set_ip(ip);
	decoders.push_back(*Decoder::try_new(64, std_array, DecoderOptions::NO_INVALID_CHECK));
	decoders.back().set_ip(ip);
	decoders.push_back(*Decoder::try_with_ip(64, bytes, ip, DecoderOptions::NO_INVALID_CHECK));
	decoders.push_back(*Decoder::try_with_ip(64, std_array, ip, DecoderOptions::NO_INVALID_CHECK));
	CHECK(Decoder::try_new(128, bytes, DecoderOptions::NONE).is_err());
	CHECK(Decoder::try_with_ip(128, std_array, ip, DecoderOptions::NONE).is_err());

	for (Decoder& decoder : decoders) {
		CHECK_EQ(decoder.max_position(), sizeof(bytes));
		CHECK_EQ(decoder.ip(), ip);
	}
	std::size_t count = 0;
	while (decoders[0].can_decode()) {
		const Instruction expected = decoders[0].decode();
		count++;
		for (std::size_t i = 1; i < decoders.size(); i++) {
			REQUIRE(decoders[i].can_decode());
			CHECK(decoders[i].decode().eq_all_bits(expected));
		}
	}
	CHECK_EQ(count, static_cast<std::size_t>(4));
	for (Decoder& decoder : decoders)
		CHECK(!decoder.can_decode());
}

} // namespace iced_x86::tests
