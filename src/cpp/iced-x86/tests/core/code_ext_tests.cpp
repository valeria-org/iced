// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Tests generated from the Rust doc examples (code.rs)

#include "test_framework.hpp"
#include "iced_x86/code_ext.hpp"

using namespace iced_x86;

TEST_CASE("core/code_ext/doc_mnemonic") {
	CHECK(code_ext::mnemonic(Code::Add_rm32_r32) == Mnemonic::Add);
}

TEST_CASE("core/code_ext/doc_condition_code") {
	CHECK(code_ext::condition_code(Code::Jbe_rel8_64) == ConditionCode::be);
	CHECK(code_ext::condition_code(Code::Cmovo_r64_rm64) == ConditionCode::o);
	CHECK(code_ext::condition_code(Code::Setne_rm8) == ConditionCode::ne);
	CHECK(code_ext::condition_code(Code::Pause) == ConditionCode::None);
}

TEST_CASE("core/code_ext/doc_negate_condition_code") {
	CHECK(code_ext::negate_condition_code(Code::Setbe_rm8) == Code::Seta_rm8);
	CHECK(code_ext::negate_condition_code(Code::Seta_rm8) == Code::Setbe_rm8);
}

TEST_CASE("core/code_ext/doc_as_short_branch") {
	CHECK(code_ext::as_short_branch(Code::Jbe_rel32_64) == Code::Jbe_rel8_64);
	CHECK(code_ext::as_short_branch(Code::Jbe_rel8_64) == Code::Jbe_rel8_64);
	CHECK(code_ext::as_short_branch(Code::Pause) == Code::Pause);
}

TEST_CASE("core/code_ext/doc_as_near_branch") {
	CHECK(code_ext::as_near_branch(Code::Jbe_rel8_64) == Code::Jbe_rel32_64);
	CHECK(code_ext::as_near_branch(Code::Jbe_rel32_64) == Code::Jbe_rel32_64);
	CHECK(code_ext::as_near_branch(Code::Pause) == Code::Pause);
}

TEST_CASE("core/code_ext/is_string_instruction") {
	CHECK(code_ext::is_string_instruction(Code::Movsb_m8_m8));
	CHECK(code_ext::is_string_instruction(Code::Scasq_RAX_m64));
	CHECK(code_ext::is_string_instruction(Code::Outsd_DX_m32));
	CHECK(!code_ext::is_string_instruction(Code::Add_rm8_r8));
	CHECK(!code_ext::is_string_instruction(Code::INVALID));
}

TEST_CASE("core/code_ext/branch_predicates") {
	CHECK(code_ext::is_jcc_short(Code::Jo_rel8_16));
	CHECK(code_ext::is_jcc_short(Code::Jg_rel8_64));
	CHECK(!code_ext::is_jcc_short(Code::Jo_rel16));
	CHECK(code_ext::is_jcc_near(Code::Jo_rel16));
	CHECK(code_ext::is_jcc_near(Code::Jg_rel32_64));
	CHECK(code_ext::is_jcc_short_or_near(Code::Jbe_rel8_64));
	CHECK(code_ext::is_jcc_short_or_near(Code::Jbe_rel32_64));
	CHECK(!code_ext::is_jcc_short_or_near(Code::Jmp_rel8_64));
	CHECK(code_ext::is_jmp_short(Code::Jmp_rel8_32));
	CHECK(code_ext::is_jmp_near(Code::Jmp_rel32_64));
	CHECK(code_ext::is_jmp_short_or_near(Code::Jmp_rel8_16));
	CHECK(code_ext::is_jmp_short_or_near(Code::Jmp_rel16));
	CHECK(!code_ext::is_jmp_short_or_near(Code::Jmp_rm64));
	CHECK(code_ext::is_jmp_far(Code::Jmp_ptr1632));
	CHECK(code_ext::is_call_near(Code::Call_rel32_64));
	CHECK(code_ext::is_call_far(Code::Call_ptr1616));
	CHECK(code_ext::is_jmp_near_indirect(Code::Jmp_rm32));
	CHECK(code_ext::is_jmp_far_indirect(Code::Jmp_m1664));
	CHECK(code_ext::is_call_near_indirect(Code::Call_rm64));
	CHECK(code_ext::is_call_far_indirect(Code::Call_m1632));
	CHECK(code_ext::is_jkcc_short_or_near(Code::VEX_KNC_Jkzd_kr_rel8_64));
	CHECK(code_ext::is_jkcc_short(Code::VEX_KNC_Jknzd_kr_rel8_64));
	CHECK(code_ext::is_jkcc_near(Code::VEX_KNC_Jknzd_kr_rel32_64));
	CHECK(!code_ext::is_jkcc_near(Code::VEX_KNC_Jknzd_kr_rel8_64));
	CHECK(code_ext::is_jcx_short(Code::Jrcxz_rel8_64));
	CHECK(code_ext::is_loopcc(Code::Loope_rel8_64_RCX));
	CHECK(code_ext::is_loopcc(Code::Loopne_rel8_16_CX));
	CHECK(!code_ext::is_loopcc(Code::Loop_rel8_16_CX));
	CHECK(code_ext::is_loop(Code::Loop_rel8_64_RCX));
}

TEST_CASE("core/code_ext/condition_code_and_negate") {
	CHECK(code_ext::condition_code(Code::Loopne_rel8_32_ECX) == ConditionCode::ne);
	CHECK(code_ext::condition_code(Code::Loope_rel8_32_ECX) == ConditionCode::e);
	CHECK(code_ext::condition_code(Code::VEX_Cmpoxadd_m32_r32_r32) == ConditionCode::o);
	CHECK(code_ext::condition_code(Code::VEX_Cmpnlexadd_m64_r64_r64) == ConditionCode::g);
	CHECK(code_ext::condition_code(Code::VEX_KNC_Jkzd_kr_rel8_64) == ConditionCode::e);
	CHECK(code_ext::condition_code(Code::VEX_KNC_Jknzd_kr_rel32_64) == ConditionCode::ne);
	CHECK(code_ext::negate_condition_code(Code::Loopne_rel8_32_ECX) == Code::Loope_rel8_32_ECX);
	CHECK(code_ext::negate_condition_code(Code::Loope_rel8_64_RCX) == Code::Loopne_rel8_64_RCX);
	CHECK(code_ext::negate_condition_code(Code::Je_rel32_64) == Code::Jne_rel32_64);
	CHECK(code_ext::negate_condition_code(Code::Jne_rel8_16) == Code::Je_rel8_16);
	CHECK(code_ext::negate_condition_code(Code::Cmovae_r32_rm32) == Code::Cmovb_r32_rm32);
	CHECK(code_ext::negate_condition_code(Code::VEX_Cmpoxadd_m64_r64_r64) == Code::VEX_Cmpnoxadd_m64_r64_r64);
	CHECK(code_ext::negate_condition_code(Code::VEX_KNC_Jkzd_kr_rel8_64) == Code::VEX_KNC_Jknzd_kr_rel8_64);
	CHECK(code_ext::negate_condition_code(Code::Pause) == Code::Pause);
	CHECK(code_ext::as_short_branch(Code::Jmp_rel32_32) == Code::Jmp_rel8_32);
	CHECK(code_ext::as_near_branch(Code::Jmp_rel8_16) == Code::Jmp_rel16);
	CHECK(code_ext::as_short_branch(Code::VEX_KNC_Jknzd_kr_rel32_64) == Code::VEX_KNC_Jknzd_kr_rel8_64);
	CHECK(code_ext::as_near_branch(Code::VEX_KNC_Jkzd_kr_rel8_64) == Code::VEX_KNC_Jkzd_kr_rel32_64);
	// Every condition code instruction's negated instruction must have the negated condition code
	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const auto code = static_cast<Code>(i);
		const ConditionCode cc = code_ext::condition_code(code);
		const Code negated = code_ext::negate_condition_code(code);
		if (cc == ConditionCode::None) {
			CHECK(negated == code);
			continue;
		}
		CHECK(negated != code);
		CHECK(code_ext::negate_condition_code(negated) == code);
		CHECK(code_ext::mnemonic(negated) != code_ext::mnemonic(code));
		const auto cc_value = static_cast<std::uint32_t>(cc);
		const auto neg_cc_value = static_cast<std::uint32_t>(code_ext::condition_code(negated));
		// o/no, b/ae, e/ne, be/a, s/ns, p/np, l/ge, le/g are adjacent values (o=1)
		CHECK(((cc_value - 1) ^ 1) == neg_cc_value - 1);
	}
}
