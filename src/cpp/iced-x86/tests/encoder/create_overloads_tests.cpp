// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// C++ only: `Instruction::with1()`..`with5()` with other integer types (eg. `long long`, `unsigned long`) and the
// `std::initializer_list`/`std::vector`/C array overloads of `Instruction::with_declare_*()`

#include "iced_x86/code.hpp"
#include "iced_x86/iced_error.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/memory_operand.hpp"
#include "iced_x86/register.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace iced_x86::tests {

// The exact type that `T` is converted to (types smaller than `int` are promoted to `int`)
template <typename T>
using ExactInt = std::conditional_t<(sizeof(T) < sizeof(int)), std::int32_t,
									std::conditional_t<sizeof(T) == 8, std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>,
													   std::conditional_t<std::is_signed_v<T>, std::int32_t, std::uint32_t>>>;

static void check_same(const Result<Instruction>& r1, const Result<Instruction>& r2) {
	REQUIRE(r1.is_ok());
	REQUIRE(r2.is_ok());
	CHECK(r1->eq_all_bits(*r2));
}

// Any integer type
template <typename T>
static void check_with_64(T value) {
	const auto v = static_cast<ExactInt<T>>(value);
	check_same(Instruction::with2(Code::Mov_r64_imm64, Register::RAX, value), Instruction::with2(Code::Mov_r64_imm64, Register::RAX, v));
	const auto result = Instruction::with2(Code::Mov_r64_imm64, Register::RAX, value);
	CHECK_EQ(result->immediate64(), static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// Integer types that are 32-bit (after conversion)
template <typename T>
static void check_with_32(T value) {
	if constexpr (sizeof(T) <= 4) {
		const auto v = static_cast<ExactInt<T>>(value);
		const MemoryOperand mem(Register::RAX, Register::None, 1, 0x10, 1, false, Register::None);
		check_same(Instruction::with1(Code::Pushq_imm32, value), Instruction::with1(Code::Pushq_imm32, v));
		check_same(Instruction::with2(Code::Add_rm32_imm32, Register::ECX, value), Instruction::with2(Code::Add_rm32_imm32, Register::ECX, v));
		check_same(Instruction::with2(Code::Add_rm32_imm32, mem, value), Instruction::with2(Code::Add_rm32_imm32, mem, v));
		check_same(Instruction::with2(Code::Enterq_imm16_imm8, value, value), Instruction::with2(Code::Enterq_imm16_imm8, v, v));
		check_same(Instruction::with3(Code::Imul_r32_rm32_imm32, Register::ECX, Register::EDX, value),
				   Instruction::with3(Code::Imul_r32_rm32_imm32, Register::ECX, Register::EDX, v));
		check_same(Instruction::with3(Code::Imul_r32_rm32_imm32, Register::ECX, mem, value),
				   Instruction::with3(Code::Imul_r32_rm32_imm32, Register::ECX, mem, v));
		check_same(Instruction::with4(Code::EVEX_Vpternlogd_xmm_k1z_xmm_xmmm128b32_imm8, Register::XMM1, Register::XMM2, Register::XMM3, value),
				   Instruction::with4(Code::EVEX_Vpternlogd_xmm_k1z_xmm_xmmm128b32_imm8, Register::XMM1, Register::XMM2, Register::XMM3, v));
		check_same(Instruction::with5(Code::VEX_Vpermil2ps_xmm_xmm_xmmm128_xmm_imm4, Register::XMM1, Register::XMM2, Register::XMM3,
									  Register::XMM4, value),
				   Instruction::with5(Code::VEX_Vpermil2ps_xmm_xmm_xmmm128_xmm_imm4, Register::XMM1, Register::XMM2, Register::XMM3,
									  Register::XMM4, v));
	}
}

TEST_CASE("encoder/create_with_int_args") {
	check_with_64(0x1234'5678'9ABC'DEF0ULL);
	check_with_64(0xFFFF'FFFF'FFFF'FFFFULL);
	check_with_64(-1LL);
	check_with_64(static_cast<long>(-2));
	check_with_64(static_cast<unsigned long>(0xFFFF'FFFFUL));
	check_with_64(static_cast<short>(-3));
	check_with_64(static_cast<unsigned short>(0xFFFF));
	check_with_64(static_cast<std::size_t>(0x1234));
	check_with_64(static_cast<std::ptrdiff_t>(-0x1234));
	check_with_64(-1);
	check_with_64(0xFFFF'FFFFU);
	check_with_64(std::int64_t{-1});
	check_with_64(std::uint64_t{0x1234'5678'9ABC'DEF0});

	check_with_32(1);
	check_with_32(1U);
	check_with_32(static_cast<short>(1));
	check_with_32(static_cast<unsigned short>(1));
	check_with_32(static_cast<signed char>(1));
	check_with_32(static_cast<unsigned char>(1));
	// 32-bit on some platforms
	check_with_32(static_cast<long>(1));
	check_with_32(static_cast<unsigned long>(1));
	check_with_32(static_cast<std::size_t>(1));

	auto instr = Instruction::with2(Code::Mov_r64_imm64, Register::RAX, 0x1234'5678'9ABC'DEF0ULL);
	REQUIRE(instr.has_value());
	CHECK(instr->code() == Code::Mov_r64_imm64);
	CHECK(instr->op0_register() == Register::RAX);
	CHECK_EQ(instr->immediate64(), UINT64_C(0x1234'5678'9ABC'DEF0));
	instr = Instruction::with2(Code::Mov_r64_imm64, Register::RAX, -1LL);
	CHECK_EQ(instr->immediate64(), UINT64_C(0xFFFF'FFFF'FFFF'FFFF));
}

TEST_CASE("encoder/create_declare_data_overloads") {
	static const std::uint8_t b[] = {0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C, 0x86, 0x32, 0xFE, 0x4F, 0x34, 0x27, 0xAA, 0x08};
	static const std::uint16_t w[] = {0x77A9, 0xCE9D, 0x5505, 0x426C, 0x8632, 0xFE4F, 0x3427, 0xAA08};
	static const std::uint32_t d[] = {0x77A9'CE9D, 0x5505'426C, 0x8632'FE4F, 0x3427'AA08};
	static const std::uint64_t q[] = {0x77A9'CE9D'5505'426C, 0x8632'FE4F'3427'AA08};

	// C arrays, std::vector, std::initializer_list are the same as (ptr, size)
	check_same(Instruction::with_declare_byte(b), Instruction::with_declare_byte(b, 16));
	check_same(Instruction::with_declare_byte(std::vector<std::uint8_t>(b, b + 3)), Instruction::with_declare_byte(b, 3));
	check_same(Instruction::with_declare_byte({0x77, 0xA9, 0xCE}), Instruction::with_declare_byte(b, 3));
	check_same(Instruction::with_declare_word_slice_u8(b), Instruction::with_declare_word_slice_u8(b, 16));
	check_same(Instruction::with_declare_word_slice_u8(std::vector<std::uint8_t>(b, b + 4)), Instruction::with_declare_word_slice_u8(b, 4));
	check_same(Instruction::with_declare_word_slice_u8({0x77, 0xA9}), Instruction::with_declare_word_slice_u8(b, 2));
	check_same(Instruction::with_declare_word(w), Instruction::with_declare_word(w, 8));
	check_same(Instruction::with_declare_word(std::vector<std::uint16_t>(w, w + 3)), Instruction::with_declare_word(w, 3));
	check_same(Instruction::with_declare_word({0x77A9, 0xCE9D}), Instruction::with_declare_word(w, 2));
	check_same(Instruction::with_declare_dword_slice_u8(b), Instruction::with_declare_dword_slice_u8(b, 16));
	check_same(Instruction::with_declare_dword_slice_u8(std::vector<std::uint8_t>(b, b + 8)), Instruction::with_declare_dword_slice_u8(b, 8));
	check_same(Instruction::with_declare_dword_slice_u8({0x77, 0xA9, 0xCE, 0x9D}), Instruction::with_declare_dword_slice_u8(b, 4));
	check_same(Instruction::with_declare_dword(d), Instruction::with_declare_dword(d, 4));
	check_same(Instruction::with_declare_dword(std::vector<std::uint32_t>(d, d + 3)), Instruction::with_declare_dword(d, 3));
	check_same(Instruction::with_declare_dword({0x77A9'CE9D, 0x5505'426C}), Instruction::with_declare_dword(d, 2));
	check_same(Instruction::with_declare_qword_slice_u8(b), Instruction::with_declare_qword_slice_u8(b, 16));
	check_same(Instruction::with_declare_qword_slice_u8(std::vector<std::uint8_t>(b, b + 8)), Instruction::with_declare_qword_slice_u8(b, 8));
	check_same(Instruction::with_declare_qword_slice_u8({0x77, 0xA9, 0xCE, 0x9D, 0x55, 0x05, 0x42, 0x6C}),
			   Instruction::with_declare_qword_slice_u8(b, 8));
	check_same(Instruction::with_declare_qword(q), Instruction::with_declare_qword(q, 2));
	check_same(Instruction::with_declare_qword(std::vector<std::uint64_t>(q, q + 1)), Instruction::with_declare_qword(q, 1));
	check_same(Instruction::with_declare_qword({0x77A9'CE9D'5505'426CULL}), Instruction::with_declare_qword(q, 1));

	auto instr = Instruction::with_declare_word(w);
	REQUIRE(instr.has_value());
	CHECK(instr->code() == Code::DeclareWord);
	CHECK_EQ(instr->declare_data_len(), static_cast<std::size_t>(8));
	CHECK_EQ(instr->get_declare_word_value(7), std::uint16_t{0xAA08});

	// Invalid sizes still fail
	static const std::uint8_t too_many_bytes[17] = {};
	static const std::uint64_t too_many_qwords[3] = {};
	CHECK(Instruction::with_declare_byte(too_many_bytes).is_err());
	CHECK(Instruction::with_declare_qword(too_many_qwords).is_err());
	CHECK(Instruction::with_declare_byte(std::vector<std::uint8_t>()).is_err());
	CHECK(Instruction::with_declare_dword_slice_u8({0x77, 0xA9, 0xCE}).is_err());
}

} // namespace iced_x86::tests
