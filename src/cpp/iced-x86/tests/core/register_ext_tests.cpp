// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Tests generated from the Rust doc examples (register.rs) + port of src/rust/iced-x86/src/test/reg.rs

#include "test_framework.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/register_ext.hpp"

#include <cstdint>
#include <unordered_set>

using namespace iced_x86;

TEST_CASE("core/register_ext/doc_register") {
	{
		const auto& info = register_ext::info(Register::EAX);
		CHECK(info.register_() == Register::EAX);
	}
}

TEST_CASE("core/register_ext/doc_base") {
	{
		const auto& info = register_ext::info(Register::GS);
		CHECK(info.base() == Register::ES);
	}
	{
		const auto& info = register_ext::info(Register::RDX);
		CHECK(info.base() == Register::RAX);
	}
	{
		const auto& info = register_ext::info(Register::XMM13);
		CHECK(info.base() == Register::XMM0);
	}
	{
		const auto& info = register_ext::info(Register::YMM13);
		CHECK(info.base() == Register::YMM0);
	}
	{
		const auto& info = register_ext::info(Register::ZMM13);
		CHECK(info.base() == Register::ZMM0);
	}
}

TEST_CASE("core/register_ext/doc_number") {
	{
		const auto& info = register_ext::info(Register::GS);
		CHECK(info.number() == 5);
	}
	{
		const auto& info = register_ext::info(Register::RDX);
		CHECK(info.number() == 2);
	}
	{
		const auto& info = register_ext::info(Register::XMM13);
		CHECK(info.number() == 13);
	}
	{
		const auto& info = register_ext::info(Register::YMM13);
		CHECK(info.number() == 13);
	}
	{
		const auto& info = register_ext::info(Register::ZMM13);
		CHECK(info.number() == 13);
	}
}

TEST_CASE("core/register_ext/doc_full_register") {
	{
		const auto& info = register_ext::info(Register::GS);
		CHECK(info.full_register() == Register::GS);
	}
	{
		const auto& info = register_ext::info(Register::BH);
		CHECK(info.full_register() == Register::RBX);
	}
	{
		const auto& info = register_ext::info(Register::DX);
		CHECK(info.full_register() == Register::RDX);
	}
	{
		const auto& info = register_ext::info(Register::ESP);
		CHECK(info.full_register() == Register::RSP);
	}
	{
		const auto& info = register_ext::info(Register::RCX);
		CHECK(info.full_register() == Register::RCX);
	}
	{
		const auto& info = register_ext::info(Register::XMM3);
		CHECK(info.full_register() == Register::ZMM3);
	}
	{
		const auto& info = register_ext::info(Register::YMM3);
		CHECK(info.full_register() == Register::ZMM3);
	}
	{
		const auto& info = register_ext::info(Register::ZMM3);
		CHECK(info.full_register() == Register::ZMM3);
	}
}

TEST_CASE("core/register_ext/doc_full_register32") {
	{
		const auto& info = register_ext::info(Register::GS);
		CHECK(info.full_register32() == Register::GS);
	}
	{
		const auto& info = register_ext::info(Register::BH);
		CHECK(info.full_register32() == Register::EBX);
	}
	{
		const auto& info = register_ext::info(Register::DX);
		CHECK(info.full_register32() == Register::EDX);
	}
	{
		const auto& info = register_ext::info(Register::ESP);
		CHECK(info.full_register32() == Register::ESP);
	}
	{
		const auto& info = register_ext::info(Register::RCX);
		CHECK(info.full_register32() == Register::ECX);
	}
	{
		const auto& info = register_ext::info(Register::XMM3);
		CHECK(info.full_register32() == Register::ZMM3);
	}
	{
		const auto& info = register_ext::info(Register::YMM3);
		CHECK(info.full_register32() == Register::ZMM3);
	}
	{
		const auto& info = register_ext::info(Register::ZMM3);
		CHECK(info.full_register32() == Register::ZMM3);
	}
}

TEST_CASE("core/register_ext/doc_size") {
	{
		const auto& info = register_ext::info(Register::GS);
		CHECK(info.size() == 2);
	}
	{
		const auto& info = register_ext::info(Register::BH);
		CHECK(info.size() == 1);
	}
	{
		const auto& info = register_ext::info(Register::DX);
		CHECK(info.size() == 2);
	}
	{
		const auto& info = register_ext::info(Register::ESP);
		CHECK(info.size() == 4);
	}
	{
		const auto& info = register_ext::info(Register::RCX);
		CHECK(info.size() == 8);
	}
	{
		const auto& info = register_ext::info(Register::XMM3);
		CHECK(info.size() == 16);
	}
	{
		const auto& info = register_ext::info(Register::YMM3);
		CHECK(info.size() == 32);
	}
	{
		const auto& info = register_ext::info(Register::ZMM3);
		CHECK(info.size() == 64);
	}
}

TEST_CASE("core/register_ext/doc_info") {
	{
		const auto& info = register_ext::info(Register::EAX);
		CHECK(info.size() == 4);
	}
}

TEST_CASE("core/register_ext/doc_base_2") {
	CHECK(register_ext::base(Register::GS) == Register::ES);
	CHECK(register_ext::base(Register::SIL) == Register::AL);
	CHECK(register_ext::base(Register::SP) == Register::AX);
	CHECK(register_ext::base(Register::R13D) == Register::EAX);
	CHECK(register_ext::base(Register::RBP) == Register::RAX);
	CHECK(register_ext::base(Register::MM6) == Register::MM0);
	CHECK(register_ext::base(Register::XMM28) == Register::XMM0);
	CHECK(register_ext::base(Register::YMM12) == Register::YMM0);
	CHECK(register_ext::base(Register::ZMM31) == Register::ZMM0);
	CHECK(register_ext::base(Register::K3) == Register::K0);
	CHECK(register_ext::base(Register::BND1) == Register::BND0);
	CHECK(register_ext::base(Register::ST7) == Register::ST0);
	CHECK(register_ext::base(Register::CR8) == Register::CR0);
	CHECK(register_ext::base(Register::DR6) == Register::DR0);
	CHECK(register_ext::base(Register::TR3) == Register::TR0);
	CHECK(register_ext::base(Register::RIP) == Register::EIP);
}

TEST_CASE("core/register_ext/doc_number_2") {
	CHECK(register_ext::number(Register::GS) == 5);
	CHECK(register_ext::number(Register::SIL) == 10);
	CHECK(register_ext::number(Register::SP) == 4);
	CHECK(register_ext::number(Register::R13D) == 13);
	CHECK(register_ext::number(Register::RBP) == 5);
	CHECK(register_ext::number(Register::MM6) == 6);
	CHECK(register_ext::number(Register::XMM28) == 28);
	CHECK(register_ext::number(Register::YMM12) == 12);
	CHECK(register_ext::number(Register::ZMM31) == 31);
	CHECK(register_ext::number(Register::K3) == 3);
	CHECK(register_ext::number(Register::BND1) == 1);
	CHECK(register_ext::number(Register::ST7) == 7);
	CHECK(register_ext::number(Register::CR8) == 8);
	CHECK(register_ext::number(Register::DR6) == 6);
	CHECK(register_ext::number(Register::TR3) == 3);
	CHECK(register_ext::number(Register::RIP) == 1);
}

TEST_CASE("core/register_ext/doc_full_register_2") {
	CHECK(register_ext::full_register(Register::GS) == Register::GS);
	CHECK(register_ext::full_register(Register::SIL) == Register::RSI);
	CHECK(register_ext::full_register(Register::SP) == Register::RSP);
	CHECK(register_ext::full_register(Register::R13D) == Register::R13);
	CHECK(register_ext::full_register(Register::RBP) == Register::RBP);
	CHECK(register_ext::full_register(Register::MM6) == Register::MM6);
	CHECK(register_ext::full_register(Register::XMM10) == Register::ZMM10);
	CHECK(register_ext::full_register(Register::YMM10) == Register::ZMM10);
	CHECK(register_ext::full_register(Register::ZMM10) == Register::ZMM10);
	CHECK(register_ext::full_register(Register::K3) == Register::K3);
	CHECK(register_ext::full_register(Register::BND1) == Register::BND1);
	CHECK(register_ext::full_register(Register::ST7) == Register::ST7);
	CHECK(register_ext::full_register(Register::CR8) == Register::CR8);
	CHECK(register_ext::full_register(Register::DR6) == Register::DR6);
	CHECK(register_ext::full_register(Register::TR3) == Register::TR3);
	CHECK(register_ext::full_register(Register::RIP) == Register::RIP);
}

TEST_CASE("core/register_ext/doc_full_register32_2") {
	CHECK(register_ext::full_register32(Register::GS) == Register::GS);
	CHECK(register_ext::full_register32(Register::SIL) == Register::ESI);
	CHECK(register_ext::full_register32(Register::SP) == Register::ESP);
	CHECK(register_ext::full_register32(Register::R13D) == Register::R13D);
	CHECK(register_ext::full_register32(Register::RBP) == Register::EBP);
	CHECK(register_ext::full_register32(Register::MM6) == Register::MM6);
	CHECK(register_ext::full_register32(Register::XMM10) == Register::ZMM10);
	CHECK(register_ext::full_register32(Register::YMM10) == Register::ZMM10);
	CHECK(register_ext::full_register32(Register::ZMM10) == Register::ZMM10);
	CHECK(register_ext::full_register32(Register::K3) == Register::K3);
	CHECK(register_ext::full_register32(Register::BND1) == Register::BND1);
	CHECK(register_ext::full_register32(Register::ST7) == Register::ST7);
	CHECK(register_ext::full_register32(Register::CR8) == Register::CR8);
	CHECK(register_ext::full_register32(Register::DR6) == Register::DR6);
	CHECK(register_ext::full_register32(Register::TR3) == Register::TR3);
	CHECK(register_ext::full_register32(Register::RIP) == Register::RIP);
}

TEST_CASE("core/register_ext/doc_size_2") {
	CHECK(register_ext::size(Register::GS) == 2);
	CHECK(register_ext::size(Register::SIL) == 1);
	CHECK(register_ext::size(Register::SP) == 2);
	CHECK(register_ext::size(Register::R13D) == 4);
	CHECK(register_ext::size(Register::RBP) == 8);
	CHECK(register_ext::size(Register::MM6) == 8);
	CHECK(register_ext::size(Register::XMM10) == 16);
	CHECK(register_ext::size(Register::YMM10) == 32);
	CHECK(register_ext::size(Register::ZMM10) == 64);
	CHECK(register_ext::size(Register::K3) == 8);
	CHECK(register_ext::size(Register::BND1) == 16);
	CHECK(register_ext::size(Register::ST7) == 10);
	CHECK(register_ext::size(Register::CR8) == 8);
	CHECK(register_ext::size(Register::DR6) == 8);
	CHECK(register_ext::size(Register::TR3) == 4);
	CHECK(register_ext::size(Register::RIP) == 8);
}

TEST_CASE("core/register_ext/doc_is_segment_register") {
	CHECK(register_ext::is_segment_register(Register::GS));
	CHECK(!register_ext::is_segment_register(Register::RCX));
}

TEST_CASE("core/register_ext/doc_is_gpr") {
	CHECK(!register_ext::is_gpr(Register::GS));
	CHECK(register_ext::is_gpr(Register::CH));
	CHECK(register_ext::is_gpr(Register::DX));
	CHECK(register_ext::is_gpr(Register::R13D));
	CHECK(register_ext::is_gpr(Register::RSP));
	CHECK(!register_ext::is_gpr(Register::XMM0));
}

TEST_CASE("core/register_ext/doc_is_gpr8") {
	CHECK(!register_ext::is_gpr8(Register::GS));
	CHECK(register_ext::is_gpr8(Register::CH));
	CHECK(!register_ext::is_gpr8(Register::DX));
	CHECK(!register_ext::is_gpr8(Register::R13D));
	CHECK(!register_ext::is_gpr8(Register::RSP));
	CHECK(!register_ext::is_gpr8(Register::XMM0));
}

TEST_CASE("core/register_ext/doc_is_gpr16") {
	CHECK(!register_ext::is_gpr16(Register::GS));
	CHECK(!register_ext::is_gpr16(Register::CH));
	CHECK(register_ext::is_gpr16(Register::DX));
	CHECK(!register_ext::is_gpr16(Register::R13D));
	CHECK(!register_ext::is_gpr16(Register::RSP));
	CHECK(!register_ext::is_gpr16(Register::XMM0));
}

TEST_CASE("core/register_ext/doc_is_gpr32") {
	CHECK(!register_ext::is_gpr32(Register::GS));
	CHECK(!register_ext::is_gpr32(Register::CH));
	CHECK(!register_ext::is_gpr32(Register::DX));
	CHECK(register_ext::is_gpr32(Register::R13D));
	CHECK(!register_ext::is_gpr32(Register::RSP));
	CHECK(!register_ext::is_gpr32(Register::XMM0));
}

TEST_CASE("core/register_ext/doc_is_gpr64") {
	CHECK(!register_ext::is_gpr64(Register::GS));
	CHECK(!register_ext::is_gpr64(Register::CH));
	CHECK(!register_ext::is_gpr64(Register::DX));
	CHECK(!register_ext::is_gpr64(Register::R13D));
	CHECK(register_ext::is_gpr64(Register::RSP));
	CHECK(!register_ext::is_gpr64(Register::XMM0));
}

TEST_CASE("core/register_ext/doc_is_xmm") {
	CHECK(!register_ext::is_xmm(Register::R13D));
	CHECK(!register_ext::is_xmm(Register::RSP));
	CHECK(register_ext::is_xmm(Register::XMM0));
	CHECK(!register_ext::is_xmm(Register::YMM0));
	CHECK(!register_ext::is_xmm(Register::ZMM0));
}

TEST_CASE("core/register_ext/doc_is_ymm") {
	CHECK(!register_ext::is_ymm(Register::R13D));
	CHECK(!register_ext::is_ymm(Register::RSP));
	CHECK(!register_ext::is_ymm(Register::XMM0));
	CHECK(register_ext::is_ymm(Register::YMM0));
	CHECK(!register_ext::is_ymm(Register::ZMM0));
}

TEST_CASE("core/register_ext/doc_is_zmm") {
	CHECK(!register_ext::is_zmm(Register::R13D));
	CHECK(!register_ext::is_zmm(Register::RSP));
	CHECK(!register_ext::is_zmm(Register::XMM0));
	CHECK(!register_ext::is_zmm(Register::YMM0));
	CHECK(register_ext::is_zmm(Register::ZMM0));
}

TEST_CASE("core/register_ext/doc_is_vector_register") {
	CHECK(!register_ext::is_vector_register(Register::R13D));
	CHECK(!register_ext::is_vector_register(Register::RSP));
	CHECK(register_ext::is_vector_register(Register::XMM0));
	CHECK(register_ext::is_vector_register(Register::YMM0));
	CHECK(register_ext::is_vector_register(Register::ZMM0));
}

TEST_CASE("core/register_ext/doc_is_ip") {
	CHECK(register_ext::is_ip(Register::EIP));
	CHECK(register_ext::is_ip(Register::RIP));
}

TEST_CASE("core/register_ext/doc_is_k") {
	CHECK(!register_ext::is_k(Register::R13D));
	CHECK(register_ext::is_k(Register::K3));
}

TEST_CASE("core/register_ext/doc_is_cr") {
	CHECK(!register_ext::is_cr(Register::R13D));
	CHECK(register_ext::is_cr(Register::CR3));
}

TEST_CASE("core/register_ext/doc_is_dr") {
	CHECK(!register_ext::is_dr(Register::R13D));
	CHECK(register_ext::is_dr(Register::DR3));
}

TEST_CASE("core/register_ext/doc_is_tr") {
	CHECK(!register_ext::is_tr(Register::R13D));
	CHECK(register_ext::is_tr(Register::TR3));
}

TEST_CASE("core/register_ext/doc_is_st") {
	CHECK(!register_ext::is_st(Register::R13D));
	CHECK(register_ext::is_st(Register::ST3));
}

TEST_CASE("core/register_ext/doc_is_bnd") {
	CHECK(!register_ext::is_bnd(Register::R13D));
	CHECK(register_ext::is_bnd(Register::BND3));
}

TEST_CASE("core/register_ext/doc_is_mm") {
	CHECK(!register_ext::is_mm(Register::R13D));
	CHECK(register_ext::is_mm(Register::MM3));
}

TEST_CASE("core/register_ext/doc_is_tmm") {
	CHECK(!register_ext::is_tmm(Register::R13D));
	CHECK(register_ext::is_tmm(Register::TMM3));
}

// Port of test/reg.rs (the tests that verify that invalid values panic aren't ported since the C++ code aborts)

TEST_CASE("core/register_ext/reg_add_ops") {
	CHECK(0 + Register::AX == Register::AX);
	CHECK(0U + Register::AX == Register::AX);
	CHECK(Register::AX + 0 == Register::AX);
	CHECK(Register::EAX + 0U == Register::EAX);

	CHECK(3 + Register::AX == Register::BX);
	CHECK(3U + Register::EAX == Register::EBX);
	CHECK(Register::AX + 3 == Register::BX);
	CHECK(Register::EAX + 3U == Register::EBX);

	Register reg = Register::AX;
	reg += 3;
	CHECK(reg == Register::BX);

	reg = Register::EAX;
	reg += 3U;
	CHECK(reg == Register::EBX);

	CHECK(Register::None + static_cast<std::int32_t>(IcedConstants::REGISTER_ENUM_COUNT - 1) == static_cast<Register>(IcedConstants::REGISTER_ENUM_COUNT - 1));
}

TEST_CASE("core/register_ext/reg_sub_ops") {
	CHECK(Register::SP - 0 == Register::SP);
	CHECK(Register::ESP - 0U == Register::ESP);

	CHECK(Register::SP - 2 == Register::DX);
	CHECK(Register::ESP - 2U == Register::EDX);

	Register reg = Register::SP;
	reg -= 2;
	CHECK(reg == Register::DX);

	reg = Register::ESP;
	reg -= 2U;
	CHECK(reg == Register::EDX);
}

TEST_CASE("core/register_ext/info_table") {
	for (std::size_t i = 0; i < IcedConstants::REGISTER_ENUM_COUNT; i++) {
		const auto reg = static_cast<Register>(i);
		const RegisterInfo& info = register_ext::info(reg);
		CHECK(info.register_() == reg);
		CHECK(register_ext::base(reg) == info.base());
		CHECK(register_ext::number(reg) == info.number());
		CHECK(register_ext::full_register(reg) == info.full_register());
		CHECK(register_ext::full_register32(reg) == info.full_register32());
		CHECK(register_ext::size(reg) == info.size());
		CHECK(info.base() <= reg);
		CHECK(info == register_ext::info(reg));
	}
	CHECK(register_ext::info(Register::EAX) != register_ext::info(Register::ECX));
	std::unordered_set<RegisterInfo> set;
	set.insert(register_ext::info(Register::EAX));
	set.insert(register_ext::info(Register::EAX));
	set.insert(register_ext::info(Register::ECX));
	CHECK(set.size() == 2);
}
