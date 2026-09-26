// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// The instruction methods also accept integer types that aren't `std::int32_t`/`std::uint32_t`/`std::int64_t`/`std::uint64_t`
// (eg. `long long`, `unsigned long`, `std::size_t` on some platforms), see iced_x86/internal/int_arg.hpp

#include "code_asm/code_asm_test_utils.hpp"
#include "iced_x86/internal/int_arg.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace iced_x86::tests::code_asm_tests {

using namespace iced_x86::code_asm;

// Only the integer types that would be ambiguous use the templates, everything else must use the old overloads
static_assert(!iced_x86::internal::is_other_int_v<std::int32_t>, "");
static_assert(!iced_x86::internal::is_other_int_v<std::uint32_t>, "");
static_assert(!iced_x86::internal::is_other_int_v<std::int64_t>, "");
static_assert(!iced_x86::internal::is_other_int_v<std::uint64_t>, "");
static_assert(!iced_x86::internal::is_other_int_v<short>, "");
static_assert(!iced_x86::internal::is_other_int_v<unsigned short>, "");
static_assert(!iced_x86::internal::is_other_int_v<char>, "");
static_assert(!iced_x86::internal::is_other_int_v<bool>, "");
static_assert(!iced_x86::internal::is_other_int_v<Register>, "");
static_assert(!iced_x86::internal::is_other_int_v<AsmRegister64>, "");
static_assert(!iced_x86::internal::is_other_int_v<float>, "");
static_assert(sizeof(long long) != 8 || std::is_same_v<std::int64_t, long long> || iced_x86::internal::is_other_int_v<long long>, "");
static_assert(std::is_same_v<iced_x86::internal::IntArgType<long long>, std::int64_t>, "");
static_assert(std::is_same_v<iced_x86::internal::IntArgType<unsigned long long>, std::uint64_t>, "");
static_assert(std::is_same_v<iced_x86::internal::IntArgType<long>, std::conditional_t<sizeof(long) == 8, std::int64_t, std::int32_t>>, "");
static_assert(std::is_same_v<iced_x86::internal::IntArgType<unsigned long>, std::conditional_t<sizeof(long) == 8, std::uint64_t, std::uint32_t>>, "");

// The exact type that `T` is converted to (types smaller than `int` are promoted to `int`)
template <typename T>
using Exact = std::conditional_t<(sizeof(T) < sizeof(int)), std::int32_t,
								 std::conditional_t<sizeof(T) == 8, std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>,
													std::conditional_t<std::is_signed_v<T>, std::int32_t, std::uint32_t>>>;

template <typename F1, typename F2>
static void check_same_instrs(std::uint32_t bitness, F1 create1, F2 create2) {
	CodeAssembler a1(bitness);
	CodeAssembler a2(bitness);
	create1(a1);
	create2(a2);
	REQUIRE(!a1.has_error());
	REQUIRE(!a2.has_error());
	REQUIRE_EQ(a1.instructions().size(), a2.instructions().size());
	REQUIRE(a1.instructions().size() > 0);
	for (std::size_t i = 0; i < a1.instructions().size(); i++)
		CHECK(a1.instructions()[i].eq_all_bits(a2.instructions()[i]));
}

// `a.mov(rax, value)` with any integer type `T` must be identical to `a.mov(rax, Exact<T>{value})`
template <typename T>
static void check_mov_r64(T value) {
	check_same_instrs(
		64, [&](CodeAssembler& a) { a.mov(rax, value); }, [&](CodeAssembler& a) { a.mov(rax, static_cast<Exact<T>>(value)); });
}

template <typename T>
static void check_add_r32(T value) {
	check_same_instrs(
		64, [&](CodeAssembler& a) { a.add(ecx, value).add(dword_ptr(rax), value).imul_3(edx, ebx, value).push(value).enter(value, value); },
		[&](CodeAssembler& a) {
			const auto v = static_cast<Exact<T>>(value);
			a.add(ecx, v).add(dword_ptr(rax), v).imul_3(edx, ebx, v).push(v).enter(v, v);
		});
}

template <typename T>
static void check_add_r32_if_32bit(T value) {
	if constexpr (sizeof(T) <= 4)
		check_add_r32(value);
}

TEST_CASE("code_asm/int_args_mov_r64") {
	// 64-bit literals
	{
		CodeAssembler a(64);
		a.mov(rax, 0x1234'5678'9ABC'DEF0ULL);
		a.mov(rcx, -1LL);
		a.mov(rdx, 0x8000'0000'0000'0000ULL);
		a.mov(rbx, -0x7FFF'FFFF'FFFF'FFFFLL - 1);
		REQUIRE(!a.has_error());
		const auto& instrs = a.instructions();
		REQUIRE_EQ(instrs.size(), static_cast<std::size_t>(4));
		CHECK(instrs[0].code() == Code::Mov_r64_imm64);
		CHECK_EQ(instrs[0].immediate64(), UINT64_C(0x1234'5678'9ABC'DEF0));
		CHECK(instrs[1].code() == Code::Mov_r64_imm64);
		CHECK_EQ(instrs[1].immediate64(), UINT64_C(0xFFFF'FFFF'FFFF'FFFF));
		CHECK(instrs[2].code() == Code::Mov_r64_imm64);
		CHECK_EQ(instrs[2].immediate64(), UINT64_C(0x8000'0000'0000'0000));
		CHECK(instrs[3].code() == Code::Mov_r64_imm64);
		CHECK_EQ(instrs[3].immediate64(), UINT64_C(0x8000'0000'0000'0000));
	}

	check_mov_r64(0x1234'5678'9ABC'DEF0ULL);
	check_mov_r64(0xFFFF'FFFF'FFFF'FFFFULL);
	check_mov_r64(-1LL);
	check_mov_r64(0x7FFF'FFFFLL);
	check_mov_r64(static_cast<long>(-2));
	check_mov_r64(static_cast<long>(0x7FFF'FFFF));
	check_mov_r64(static_cast<unsigned long>(0xFFFF'FFFFUL));
	check_mov_r64(static_cast<std::size_t>(0x1234));
	check_mov_r64(static_cast<std::ptrdiff_t>(-0x1234));
	check_mov_r64(std::uint64_t{0x1234'5678'9ABC'DEF0});
	check_mov_r64(std::int64_t{-1});
	// int/unsigned int (only 64-bit overloads exist: the value is sign/zero extended)
	check_mov_r64(-1);
	check_mov_r64(0x7FFF'FFFF);
	check_mov_r64(0xFFFF'FFFFU);
	check_mov_r64(static_cast<short>(-5));
	check_mov_r64(static_cast<unsigned short>(0xFFFF));
	check_mov_r64(static_cast<char>(0x7F));
	{
		CodeAssembler a(64);
		a.mov(rax, -1);
		a.mov(rcx, 0xFFFF'FFFFU);
		a.mov(rdx, 5);
		REQUIRE(!a.has_error());
		const auto& instrs = a.instructions();
		REQUIRE_EQ(instrs.size(), static_cast<std::size_t>(3));
		for (const Instruction& instr : instrs)
			CHECK(instr.code() == Code::Mov_r64_imm64);
		CHECK_EQ(instrs[0].immediate64(), UINT64_C(0xFFFF'FFFF'FFFF'FFFF));
		CHECK_EQ(instrs[1].immediate64(), UINT64_C(0xFFFF'FFFF));
		CHECK_EQ(instrs[2].immediate64(), UINT64_C(5));
	}
}

TEST_CASE("code_asm/int_args_imm32") {
	// Types <= 32 bits (after conversion) can be used with instructions that only have 32-bit overloads
	check_add_r32(static_cast<short>(-5));
	check_add_r32(static_cast<unsigned short>(5));
	check_add_r32(static_cast<signed char>(-5));
	check_add_r32(static_cast<unsigned char>(5));
	check_add_r32(-5);
	check_add_r32(5U);
	check_add_r32(static_cast<std::int32_t>(-5));
	check_add_r32(static_cast<std::uint32_t>(5));
	// 32-bit on some platforms (a 64-bit value is ambiguous if there are only 32-bit overloads, same as `std::int64_t`)
	check_add_r32_if_32bit(static_cast<long>(-5));
	check_add_r32_if_32bit(static_cast<unsigned long>(5));
	check_add_r32_if_32bit(static_cast<std::size_t>(5));

	// Short values: same as int
	check_same_instrs(
		64, [](CodeAssembler& a) { a.add(eax, static_cast<short>(-1)); }, [](CodeAssembler& a) { a.add(eax, -1); });
}

TEST_CASE("code_asm/int_args_label_u64") {
	check_same_instrs(
		64, [](CodeAssembler& a) { a.jmp(0x1234'5678ULL).call(0x1234'5678LL).jne(static_cast<std::size_t>(0x1234'5678)); },
		[](CodeAssembler& a) { a.jmp(UINT64_C(0x1234'5678)).call(UINT64_C(0x1234'5678)).jne(UINT64_C(0x1234'5678)); });
	check_same_instrs(
		64, [](CodeAssembler& a) { a.jmp(0x1234'5678).call(0x1234'5678U); },
		[](CodeAssembler& a) { a.jmp(UINT64_C(0x1234'5678)).call(UINT64_C(0x1234'5678)); });
}

TEST_CASE("code_asm/int_args_memory") {
	CHECK_EQ((rax + 0x1234'5678'9ABC'DEF0ULL).displacement(), INT64_C(0x1234'5678'9ABC'DEF0));
	CHECK_EQ((rax - 1LL).displacement(), INT64_C(-1));
	CHECK_EQ((rax + rcx * 4ULL - 8L).displacement(), INT64_C(-8));
	CHECK_EQ((rax + rcx * 4ULL - 8L).scale(), 4U);
	CHECK_EQ((static_cast<std::size_t>(0x10) + rax).displacement(), INT64_C(0x10));
	CHECK_EQ(ptr(0xFFFF'FFFF'FFFF'FFFFULL).displacement(), INT64_C(-1));
	CHECK_EQ(qword_ptr(static_cast<unsigned long>(0x1234)).displacement(), INT64_C(0x1234));
	CHECK_EQ((dword_ptr(rax) + static_cast<short>(-2)).displacement(), INT64_C(-2));
	check_same_instrs(
		64, [](CodeAssembler& a) { a.mov(rcx, qword_ptr(rax + 0x10ULL)).mov(qword_ptr(rax - 1LL), 0x7F); },
		[](CodeAssembler& a) { a.mov(rcx, qword_ptr(rax + 0x10)).mov(qword_ptr(rax - 1), 0x7F); });
}

TEST_CASE("code_asm/data_c_arrays") {
	static const std::uint8_t b[] = {0x90, 0xCC};
	static const std::int8_t bi[] = {-1, 2};
	static const std::uint16_t w[] = {0x1234, 0x5678};
	static const std::int16_t wi[] = {-1, 2};
	static const std::uint32_t d[] = {0x1234'5678, 0x9ABC'DEF0};
	static const std::int32_t di[] = {-1, 2};
	static const float df[] = {1.0f, -2.5f};
	static const std::uint64_t q[] = {0x1234'5678'9ABC'DEF0, 1};
	static const std::int64_t qi[] = {-1, 2};
	static const double qf[] = {1.0, -2.5};
	check_same_instrs(
		64,
		[](CodeAssembler& a) { a.db(b).db_i(bi).dw(w).dw_i(wi).dd(d).dd_i(di).dd_f32(df).dq(q).dq_i(qi).dq_f64(qf); },
		[](CodeAssembler& a) {
			a.db(b, 2).db_i(bi, 2).dw(w, 2).dw_i(wi, 2).dd(d, 2).dd_i(di, 2).dd_f32(df, 2).dq(q, 2).dq_i(qi, 2).dq_f64(qf, 2);
		});
	// Braced lists still use the std::initializer_list overloads
	check_same_instrs(
		64, [](CodeAssembler& a) { a.db({0x90, 0xCC}).dq({0x1234'5678'9ABC'DEF0ULL, 1}); },
		[](CodeAssembler& a) { a.db(std::vector<std::uint8_t>{0x90, 0xCC}).dq(q, 2); });
}

} // namespace iced_x86::tests::code_asm_tests
