// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "code_asm/code_asm_test_utils.hpp"
#include "iced_x86/block_encoder_options.hpp"
#include "iced_x86/code_ext.hpp"
#include "iced_x86/decoder.hpp"

#include <string>
#include <vector>

namespace iced_x86::tests::code_asm_tests {

CodeAssembler create_asm(std::uint32_t bitness, std::uint32_t flags) {
	CodeAssembler a = unwrap(CodeAssembler::create(bitness));
	if ((flags & TestInstrFlags::PREFER_VEX) != 0)
		a.set_prefer_vex(true);
	else if ((flags & TestInstrFlags::PREFER_EVEX) != 0)
		a.set_prefer_vex(false);
	if ((flags & TestInstrFlags::PREFER_SHORT_BRANCH) != 0)
		a.set_prefer_short_branch(true);
	else if ((flags & TestInstrFlags::PREFER_NEAR_BRANCH) != 0)
		a.set_prefer_short_branch(false);
	return a;
}

static std::string instr_codes_to_string(const Instruction& a, const Instruction& b) {
	return std::string(to_string(a.code())) + " vs " + to_string(b.code());
}

void test_instr(std::uint32_t bitness, void (*create)(CodeAssembler& a), Instruction expected, std::uint32_t flags, std::uint32_t decoder_options) {
	CodeAssembler a = create_asm(bitness, flags);
	create(a);
	REQUIRE_MSG(!a.has_error(), a.error()->message());
	REQUIRE_EQ(a.instructions().size(), static_cast<std::size_t>(1));

	if ((flags & TestInstrFlags::BROADCAST) != 0)
		expected.set_is_broadcast(true);
	Instruction asm_instr = a.instructions()[0];
	REQUIRE_MSG(asm_instr == expected, instr_codes_to_string(asm_instr, expected));

	const std::uint64_t rip = 0;
	std::vector<std::uint8_t> bytes;
	if ((flags & TestInstrFlags::BRANCH_U64) != 0)
		bytes = unwrap(a.assemble(rip));
	else
		bytes = unwrap(a.assemble_options(rip, BlockEncoderOptions::DONT_FIX_BRANCHES)).inner.code_buffer;

	Decoder decoder = unwrap(Decoder::try_with_ip(bitness, bytes.data(), bytes.size(), rip, decoder_options));
	Instruction decoded_instr = expected.code() == Code::Zero_bytes && bytes.empty() ? Instruction::with(Code::Zero_bytes) : decoder.decode();
	if ((flags & TestInstrFlags::IGNORE_CODE) != 0)
		decoded_instr.set_code(asm_instr.code());
	if ((flags & TestInstrFlags::REMOVE_REP_REPNE_PREFIXES) != 0) {
		decoded_instr.set_has_rep_prefix(false);
		decoded_instr.set_has_repne_prefix(false);
	}
	if ((flags & TestInstrFlags::FWAIT) != 0) {
		REQUIRE(decoded_instr.code() == Code::Wait);
		decoded_instr = decoder.decode();
		Code new_code;
		switch (decoded_instr.code()) {
		case Code::Fnstenv_m14byte:
			new_code = Code::Fstenv_m14byte;
			break;
		case Code::Fnstenv_m28byte:
			new_code = Code::Fstenv_m28byte;
			break;
		case Code::Fnstcw_m2byte:
			new_code = Code::Fstcw_m2byte;
			break;
		case Code::Fneni:
			new_code = Code::Feni;
			break;
		case Code::Fndisi:
			new_code = Code::Fdisi;
			break;
		case Code::Fnclex:
			new_code = Code::Fclex;
			break;
		case Code::Fninit:
			new_code = Code::Finit;
			break;
		case Code::Fnsetpm:
			new_code = Code::Fsetpm;
			break;
		case Code::Fnsave_m94byte:
			new_code = Code::Fsave_m94byte;
			break;
		case Code::Fnsave_m108byte:
			new_code = Code::Fsave_m108byte;
			break;
		case Code::Fnstsw_m2byte:
			new_code = Code::Fstsw_m2byte;
			break;
		case Code::Fnstsw_AX:
			new_code = Code::Fstsw_AX;
			break;
		case Code::Fnstdw_AX:
			new_code = Code::Fstdw_AX;
			break;
		case Code::Fnstsg_AX:
			new_code = Code::Fstsg_AX;
			break;
		default:
			FAIL(std::string("Unexpected code: ") + to_string(decoded_instr.code()));
		}
		decoded_instr.set_code(new_code);
	}
	if (asm_instr.code() != Code::Jmpe_disp16 && asm_instr.code() != Code::Jmpe_disp32 && (flags & TestInstrFlags::BRANCH) != 0)
		asm_instr.set_near_branch64(0);

	// Short branches can be re-written if the target is too far away.
	// Eg. `loopne target` => `loopne jmpt; jmp short skip; jmpt: jmp near target; skip:`
	if ((flags & TestInstrFlags::BRANCH_U64) != 0) {
		asm_instr.set_code(code_ext::as_short_branch(asm_instr.code()));
		decoded_instr.set_code(code_ext::as_short_branch(decoded_instr.code()));
		CHECK(asm_instr.code() == decoded_instr.code());

		if (decoded_instr.near_branch64() == 4) {
			Instruction next_instr = decoder.decode();
			Code expected_code;
			switch (bitness) {
			case 16:
				expected_code = Code::Jmp_rel8_16;
				break;
			case 32:
				expected_code = Code::Jmp_rel8_32;
				break;
			case 64:
				expected_code = Code::Jmp_rel8_64;
				break;
			default:
				FAIL("Invalid bitness");
			}
			CHECK(next_instr.code() == expected_code);
		}
		else
			CHECK_MSG(decoded_instr == asm_instr, instr_codes_to_string(decoded_instr, asm_instr));
	}
	else
		CHECK_MSG(decoded_instr == asm_instr, instr_codes_to_string(decoded_instr, asm_instr));
}

void test_invalid_instr(std::uint32_t bitness, void (*create)(CodeAssembler& a), std::uint32_t flags) {
	CodeAssembler a = create_asm(bitness, flags);
	create(a);
	CHECK(a.has_error());
	CHECK(a.error() != nullptr);
	CHECK_EQ(a.instructions().size(), static_cast<std::size_t>(0));
}

CodeLabel create_and_emit_label(CodeAssembler& a) {
	CodeLabel lbl = a.create_label();
	a.set_label(lbl);
	REQUIRE(!a.has_error());
	return lbl;
}

} // namespace iced_x86::tests::code_asm_tests
