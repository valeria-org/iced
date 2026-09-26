// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_framework.hpp"
#include "iced_x86/code.hpp"
#include "iced_x86/code_ext.hpp"
#include "iced_x86/code_size.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/encoding_kind.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/instruction_info.hpp"
#include "iced_x86/op_kind.hpp"
#include "iced_x86/register_ext.hpp"
#include "iced_x86/rflags_bits.hpp"
#include "info/info_test_cases.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

using namespace iced_x86;
using namespace iced_x86::tests;
using namespace iced_x86::tests::instr_info;

namespace {

template <typename T>
std::string vec_to_string(const std::vector<T>& v) {
	std::string s = "{";
	for (std::size_t i = 0; i < v.size(); i++) {
		if (i != 0)
			s += ", ";
		s += to_string(v[i]);
	}
	s += "}";
	return s;
}

template <typename T>
std::unordered_set<T> to_set(const std::vector<T>& v) {
	return std::unordered_set<T>(v.begin(), v.end());
}

template <typename TFeatures>
bool same_cpuid_features(const TFeatures& features, const std::vector<CpuidFeature>& expected) {
	return std::equal(features.begin(), features.end(), expected.begin(), expected.end());
}

// Registers belonging to a group (RAX, EAX, AX, AL, ...) are ordered from biggest to smallest
int get_register_group_order(Register reg) {
	if (Register::RAX <= reg && reg <= Register::R15)
		return 0;
	if (Register::EAX <= reg && reg <= Register::R15D)
		return 1;
	if (Register::AX <= reg && reg <= Register::R15W)
		return 2;
	if (Register::AL <= reg && reg <= Register::R15L)
		return 3;
	if (Register::ZMM0 <= reg && reg <= IcedConstants::ZMM_LAST)
		return 4;
	if (Register::YMM0 <= reg && reg <= IcedConstants::YMM_LAST)
		return 5;
	if (Register::XMM0 <= reg && reg <= IcedConstants::XMM_LAST)
		return 6;
	return -1;
}

Register reg_add(Register reg, std::uint32_t value) { return static_cast<Register>(static_cast<std::uint32_t>(reg) + value); }

constexpr std::array<std::tuple<Register, Register, Register>, 4> LOW_REGS = {{
	{Register::AL, Register::AH, Register::AX},
	{Register::CL, Register::CH, Register::CX},
	{Register::DL, Register::DH, Register::DX},
	{Register::BL, Register::BH, Register::BX},
}};

std::vector<Register> get_registers(std::vector<Register> regs) {
	if (regs.size() <= 1)
		return regs;

	std::sort(regs.begin(), regs.end(), [](Register x, Register y) {
		const int ox = get_register_group_order(x);
		const int oy = get_register_group_order(y);
		if (ox != oy)
			return ox < oy;
		return static_cast<std::int32_t>(x) < static_cast<std::int32_t>(y);
	});

	std::unordered_set<Register> hash;
	std::uint32_t index;
	for (Register reg : regs) {
		if (Register::EAX <= reg && reg <= Register::R15D) {
			index = static_cast<std::uint32_t>(reg) - static_cast<std::uint32_t>(Register::EAX);
			if (hash.count(reg_add(Register::RAX, index)) != 0)
				continue;
		} else if (Register::AX <= reg && reg <= Register::R15W) {
			index = static_cast<std::uint32_t>(reg) - static_cast<std::uint32_t>(Register::AX);
			if (hash.count(reg_add(Register::RAX, index)) != 0)
				continue;
			if (hash.count(reg_add(Register::EAX, index)) != 0)
				continue;
		} else if (Register::AL <= reg && reg <= Register::R15L) {
			index = static_cast<std::uint32_t>(reg) - static_cast<std::uint32_t>(Register::AL);
			if (Register::AH <= reg && reg <= Register::BH)
				index -= 4;
			if (hash.count(reg_add(Register::RAX, index)) != 0)
				continue;
			if (hash.count(reg_add(Register::EAX, index)) != 0)
				continue;
			if (hash.count(reg_add(Register::AX, index)) != 0)
				continue;
		} else if (Register::YMM0 <= reg && reg <= IcedConstants::YMM_LAST) {
			index = static_cast<std::uint32_t>(reg) - static_cast<std::uint32_t>(Register::YMM0);
			if (hash.count(reg_add(Register::ZMM0, index)) != 0)
				continue;
		} else if (Register::XMM0 <= reg && reg <= IcedConstants::XMM_LAST) {
			index = static_cast<std::uint32_t>(reg) - static_cast<std::uint32_t>(Register::XMM0);
			if (hash.count(reg_add(Register::ZMM0, index)) != 0)
				continue;
			if (hash.count(reg_add(Register::YMM0, index)) != 0)
				continue;
		}
		hash.insert(reg);
	}

	for (const auto& info : LOW_REGS) {
		if (hash.count(std::get<0>(info)) != 0 && hash.count(std::get<1>(info)) != 0) {
			hash.erase(std::get<0>(info));
			hash.erase(std::get<1>(info));
			hash.insert(std::get<2>(info));
		}
	}

	return std::vector<Register>(hash.begin(), hash.end());
}

std::vector<UsedRegister> get_used_registers(const std::vector<UsedRegister>& used_registers) {
	std::vector<Register> read;
	std::vector<Register> write;
	std::vector<Register> cond_read;
	std::vector<Register> cond_write;

	for (const UsedRegister& info : used_registers) {
		switch (info.access()) {
		case OpAccess::Read:
			read.push_back(info.register_());
			break;
		case OpAccess::CondRead:
			cond_read.push_back(info.register_());
			break;
		case OpAccess::Write:
			write.push_back(info.register_());
			break;
		case OpAccess::CondWrite:
			cond_write.push_back(info.register_());
			break;
		case OpAccess::ReadWrite:
			read.push_back(info.register_());
			write.push_back(info.register_());
			break;
		case OpAccess::ReadCondWrite:
			read.push_back(info.register_());
			cond_write.push_back(info.register_());
			break;
		case OpAccess::None:
		case OpAccess::NoMemAccess:
		default:
			FAIL("Invalid register access: " + to_string(info));
		}
	}

	std::unordered_set<UsedRegister> h;
	for (Register reg : get_registers(read))
		h.insert(UsedRegister(reg, OpAccess::Read));
	for (Register reg : get_registers(write))
		h.insert(UsedRegister(reg, OpAccess::Write));
	for (Register reg : get_registers(cond_read))
		h.insert(UsedRegister(reg, OpAccess::CondRead));
	for (Register reg : get_registers(cond_write))
		h.insert(UsedRegister(reg, OpAccess::CondWrite));
	std::vector<UsedRegister> vec(h.begin(), h.end());
	std::sort(vec.begin(), vec.end(), [](const UsedRegister& x, const UsedRegister& y) {
		if (x.register_() != y.register_())
			return x.register_() < y.register_();
		return x.access() < y.access();
	});
	return vec;
}

void check_equal(const InstructionInfo& info1, const InstructionInfo& info2, bool has_regs2, bool has_mem2) {
	if (has_regs2)
		CHECK(info2.used_registers() == info1.used_registers());
	else
		CHECK(info2.used_registers().empty());
	if (has_mem2)
		CHECK(info2.used_memory() == info1.used_memory());
	else
		CHECK(info2.used_memory().empty());
	CHECK_EQ(info2.op0_access(), info1.op0_access());
	CHECK_EQ(info2.op1_access(), info1.op1_access());
	CHECK_EQ(info2.op2_access(), info1.op2_access());
	CHECK_EQ(info2.op3_access(), info1.op3_access());
	CHECK_EQ(info2.op4_access(), info1.op4_access());
}

Instruction create_instruction(const InstrInfoTestCase& tc, const std::vector<std::uint8_t>& code_bytes) {
	Instruction instr;
	if (tc.is_special) {
		if (tc.bitness == 16 && tc.code == Code::Popw_CS && tc.hex_bytes == "0F") {
			instr = Instruction();
			instr.set_code(Code::Popw_CS);
			instr.set_op0_kind(OpKind::Register);
			instr.set_op0_register(Register::CS);
			instr.set_code_size(CodeSize::Code16);
			instr.set_len(1);
		} else if (tc.code <= Code::DeclareQword) {
			instr = Instruction();
			instr.set_code(tc.code);
			instr.set_declare_data_len(1);
			REQUIRE_EQ(tc.bitness, 64U);
			instr.set_code_size(CodeSize::Code64);
			switch (tc.code) {
			case Code::DeclareByte:
				REQUIRE_EQ(tc.hex_bytes, "66");
				instr.set_declare_byte_value(0, 0x66);
				break;
			case Code::DeclareWord:
				REQUIRE_EQ(tc.hex_bytes, "6644");
				instr.set_declare_word_value(0, 0x4466);
				break;
			case Code::DeclareDword:
				REQUIRE_EQ(tc.hex_bytes, "664422EE");
				instr.set_declare_dword_value(0, 0xEE22'4466);
				break;
			case Code::DeclareQword:
				REQUIRE_EQ(tc.hex_bytes, "664422EE12345678");
				instr.set_declare_qword_value(0, 0x7856'3412'EE22'4466ULL);
				break;
			default:
				FAIL("unreachable");
			}
		} else if (tc.code == Code::Zero_bytes) {
			instr = Instruction();
			instr.set_code(tc.code);
			REQUIRE_EQ(tc.bitness, 64U);
			instr.set_code_size(CodeSize::Code64);
			REQUIRE_EQ(tc.hex_bytes, "");
		} else {
			Decoder decoder = Decoder::with_ip(tc.bitness, code_bytes, tc.ip, tc.decoder_options);
			instr = decoder.decode();
			if (code_bytes.size() > 1 && code_bytes[0] == 0x9B && instr.len() == 1) {
				instr = decoder.decode();
				switch (instr.code()) {
				case Code::Fnstenv_m14byte:
					instr.set_code(Code::Fstenv_m14byte);
					break;
				case Code::Fnstenv_m28byte:
					instr.set_code(Code::Fstenv_m28byte);
					break;
				case Code::Fnstcw_m2byte:
					instr.set_code(Code::Fstcw_m2byte);
					break;
				case Code::Fneni:
					instr.set_code(Code::Feni);
					break;
				case Code::Fndisi:
					instr.set_code(Code::Fdisi);
					break;
				case Code::Fnclex:
					instr.set_code(Code::Fclex);
					break;
				case Code::Fninit:
					instr.set_code(Code::Finit);
					break;
				case Code::Fnsetpm:
					instr.set_code(Code::Fsetpm);
					break;
				case Code::Fnsave_m94byte:
					instr.set_code(Code::Fsave_m94byte);
					break;
				case Code::Fnsave_m108byte:
					instr.set_code(Code::Fsave_m108byte);
					break;
				case Code::Fnstsw_m2byte:
					instr.set_code(Code::Fstsw_m2byte);
					break;
				case Code::Fnstsw_AX:
					instr.set_code(Code::Fstsw_AX);
					break;
				case Code::Fnstdw_AX:
					instr.set_code(Code::Fstdw_AX);
					break;
				case Code::Fnstsg_AX:
					instr.set_code(Code::Fstsg_AX);
					break;
				default:
					FAIL("unreachable");
				}
			} else
				FAIL("unreachable");
		}
	} else {
		Decoder decoder = Decoder::with_ip(tc.bitness, code_bytes, tc.ip, tc.decoder_options);
		instr = decoder.decode();
	}
	return instr;
}

void test_info_core(const InstrInfoTestCase& tc, InstructionInfoFactory& factory) {
	const std::vector<std::uint8_t> code_bytes = to_vec_u8(tc.hex_bytes);
	const Instruction instr = create_instruction(tc, code_bytes);
	REQUIRE_EQ(instr.code(), tc.code);

	CHECK_EQ(instr.stack_pointer_increment(), tc.stack_pointer_increment);

	InstructionInfoFactory factory1;
	const InstructionInfo& info = factory1.info(instr);
	CHECK_EQ(info.op0_access(), tc.op0_access);
	CHECK_EQ(info.op1_access(), tc.op1_access);
	CHECK_EQ(info.op2_access(), tc.op2_access);
	CHECK_EQ(info.op3_access(), tc.op3_access);
	CHECK_EQ(info.op4_access(), tc.op4_access);
	const FpuStackIncrementInfo fpu_info = instr.fpu_stack_increment_info();
	CHECK_EQ(fpu_info.increment(), tc.fpu_top_increment);
	CHECK_EQ(fpu_info.conditional(), tc.fpu_conditional_top);
	CHECK_EQ(fpu_info.writes_top(), tc.fpu_writes_top);
	CHECK_MSG(to_set(info.used_memory()) == to_set(tc.used_memory),
		"line " + std::to_string(tc.line_number) + ": " + vec_to_string(info.used_memory()) + " != " + vec_to_string(tc.used_memory));
	const std::vector<UsedRegister> actual_regs = get_used_registers(info.used_registers());
	const std::vector<UsedRegister> expected_regs = get_used_registers(tc.used_registers);
	CHECK_MSG(actual_regs == expected_regs, "line " + std::to_string(tc.line_number) + ": " + vec_to_string(actual_regs) + " != " + vec_to_string(expected_regs));

	static_assert(IcedConstants::MAX_OP_COUNT == 5, "");
	REQUIRE(instr.op_count() <= IcedConstants::MAX_OP_COUNT);
	const OpAccess expected_accesses[IcedConstants::MAX_OP_COUNT] = {tc.op0_access, tc.op1_access, tc.op2_access, tc.op3_access, tc.op4_access};
	for (std::uint32_t i = 0; i < instr.op_count(); i++) {
		CHECK_EQ(expected_accesses[i], info.op_access(i));
		const Result<OpAccess> access = info.try_op_access(i);
		REQUIRE(access.is_ok());
		CHECK_EQ(expected_accesses[i], access.value());
	}
	for (std::uint32_t i = instr.op_count(); i < IcedConstants::MAX_OP_COUNT; i++) {
		CHECK_EQ(info.op_access(i), OpAccess::None);
		const Result<OpAccess> access = info.try_op_access(i);
		REQUIRE(access.is_ok());
		CHECK_EQ(access.value(), OpAccess::None);
	}
#ifdef NDEBUG
	// Debug builds abort (Rust: debug_assert!() panics)
	CHECK_EQ(info.op_access(static_cast<std::uint32_t>(IcedConstants::MAX_OP_COUNT)), OpAccess::None);
#endif
	CHECK(info.try_op_access(static_cast<std::uint32_t>(IcedConstants::MAX_OP_COUNT)).is_err());

	{
		InstructionInfoFactory factory2;
		check_equal(info, factory2.info_options(instr, InstructionInfoOptions::NONE), true, true);
	}
	{
		InstructionInfoFactory factory2;
		check_equal(info, factory2.info_options(instr, InstructionInfoOptions::NO_MEMORY_USAGE), true, false);
	}
	{
		InstructionInfoFactory factory2;
		check_equal(info, factory2.info_options(instr, InstructionInfoOptions::NO_REGISTER_USAGE), false, true);
	}
	{
		InstructionInfoFactory factory2;
		check_equal(info, factory2.info_options(instr, InstructionInfoOptions::NO_REGISTER_USAGE | InstructionInfoOptions::NO_MEMORY_USAGE), false,
			false);
	}

	check_equal(info, factory.info(instr), true, true);
	check_equal(info, factory.info_options(instr, InstructionInfoOptions::NONE), true, true);
	check_equal(info, factory.info_options(instr, InstructionInfoOptions::NO_MEMORY_USAGE), true, false);
	check_equal(info, factory.info_options(instr, InstructionInfoOptions::NO_REGISTER_USAGE), false, true);
	check_equal(info, factory.info_options(instr, InstructionInfoOptions::NO_REGISTER_USAGE | InstructionInfoOptions::NO_MEMORY_USAGE), false, false);

	CHECK_EQ(code_ext::encoding(instr.code()), tc.encoding);
	CHECK(same_cpuid_features(code_ext::cpuid_features(instr.code()), tc.cpuid_features));
	CHECK_EQ(code_ext::flow_control(instr.code()), tc.flow_control);
	CHECK_EQ(code_ext::is_privileged(instr.code()), tc.is_privileged);
	CHECK_EQ(code_ext::is_stack_instruction(instr.code()), tc.is_stack_instruction);
	CHECK_EQ(code_ext::is_save_restore_instruction(instr.code()), tc.is_save_restore_instruction);

	CHECK_EQ(instr.encoding(), tc.encoding);
	CHECK_EQ(instr.encoding() == EncodingKind::MVEX, IcedConstants::is_mvex(instr.code()));
	CHECK(same_cpuid_features(instr.cpuid_features(), tc.cpuid_features));
	CHECK_EQ(instr.flow_control(), tc.flow_control);
	CHECK_EQ(instr.is_privileged(), tc.is_privileged);
	CHECK_EQ(instr.is_stack_instruction(), tc.is_stack_instruction);
	CHECK_EQ(instr.is_save_restore_instruction(), tc.is_save_restore_instruction);
	CHECK_EQ(instr.rflags_read(), tc.rflags_read);
	CHECK_EQ(instr.rflags_written(), tc.rflags_written);
	CHECK_EQ(instr.rflags_cleared(), tc.rflags_cleared);
	CHECK_EQ(instr.rflags_set(), tc.rflags_set);
	CHECK_EQ(instr.rflags_undefined(), tc.rflags_undefined);
	CHECK_EQ(instr.rflags_modified(), tc.rflags_written | tc.rflags_cleared | tc.rflags_set | tc.rflags_undefined);

	CHECK_EQ(instr.rflags_written() & (instr.rflags_cleared() | instr.rflags_set() | instr.rflags_undefined()), RflagsBits::NONE);
	CHECK_EQ(instr.rflags_cleared() & (instr.rflags_written() | instr.rflags_set() | instr.rflags_undefined()), RflagsBits::NONE);
	CHECK_EQ(instr.rflags_set() & (instr.rflags_written() | instr.rflags_cleared() | instr.rflags_undefined()), RflagsBits::NONE);
	CHECK_EQ(instr.rflags_undefined() & (instr.rflags_written() | instr.rflags_cleared() | instr.rflags_set()), RflagsBits::NONE);
}

void test_info(std::uint32_t bitness) {
	InstructionInfoFactory factory;
	for (const InstrInfoTestCase& tc : get_instr_info_test_cases(bitness)) {
		try {
			test_info_core(tc, factory);
		}
		catch (const RequireFailedException&) {
			report_failure(__FILE__, __LINE__, "Test case failed: " + std::to_string(bitness) + "-bit, line " + std::to_string(tc.line_number));
		}
	}
}

} // namespace

TEST_CASE("info/info_16") { test_info(16); }

TEST_CASE("info/info_32") { test_info(32); }

TEST_CASE("info/info_64") { test_info(64); }

TEST_CASE("info/make_sure_all_code_values_are_tested") {
	std::vector<bool> tested(IcedConstants::CODE_ENUM_COUNT, false);
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		for (const InstrInfoTestCase& tc : get_instr_info_test_cases(bitness))
			tested[static_cast<std::size_t>(tc.code)] = true;
	}

	std::string s;
	std::size_t missing = 0;
	for (std::size_t i = 0; i < tested.size(); i++) {
		const char* name = to_string(static_cast<Code>(i));
		if (!tested[i] && !is_ignored_code(name)) {
			s += name;
			s += ' ';
			missing++;
		}
	}
	CHECK_EQ(std::to_string(missing) + " ins " + s, std::string("0 ins "));
}

TEST_CASE("info/verify_used_memory_size") {
	static_assert(sizeof(UsedMemory) == 16, "");
}
