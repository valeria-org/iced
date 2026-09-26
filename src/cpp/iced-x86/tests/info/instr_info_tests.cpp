// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_framework.hpp"
#include "generated/memory_size_flags.hpp"
#include "generated/register_flags.hpp"
#include "iced_x86/code.hpp"
#include "iced_x86/code_ext.hpp"
#include "iced_x86/code_size.hpp"
#include "iced_x86/condition_code.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/encoding_kind.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/instruction_info.hpp"
#include "iced_x86/memory_size_ext.hpp"
#include "iced_x86/op_kind.hpp"
#include "iced_x86/register_ext.hpp"
#include "iced_x86/rflags_bits.hpp"
#include "info/info_test_cases.hpp"
#include "info/misc_test_data.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>
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

TEST_CASE("info/memory_size_info") {
	const std::vector<MemorySizeInfoTestCase> test_cases = read_memory_size_info_test_cases(get_instr_info_unit_tests_dir() + "/MemorySizeInfo.txt");
	std::unordered_set<MemorySize> h;
	for (const auto& tc : test_cases)
		h.insert(tc.memory_size);
	// Make sure every value is tested
	CHECK_EQ(h.size(), IcedConstants::MEMORY_SIZE_ENUM_COUNT);
	// Make sure there are no dupes
	CHECK_EQ(test_cases.size(), IcedConstants::MEMORY_SIZE_ENUM_COUNT);
	for (const auto& tc : test_cases) {
		const MemorySizeInfo& info = memory_size_ext::info(tc.memory_size);
		CHECK_EQ(info.memory_size(), tc.memory_size);
		CHECK_EQ(info.size(), tc.size);
		CHECK_EQ(info.element_size(), tc.element_size);
		CHECK_EQ(info.element_type(), tc.element_type);
		CHECK_EQ(info.is_signed(), (tc.flags & MemorySizeFlags::SIGNED) != 0);
		CHECK_EQ(info.is_broadcast(), (tc.flags & MemorySizeFlags::BROADCAST) != 0);
		CHECK_EQ(info.is_packed(), (tc.flags & MemorySizeFlags::PACKED) != 0);
		CHECK_EQ(info.element_count(), tc.element_count);

		CHECK_EQ(memory_size_ext::size(tc.memory_size), tc.size);
		CHECK_EQ(memory_size_ext::element_size(tc.memory_size), tc.element_size);
		CHECK_EQ(memory_size_ext::element_type(tc.memory_size), tc.element_type);
		CHECK_EQ(memory_size_ext::element_type_info(tc.memory_size).memory_size(), tc.element_type);
		CHECK_EQ(memory_size_ext::is_signed(tc.memory_size), (tc.flags & MemorySizeFlags::SIGNED) != 0);
		CHECK_EQ(memory_size_ext::is_packed(tc.memory_size), (tc.flags & MemorySizeFlags::PACKED) != 0);
		CHECK_EQ(memory_size_ext::is_broadcast(tc.memory_size), (tc.flags & MemorySizeFlags::BROADCAST) != 0);
		CHECK_EQ(memory_size_ext::element_count(tc.memory_size), tc.element_count);
	}
}

TEST_CASE("info/register_info") {
	const std::vector<RegisterInfoTestCase> test_cases = read_register_info_test_cases(get_instr_info_unit_tests_dir() + "/RegisterInfo.txt");
	std::unordered_set<Register> h;
	for (const auto& tc : test_cases)
		h.insert(tc.register_);
	// Make sure every value is tested
	CHECK_EQ(h.size(), IcedConstants::REGISTER_ENUM_COUNT);
	// Make sure there are no dupes
	CHECK_EQ(test_cases.size(), IcedConstants::REGISTER_ENUM_COUNT);
	for (const auto& tc : test_cases) {
		const RegisterInfo& info = register_ext::info(tc.register_);
		CHECK_EQ(info.register_(), tc.register_);
		CHECK_EQ(info.base(), tc.base);
		CHECK_EQ(info.number(), tc.number);
		CHECK_EQ(info.full_register(), tc.full_register);
		CHECK_EQ(info.full_register32(), tc.full_register32);
		CHECK_EQ(info.size(), tc.size);

		CHECK_EQ(register_ext::base(tc.register_), tc.base);
		CHECK_EQ(register_ext::number(tc.register_), tc.number);
		CHECK_EQ(register_ext::full_register(tc.register_), tc.full_register);
		CHECK_EQ(register_ext::full_register32(tc.register_), tc.full_register32);
		CHECK_EQ(register_ext::size(tc.register_), tc.size);

		constexpr std::uint32_t ALL_FLAGS = RegisterFlags::SEGMENT_REGISTER | RegisterFlags::GPR | RegisterFlags::GPR8 | RegisterFlags::GPR16 |
											RegisterFlags::GPR32 | RegisterFlags::GPR64 | RegisterFlags::XMM | RegisterFlags::YMM | RegisterFlags::ZMM |
											RegisterFlags::VECTOR_REGISTER | RegisterFlags::IP | RegisterFlags::K | RegisterFlags::BND |
											RegisterFlags::CR | RegisterFlags::DR | RegisterFlags::TR | RegisterFlags::ST | RegisterFlags::MM |
											RegisterFlags::TMM;
		// If it fails, update the flags above and the code below, eg. add a is_tmm() test
		CHECK_EQ(tc.flags & ALL_FLAGS, tc.flags);

		CHECK_EQ(register_ext::is_segment_register(tc.register_), (tc.flags & RegisterFlags::SEGMENT_REGISTER) != 0);
		CHECK_EQ(register_ext::is_gpr(tc.register_), (tc.flags & RegisterFlags::GPR) != 0);
		CHECK_EQ(register_ext::is_gpr8(tc.register_), (tc.flags & RegisterFlags::GPR8) != 0);
		CHECK_EQ(register_ext::is_gpr16(tc.register_), (tc.flags & RegisterFlags::GPR16) != 0);
		CHECK_EQ(register_ext::is_gpr32(tc.register_), (tc.flags & RegisterFlags::GPR32) != 0);
		CHECK_EQ(register_ext::is_gpr64(tc.register_), (tc.flags & RegisterFlags::GPR64) != 0);
		CHECK_EQ(register_ext::is_xmm(tc.register_), (tc.flags & RegisterFlags::XMM) != 0);
		CHECK_EQ(register_ext::is_ymm(tc.register_), (tc.flags & RegisterFlags::YMM) != 0);
		CHECK_EQ(register_ext::is_zmm(tc.register_), (tc.flags & RegisterFlags::ZMM) != 0);
		CHECK_EQ(register_ext::is_vector_register(tc.register_), (tc.flags & RegisterFlags::VECTOR_REGISTER) != 0);
		CHECK_EQ(register_ext::is_ip(tc.register_), (tc.flags & RegisterFlags::IP) != 0);
		CHECK_EQ(register_ext::is_k(tc.register_), (tc.flags & RegisterFlags::K) != 0);
		CHECK_EQ(register_ext::is_bnd(tc.register_), (tc.flags & RegisterFlags::BND) != 0);
		CHECK_EQ(register_ext::is_cr(tc.register_), (tc.flags & RegisterFlags::CR) != 0);
		CHECK_EQ(register_ext::is_dr(tc.register_), (tc.flags & RegisterFlags::DR) != 0);
		CHECK_EQ(register_ext::is_tr(tc.register_), (tc.flags & RegisterFlags::TR) != 0);
		CHECK_EQ(register_ext::is_st(tc.register_), (tc.flags & RegisterFlags::ST) != 0);
		CHECK_EQ(register_ext::is_mm(tc.register_), (tc.flags & RegisterFlags::MM) != 0);
		CHECK_EQ(register_ext::is_tmm(tc.register_), (tc.flags & RegisterFlags::TMM) != 0);
	}
}

TEST_CASE("info/is_branch_call") {
	const MiscTestsData& data = get_misc_tests_data();
	const auto& jcc_short = data.jcc_short;
	const auto& jcx_short = data.jrcxz;
	const auto& jmp_near = data.jmp_near;
	const auto& jmp_far = data.jmp_far;
	const auto& jmp_short = data.jmp_short;
	const auto& jmp_near_indirect = data.jmp_near_indirect;
	const auto& jmp_far_indirect = data.jmp_far_indirect;
	const auto& jcc_near = data.jcc_near;
	const auto& call_far = data.call_far;
	const auto& call_near = data.call_near;
	const auto& call_near_indirect = data.call_near_indirect;
	const auto& call_far_indirect = data.call_far_indirect;
	const auto& jkcc_short = data.jkcc_short;
	const auto& jkcc_near = data.jkcc_near;
	const auto& loop_ = data.loop_;

	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const Code code = static_cast<Code>(i);
		Instruction instr;
		instr.set_code(code);

		CHECK_EQ(code_ext::is_jcc_short_or_near(code), jcc_short.count(code) != 0 || jcc_near.count(code) != 0);
		CHECK_EQ(instr.is_jcc_short_or_near(), code_ext::is_jcc_short_or_near(code));

		CHECK_EQ(code_ext::is_jcc_near(code), jcc_near.count(code) != 0);
		CHECK_EQ(instr.is_jcc_near(), code_ext::is_jcc_near(code));

		CHECK_EQ(code_ext::is_jcc_short(code), jcc_short.count(code) != 0);
		CHECK_EQ(instr.is_jcc_short(), code_ext::is_jcc_short(code));

		CHECK_EQ(code_ext::is_jcx_short(code), jcx_short.count(code) != 0);
		CHECK_EQ(instr.is_jcx_short(), code_ext::is_jcx_short(code));

		CHECK_EQ(code_ext::is_jmp_short(code), jmp_short.count(code) != 0);
		CHECK_EQ(instr.is_jmp_short(), code_ext::is_jmp_short(code));

		CHECK_EQ(code_ext::is_jmp_near(code), jmp_near.count(code) != 0);
		CHECK_EQ(instr.is_jmp_near(), code_ext::is_jmp_near(code));

		CHECK_EQ(code_ext::is_jmp_short_or_near(code), jmp_short.count(code) != 0 || jmp_near.count(code) != 0);
		CHECK_EQ(instr.is_jmp_short_or_near(), code_ext::is_jmp_short_or_near(code));

		CHECK_EQ(code_ext::is_jmp_far(code), jmp_far.count(code) != 0);
		CHECK_EQ(instr.is_jmp_far(), code_ext::is_jmp_far(code));

		CHECK_EQ(code_ext::is_call_near(code), call_near.count(code) != 0);
		CHECK_EQ(instr.is_call_near(), code_ext::is_call_near(code));

		CHECK_EQ(code_ext::is_call_far(code), call_far.count(code) != 0);
		CHECK_EQ(instr.is_call_far(), code_ext::is_call_far(code));

		CHECK_EQ(code_ext::is_jmp_near_indirect(code), jmp_near_indirect.count(code) != 0);
		CHECK_EQ(instr.is_jmp_near_indirect(), code_ext::is_jmp_near_indirect(code));

		CHECK_EQ(code_ext::is_jmp_far_indirect(code), jmp_far_indirect.count(code) != 0);
		CHECK_EQ(instr.is_jmp_far_indirect(), code_ext::is_jmp_far_indirect(code));

		CHECK_EQ(code_ext::is_call_near_indirect(code), call_near_indirect.count(code) != 0);
		CHECK_EQ(instr.is_call_near_indirect(), code_ext::is_call_near_indirect(code));

		CHECK_EQ(code_ext::is_call_far_indirect(code), call_far_indirect.count(code) != 0);
		CHECK_EQ(instr.is_call_far_indirect(), code_ext::is_call_far_indirect(code));

		CHECK_EQ(code_ext::is_jkcc_short_or_near(code), jkcc_short.count(code) != 0 || jkcc_near.count(code) != 0);
		CHECK_EQ(instr.is_jkcc_short_or_near(), code_ext::is_jkcc_short_or_near(code));

		CHECK_EQ(code_ext::is_jkcc_near(code), jkcc_near.count(code) != 0);
		CHECK_EQ(instr.is_jkcc_near(), code_ext::is_jkcc_near(code));

		CHECK_EQ(code_ext::is_jkcc_short(code), jkcc_short.count(code) != 0);
		CHECK_EQ(instr.is_jkcc_short(), code_ext::is_jkcc_short(code));

		CHECK_EQ(loop_.count(code) != 0, code_ext::is_loop(code) || code_ext::is_loopcc(code));
		CHECK_EQ(code_ext::is_loop(code), instr.is_loop());
		CHECK_EQ(code_ext::is_loopcc(code), instr.is_loopcc());
	}
}

TEST_CASE("info/verify_negate_condition_code") {
	const MiscTestsData& data = get_misc_tests_data();

	std::unordered_map<Code, Code> to_negated_code_value;
	for (const auto& a : data.jcc_short_infos)
		to_negated_code_value[std::get<0>(a)] = std::get<1>(a);
	for (const auto& a : data.jcc_near_infos)
		to_negated_code_value[std::get<0>(a)] = std::get<1>(a);
	for (const auto& a : data.setcc_infos)
		to_negated_code_value[std::get<0>(a)] = std::get<1>(a);
	for (const auto& a : data.cmovcc_infos)
		to_negated_code_value[std::get<0>(a)] = std::get<1>(a);
	for (const auto& a : data.cmpccxadd_infos)
		to_negated_code_value[std::get<0>(a)] = std::get<1>(a);
	for (const auto& a : data.loopcc_infos)
		to_negated_code_value[std::get<0>(a)] = std::get<1>(a);
	for (const auto& a : data.jkcc_short_infos)
		to_negated_code_value[std::get<0>(a)] = std::get<1>(a);
	for (const auto& a : data.jkcc_near_infos)
		to_negated_code_value[std::get<0>(a)] = std::get<1>(a);

	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const Code code = static_cast<Code>(i);
		Instruction instr;
		instr.set_code(code);

		auto it = to_negated_code_value.find(code);
		const Code negated = it == to_negated_code_value.end() ? code : it->second;

		CHECK_EQ(code_ext::negate_condition_code(code), negated);
		instr.negate_condition_code();
		CHECK_EQ(instr.code(), negated);
	}
}

TEST_CASE("info/verify_to_short_branch") {
	const MiscTestsData& data = get_misc_tests_data();

	std::unordered_map<Code, Code> as_short_branch;
	for (const auto& a : data.jcc_near_infos)
		as_short_branch[std::get<0>(a)] = std::get<2>(a);
	for (const auto& a : data.jmp_infos)
		as_short_branch[a.second] = a.first;
	for (const auto& a : data.jkcc_near_infos)
		as_short_branch[std::get<0>(a)] = std::get<2>(a);

	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const Code code = static_cast<Code>(i);
		Instruction instr;
		instr.set_code(code);

		auto it = as_short_branch.find(code);
		const Code short_code = it == as_short_branch.end() ? code : it->second;

		CHECK_EQ(code_ext::as_short_branch(code), short_code);
		instr.as_short_branch();
		CHECK_EQ(instr.code(), short_code);
	}
}

TEST_CASE("info/verify_to_near_branch") {
	const MiscTestsData& data = get_misc_tests_data();

	std::unordered_map<Code, Code> as_near_branch;
	for (const auto& a : data.jcc_short_infos)
		as_near_branch[std::get<0>(a)] = std::get<2>(a);
	for (const auto& a : data.jmp_infos)
		as_near_branch[a.first] = a.second;
	for (const auto& a : data.jkcc_short_infos)
		as_near_branch[std::get<0>(a)] = std::get<2>(a);

	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const Code code = static_cast<Code>(i);
		Instruction instr;
		instr.set_code(code);

		auto it = as_near_branch.find(code);
		const Code near_code = it == as_near_branch.end() ? code : it->second;

		CHECK_EQ(code_ext::as_near_branch(code), near_code);
		instr.as_near_branch();
		CHECK_EQ(instr.code(), near_code);
	}
}

TEST_CASE("info/verify_condition_code") {
	const MiscTestsData& data = get_misc_tests_data();

	std::unordered_map<Code, ConditionCode> to_condition_code;
	for (const auto& a : data.jcc_short_infos)
		to_condition_code[std::get<0>(a)] = std::get<3>(a);
	for (const auto& a : data.jcc_near_infos)
		to_condition_code[std::get<0>(a)] = std::get<3>(a);
	for (const auto& a : data.setcc_infos)
		to_condition_code[std::get<0>(a)] = std::get<2>(a);
	for (const auto& a : data.cmovcc_infos)
		to_condition_code[std::get<0>(a)] = std::get<2>(a);
	for (const auto& a : data.cmpccxadd_infos)
		to_condition_code[std::get<0>(a)] = std::get<2>(a);
	for (const auto& a : data.loopcc_infos)
		to_condition_code[std::get<0>(a)] = std::get<2>(a);
	for (const auto& a : data.jkcc_short_infos)
		to_condition_code[std::get<0>(a)] = std::get<3>(a);
	for (const auto& a : data.jkcc_near_infos)
		to_condition_code[std::get<0>(a)] = std::get<3>(a);

	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const Code code = static_cast<Code>(i);
		Instruction instr;
		instr.set_code(code);

		auto it = to_condition_code.find(code);
		const ConditionCode cc = it == to_condition_code.end() ? ConditionCode::None : it->second;

		CHECK_EQ(code_ext::condition_code(code), cc);
		CHECK_EQ(instr.condition_code(), cc);
	}
}

TEST_CASE("info/verify_string_instr") {
	const MiscTestsData& data = get_misc_tests_data();
	const auto& string = data.string;

	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		const Code code = static_cast<Code>(i);
		Instruction instr;
		instr.set_code(code);

		CHECK_EQ(code_ext::is_string_instruction(code), string.count(code) != 0);
		CHECK_EQ(instr.is_string_instruction(), code_ext::is_string_instruction(code));
	}
}

TEST_CASE("info/verify_condition_code_values_are_in_correct_order") {
	static_assert(static_cast<std::uint32_t>(ConditionCode::None) == 0, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::o) == 1, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::no) == 2, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::b) == 3, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::ae) == 4, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::e) == 5, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::ne) == 6, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::be) == 7, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::a) == 8, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::s) == 9, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::ns) == 10, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::p) == 11, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::np) == 12, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::l) == 13, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::ge) == 14, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::le) == 15, "");
	static_assert(static_cast<std::uint32_t>(ConditionCode::g) == 16, "");
}

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
