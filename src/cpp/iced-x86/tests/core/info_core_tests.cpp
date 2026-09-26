// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of the tests in src/rust/iced-x86/src/info/tests/mod.rs that only test core functions:
// memory_size_info, register_info, is_branch_call, verify_negate_condition_code, verify_to_short_branch,
// verify_to_near_branch, verify_condition_code, verify_string_instr, verify_condition_code_values_are_in_correct_order.
// Also ports info/tests/{mem_size_test_case,mem_size_test_parser,reg_info_test_case,reg_test_parser,misc_test_data}.rs

#include "test_framework.hpp"
#include "test_utils.hpp"
#include "generated/memory_size_flags.hpp"
#include "generated/misc_instr_info_test_constants.hpp"
#include "generated/misc_section_names.hpp"
#include "generated/register_flags.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/section_file_reader.hpp"
#include "iced_x86/code_ext.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/memory_size_ext.hpp"
#include "iced_x86/register_ext.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace iced_x86;
using namespace iced_x86::tests;

namespace {

template <typename T>
std::vector<T> all_values(std::size_t count) {
	std::vector<T> values;
	values.reserve(count);
	for (std::size_t i = 0; i < count; i++)
		values.push_back(static_cast<T>(i));
	return values;
}

std::vector<Code> all_codes() { return all_values<Code>(IcedConstants::CODE_ENUM_COUNT); }

// Reads a file, skips empty lines and comments and calls `handler` for each line.
// Errors are reported as "Error parsing <kind> test case file '<filename>', line <n>: <msg>"
template <typename F>
void read_test_file(const std::string& filename, const char* kind, F handler) {
	const auto lines = read_lines(filename);
	std::uint32_t line_number = 0;
	for (const auto& line : lines) {
		line_number++;
		if (line.empty() || line[0] == '#')
			continue;
		try {
			handler(std::string_view(line), line_number);
		}
		catch (const std::exception& ex) {
			throw std::runtime_error(std::string("Error parsing ") + kind + " test case file '" + filename + "', line " + std::to_string(line_number) +
				": " + ex.what());
		}
	}
}

std::uint32_t parse_flags(std::string_view flags_str, const std::unordered_map<std::string_view, std::uint32_t>& to_flags) {
	std::uint32_t flags = 0;
	for (auto value : split_whitespace(flags_str)) {
		if (value.empty())
			continue;
		const auto it = to_flags.find(value);
		if (it == to_flags.end())
			throw std::runtime_error("Invalid flags value: " + std::string(value));
		flags |= it->second;
	}
	return flags;
}

// ---- mem_size_test_case.rs / mem_size_test_parser.rs ----

struct MemorySizeInfoTestCase {
	std::uint32_t line_number = 0;
	MemorySize memory_size = MemorySize::Unknown;
	std::size_t size = 0;
	std::size_t element_size = 0;
	MemorySize element_type = MemorySize::Unknown;
	std::size_t element_count = 0;
	std::uint32_t flags = 0; // MemorySizeFlags
};

std::vector<MemorySizeInfoTestCase> read_memory_size_info_test_cases(const std::string& filename) {
	const auto to_flags = create_dict(MEMORY_SIZE_FLAGS_DICT);
	std::vector<MemorySizeInfoTestCase> result;
	read_test_file(filename, "memory size info", [&](std::string_view line, std::uint32_t line_number) {
		static_assert(MiscInstrInfoTestConstants::MEMORY_SIZE_ELEMS_PER_LINE == 6, "");
		const auto elems = splitn(line, MiscInstrInfoTestConstants::MEMORY_SIZE_ELEMS_PER_LINE, ',');
		if (elems.size() != MiscInstrInfoTestConstants::MEMORY_SIZE_ELEMS_PER_LINE)
			throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));
		MemorySizeInfoTestCase tc;
		tc.line_number = line_number;
		tc.memory_size = to_memory_size(elems[0]);
		tc.size = to_u32(elems[1]);
		tc.element_size = to_u32(elems[2]);
		tc.element_type = to_memory_size(elems[3]);
		tc.element_count = to_u32(elems[4]);
		tc.flags = parse_flags(elems[5], to_flags);
		result.push_back(tc);
	});
	return result;
}

// ---- reg_info_test_case.rs / reg_test_parser.rs ----

struct RegisterInfoTestCase {
	std::uint32_t line_number = 0;
	Register register_ = Register::None;
	std::size_t number = 0;
	Register base = Register::None;
	Register full_register = Register::None;
	Register full_register32 = Register::None;
	std::size_t size = 0;
	std::uint32_t flags = 0; // RegisterFlags
};

std::vector<RegisterInfoTestCase> read_register_info_test_cases(const std::string& filename) {
	const auto to_flags = create_dict(REGISTER_FLAGS_DICT);
	std::vector<RegisterInfoTestCase> result;
	read_test_file(filename, "register info", [&](std::string_view line, std::uint32_t line_number) {
		static_assert(MiscInstrInfoTestConstants::REGISTER_ELEMS_PER_LINE == 7, "");
		const auto elems = splitn(line, MiscInstrInfoTestConstants::REGISTER_ELEMS_PER_LINE, ',');
		if (elems.size() != MiscInstrInfoTestConstants::REGISTER_ELEMS_PER_LINE)
			throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));
		RegisterInfoTestCase tc;
		tc.line_number = line_number;
		tc.register_ = to_register(elems[0]);
		tc.number = to_u32(elems[1]);
		tc.base = to_register(elems[2]);
		tc.full_register = to_register(elems[3]);
		tc.full_register32 = to_register(elems[4]);
		tc.size = to_u32(elems[5]);
		tc.flags = parse_flags(elems[6], to_flags);
		result.push_back(tc);
	});
	return result;
}

// ---- misc_test_data.rs ----

struct MiscSectionNameIds {
	static constexpr std::uint32_t JCC_SHORT = 0;
	static constexpr std::uint32_t JCC_NEAR = 1;
	static constexpr std::uint32_t JMP_SHORT = 2;
	static constexpr std::uint32_t JMP_NEAR = 3;
	static constexpr std::uint32_t JMP_FAR = 4;
	static constexpr std::uint32_t JMP_NEAR_INDIRECT = 5;
	static constexpr std::uint32_t JMP_FAR_INDIRECT = 6;
	static constexpr std::uint32_t CALL_NEAR = 7;
	static constexpr std::uint32_t CALL_FAR = 8;
	static constexpr std::uint32_t CALL_NEAR_INDIRECT = 9;
	static constexpr std::uint32_t CALL_FAR_INDIRECT = 10;
	static constexpr std::uint32_t JMPE_NEAR = 11;
	static constexpr std::uint32_t JMPE_NEAR_INDIRECT = 12;
	static constexpr std::uint32_t LOOP = 13;
	static constexpr std::uint32_t JRCXZ = 14;
	static constexpr std::uint32_t XBEGIN = 15;
	static constexpr std::uint32_t JMP_INFO = 16;
	static constexpr std::uint32_t JCC_SHORT_INFO = 17;
	static constexpr std::uint32_t JCC_NEAR_INFO = 18;
	static constexpr std::uint32_t SETCC_INFO = 19;
	static constexpr std::uint32_t CMOVCC_INFO = 20;
	static constexpr std::uint32_t CMPCCXADD_INFO = 21;
	static constexpr std::uint32_t LOOPCC_INFO = 22;
	static constexpr std::uint32_t STRING_INSTRUCTION = 23;
	static constexpr std::uint32_t JKCC_SHORT = 24;
	static constexpr std::uint32_t JKCC_NEAR = 25;
	static constexpr std::uint32_t JKCC_SHORT_INFO = 26;
	static constexpr std::uint32_t JKCC_NEAR_INFO = 27;
};

using CodeSet = std::unordered_set<Code>;
using JccInfo = std::tuple<Code, Code, Code, ConditionCode>;
using InstrCcInfo = std::tuple<Code, Code, ConditionCode>;

struct MiscTestsData {
	CodeSet jcc_short;
	CodeSet jmp_near;
	CodeSet jmp_far;
	CodeSet jmp_short;
	CodeSet jmp_near_indirect;
	CodeSet jmp_far_indirect;
	CodeSet jcc_near;
	CodeSet call_far;
	CodeSet call_near;
	CodeSet call_near_indirect;
	CodeSet call_far_indirect;
	CodeSet jmpe_near;
	CodeSet jmpe_near_indirect;
	CodeSet loop_;
	CodeSet jrcxz;
	CodeSet xbegin;
	CodeSet string;
	CodeSet jkcc_short;
	CodeSet jkcc_near;
	std::vector<std::pair<Code, Code>> jmp_infos;
	std::vector<JccInfo> jcc_short_infos;
	std::vector<JccInfo> jcc_near_infos;
	std::vector<JccInfo> jkcc_short_infos;
	std::vector<JccInfo> jkcc_near_infos;
	std::vector<InstrCcInfo> setcc_infos;
	std::vector<InstrCcInfo> cmovcc_infos;
	std::vector<InstrCcInfo> cmpccxadd_infos;
	std::vector<InstrCcInfo> loopcc_infos;
};

void add_code(CodeSet& h, std::string_view line) {
	if (!is_ignored_code(line))
		h.insert(to_code(line));
}

std::vector<std::string_view> split_elems(std::string_view line, std::size_t expected) {
	auto elems = split(line, ',');
	if (elems.size() != expected)
		throw std::runtime_error("Expected " + std::to_string(expected) + " elements, found " + std::to_string(elems.size()));
	return elems;
}

void add_jmp_info(std::vector<std::pair<Code, Code>>& v, std::string_view line) {
	const auto elems = split_elems(line, 2);
	if (!is_ignored_code(elems[0]) && !is_ignored_code(elems[1]))
		v.emplace_back(to_code(elems[0]), to_code(elems[1]));
}

void add_jcc_info(std::vector<JccInfo>& v, std::string_view line) {
	const auto elems = split_elems(line, 4);
	if (!is_ignored_code(elems[0]) && !is_ignored_code(elems[1]) && !is_ignored_code(elems[2]))
		v.emplace_back(to_code(elems[0]), to_code(elems[1]), to_code(elems[2]), to_condition_code(elems[3]));
}

void add_instr_cc_info(std::vector<InstrCcInfo>& v, std::string_view line) {
	const auto elems = split_elems(line, 3);
	if (!is_ignored_code(elems[0]) && !is_ignored_code(elems[1]))
		v.emplace_back(to_code(elems[0]), to_code(elems[1]), to_condition_code(elems[2]));
}

MiscTestsData read_misc_tests_data() {
	MiscTestsData data;
	using N = MiscSectionNames;
	using I = MiscSectionNameIds;
	SectionFileReader reader({
		{N::JCC_SHORT, I::JCC_SHORT},
		{N::JMP_NEAR, I::JMP_NEAR},
		{N::JMP_FAR, I::JMP_FAR},
		{N::JMP_SHORT, I::JMP_SHORT},
		{N::JMP_NEAR_INDIRECT, I::JMP_NEAR_INDIRECT},
		{N::JMP_FAR_INDIRECT, I::JMP_FAR_INDIRECT},
		{N::JCC_NEAR, I::JCC_NEAR},
		{N::CALL_FAR, I::CALL_FAR},
		{N::CALL_NEAR, I::CALL_NEAR},
		{N::CALL_NEAR_INDIRECT, I::CALL_NEAR_INDIRECT},
		{N::CALL_FAR_INDIRECT, I::CALL_FAR_INDIRECT},
		{N::JMPE_NEAR, I::JMPE_NEAR},
		{N::JMPE_NEAR_INDIRECT, I::JMPE_NEAR_INDIRECT},
		{N::LOOP, I::LOOP},
		{N::JRCXZ, I::JRCXZ},
		{N::XBEGIN, I::XBEGIN},
		{N::JMP_INFO, I::JMP_INFO},
		{N::JCC_SHORT_INFO, I::JCC_SHORT_INFO},
		{N::JCC_NEAR_INFO, I::JCC_NEAR_INFO},
		{N::SETCC_INFO, I::SETCC_INFO},
		{N::CMOVCC_INFO, I::CMOVCC_INFO},
		{N::CMPCCXADD_INFO, I::CMPCCXADD_INFO},
		{N::LOOPCC_INFO, I::LOOPCC_INFO},
		{N::STRING_INSTRUCTION, I::STRING_INSTRUCTION},
		{N::JKCC_SHORT, I::JKCC_SHORT},
		{N::JKCC_NEAR, I::JKCC_NEAR},
		{N::JKCC_SHORT_INFO, I::JKCC_SHORT_INFO},
		{N::JKCC_NEAR_INFO, I::JKCC_NEAR_INFO},
	});
	reader.read(get_instr_info_unit_tests_dir() + "/Misc.txt", [&data](std::uint32_t id, std::string_view line) {
		switch (id) {
		case I::JCC_SHORT: add_code(data.jcc_short, line); break;
		case I::JMP_NEAR: add_code(data.jmp_near, line); break;
		case I::JMP_FAR: add_code(data.jmp_far, line); break;
		case I::JMP_SHORT: add_code(data.jmp_short, line); break;
		case I::JMP_NEAR_INDIRECT: add_code(data.jmp_near_indirect, line); break;
		case I::JMP_FAR_INDIRECT: add_code(data.jmp_far_indirect, line); break;
		case I::JCC_NEAR: add_code(data.jcc_near, line); break;
		case I::CALL_FAR: add_code(data.call_far, line); break;
		case I::CALL_NEAR: add_code(data.call_near, line); break;
		case I::CALL_NEAR_INDIRECT: add_code(data.call_near_indirect, line); break;
		case I::CALL_FAR_INDIRECT: add_code(data.call_far_indirect, line); break;
		case I::JMPE_NEAR: add_code(data.jmpe_near, line); break;
		case I::JMPE_NEAR_INDIRECT: add_code(data.jmpe_near_indirect, line); break;
		case I::LOOP: add_code(data.loop_, line); break;
		case I::JRCXZ: add_code(data.jrcxz, line); break;
		case I::XBEGIN: add_code(data.xbegin, line); break;
		case I::JMP_INFO: add_jmp_info(data.jmp_infos, line); break;
		case I::JCC_SHORT_INFO: add_jcc_info(data.jcc_short_infos, line); break;
		case I::JCC_NEAR_INFO: add_jcc_info(data.jcc_near_infos, line); break;
		case I::SETCC_INFO: add_instr_cc_info(data.setcc_infos, line); break;
		case I::CMOVCC_INFO: add_instr_cc_info(data.cmovcc_infos, line); break;
		case I::CMPCCXADD_INFO: add_instr_cc_info(data.cmpccxadd_infos, line); break;
		case I::LOOPCC_INFO: add_instr_cc_info(data.loopcc_infos, line); break;
		case I::STRING_INSTRUCTION: add_code(data.string, line); break;
		case I::JKCC_SHORT: add_code(data.jkcc_short, line); break;
		case I::JKCC_NEAR: add_code(data.jkcc_near, line); break;
		case I::JKCC_SHORT_INFO: add_jcc_info(data.jkcc_short_infos, line); break;
		case I::JKCC_NEAR_INFO: add_jcc_info(data.jkcc_near_infos, line); break;
		default: throw std::runtime_error("Unknown section id");
		}
	});
	return data;
}

const MiscTestsData& get_misc_tests_data() {
	static const MiscTestsData data = read_misc_tests_data();
	return data;
}

bool contains_code(const CodeSet& set, Code code) { return set.find(code) != set.end(); }

// Inserts a value (like Rust's HashMap::extend(): later values overwrite existing ones)
template <typename K, typename V>
void extend(std::unordered_map<K, V>& map, K key, V value) {
	map[key] = value;
}

template <typename K, typename V>
V get_or(const std::unordered_map<K, V>& map, K key, V default_value) {
	const auto it = map.find(key);
	return it == map.end() ? default_value : it->second;
}

} // namespace

TEST_CASE("core/info/memory_size_info") {
	const auto test_cases = read_memory_size_info_test_cases(get_instr_info_unit_tests_dir() + "/MemorySizeInfo.txt");
	std::unordered_set<MemorySize> h;
	for (const auto& tc : test_cases)
		h.insert(tc.memory_size);
	// Make sure every value is tested
	CHECK_EQ(h.size(), IcedConstants::MEMORY_SIZE_ENUM_COUNT);
	// Make sure there are no dupes
	CHECK_EQ(test_cases.size(), IcedConstants::MEMORY_SIZE_ENUM_COUNT);
	for (const auto& tc : test_cases) {
		const MemorySizeInfo& info = memory_size_ext::info(tc.memory_size);
		CHECK(info.memory_size() == tc.memory_size);
		CHECK_EQ(info.size(), tc.size);
		CHECK_EQ(info.element_size(), tc.element_size);
		CHECK(info.element_type() == tc.element_type);
		CHECK_EQ(info.is_signed(), (tc.flags & MemorySizeFlags::SIGNED) != 0);
		CHECK_EQ(info.is_broadcast(), (tc.flags & MemorySizeFlags::BROADCAST) != 0);
		CHECK_EQ(info.is_packed(), (tc.flags & MemorySizeFlags::PACKED) != 0);
		CHECK_EQ(info.element_count(), tc.element_count);

		CHECK_EQ(memory_size_ext::size(tc.memory_size), tc.size);
		CHECK_EQ(memory_size_ext::element_size(tc.memory_size), tc.element_size);
		CHECK(memory_size_ext::element_type(tc.memory_size) == tc.element_type);
		CHECK(memory_size_ext::element_type_info(tc.memory_size).memory_size() == tc.element_type);
		CHECK_EQ(memory_size_ext::is_signed(tc.memory_size), (tc.flags & MemorySizeFlags::SIGNED) != 0);
		CHECK_EQ(memory_size_ext::is_packed(tc.memory_size), (tc.flags & MemorySizeFlags::PACKED) != 0);
		CHECK_EQ(memory_size_ext::is_broadcast(tc.memory_size), (tc.flags & MemorySizeFlags::BROADCAST) != 0);
		CHECK_EQ(memory_size_ext::element_count(tc.memory_size), tc.element_count);
	}
}

TEST_CASE("core/info/register_info") {
	const auto test_cases = read_register_info_test_cases(get_instr_info_unit_tests_dir() + "/RegisterInfo.txt");
	std::unordered_set<Register> h;
	for (const auto& tc : test_cases)
		h.insert(tc.register_);
	// Make sure every value is tested
	CHECK_EQ(h.size(), IcedConstants::REGISTER_ENUM_COUNT);
	// Make sure there are no dupes
	CHECK_EQ(test_cases.size(), IcedConstants::REGISTER_ENUM_COUNT);
	for (const auto& tc : test_cases) {
		const RegisterInfo& info = register_ext::info(tc.register_);
		CHECK(info.register_() == tc.register_);
		CHECK(info.base() == tc.base);
		CHECK_EQ(info.number(), tc.number);
		CHECK(info.full_register() == tc.full_register);
		CHECK(info.full_register32() == tc.full_register32);
		CHECK_EQ(info.size(), tc.size);

		CHECK(register_ext::base(tc.register_) == tc.base);
		CHECK_EQ(register_ext::number(tc.register_), tc.number);
		CHECK(register_ext::full_register(tc.register_) == tc.full_register);
		CHECK(register_ext::full_register32(tc.register_) == tc.full_register32);
		CHECK_EQ(register_ext::size(tc.register_), tc.size);

		constexpr std::uint32_t ALL_FLAGS = RegisterFlags::SEGMENT_REGISTER | RegisterFlags::GPR | RegisterFlags::GPR8 | RegisterFlags::GPR16 |
			RegisterFlags::GPR32 | RegisterFlags::GPR64 | RegisterFlags::XMM | RegisterFlags::YMM | RegisterFlags::ZMM | RegisterFlags::VECTOR_REGISTER |
			RegisterFlags::IP | RegisterFlags::K | RegisterFlags::BND | RegisterFlags::CR | RegisterFlags::DR | RegisterFlags::TR | RegisterFlags::ST |
			RegisterFlags::MM | RegisterFlags::TMM;
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

TEST_CASE("core/info/is_branch_call") {
	const auto& data = get_misc_tests_data();
	for (Code code : all_codes()) {
		Instruction instr;
		instr.set_code(code);

		CHECK_EQ(code_ext::is_jcc_short_or_near(code), contains_code(data.jcc_short, code) || contains_code(data.jcc_near, code));
		CHECK_EQ(instr.is_jcc_short_or_near(), code_ext::is_jcc_short_or_near(code));

		CHECK_EQ(code_ext::is_jcc_near(code), contains_code(data.jcc_near, code));
		CHECK_EQ(instr.is_jcc_near(), code_ext::is_jcc_near(code));

		CHECK_EQ(code_ext::is_jcc_short(code), contains_code(data.jcc_short, code));
		CHECK_EQ(instr.is_jcc_short(), code_ext::is_jcc_short(code));

		CHECK_EQ(code_ext::is_jcx_short(code), contains_code(data.jrcxz, code));
		CHECK_EQ(instr.is_jcx_short(), code_ext::is_jcx_short(code));

		CHECK_EQ(code_ext::is_jmp_short(code), contains_code(data.jmp_short, code));
		CHECK_EQ(instr.is_jmp_short(), code_ext::is_jmp_short(code));

		CHECK_EQ(code_ext::is_jmp_near(code), contains_code(data.jmp_near, code));
		CHECK_EQ(instr.is_jmp_near(), code_ext::is_jmp_near(code));

		CHECK_EQ(code_ext::is_jmp_short_or_near(code), contains_code(data.jmp_short, code) || contains_code(data.jmp_near, code));
		CHECK_EQ(instr.is_jmp_short_or_near(), code_ext::is_jmp_short_or_near(code));

		CHECK_EQ(code_ext::is_jmp_far(code), contains_code(data.jmp_far, code));
		CHECK_EQ(instr.is_jmp_far(), code_ext::is_jmp_far(code));

		CHECK_EQ(code_ext::is_call_near(code), contains_code(data.call_near, code));
		CHECK_EQ(instr.is_call_near(), code_ext::is_call_near(code));

		CHECK_EQ(code_ext::is_call_far(code), contains_code(data.call_far, code));
		CHECK_EQ(instr.is_call_far(), code_ext::is_call_far(code));

		CHECK_EQ(code_ext::is_jmp_near_indirect(code), contains_code(data.jmp_near_indirect, code));
		CHECK_EQ(instr.is_jmp_near_indirect(), code_ext::is_jmp_near_indirect(code));

		CHECK_EQ(code_ext::is_jmp_far_indirect(code), contains_code(data.jmp_far_indirect, code));
		CHECK_EQ(instr.is_jmp_far_indirect(), code_ext::is_jmp_far_indirect(code));

		CHECK_EQ(code_ext::is_call_near_indirect(code), contains_code(data.call_near_indirect, code));
		CHECK_EQ(instr.is_call_near_indirect(), code_ext::is_call_near_indirect(code));

		CHECK_EQ(code_ext::is_call_far_indirect(code), contains_code(data.call_far_indirect, code));
		CHECK_EQ(instr.is_call_far_indirect(), code_ext::is_call_far_indirect(code));

		CHECK_EQ(code_ext::is_jkcc_short_or_near(code), contains_code(data.jkcc_short, code) || contains_code(data.jkcc_near, code));
		CHECK_EQ(instr.is_jkcc_short_or_near(), code_ext::is_jkcc_short_or_near(code));

		CHECK_EQ(code_ext::is_jkcc_near(code), contains_code(data.jkcc_near, code));
		CHECK_EQ(instr.is_jkcc_near(), code_ext::is_jkcc_near(code));

		CHECK_EQ(code_ext::is_jkcc_short(code), contains_code(data.jkcc_short, code));
		CHECK_EQ(instr.is_jkcc_short(), code_ext::is_jkcc_short(code));

		CHECK_EQ(contains_code(data.loop_, code), code_ext::is_loop(code) || code_ext::is_loopcc(code));
		CHECK_EQ(code_ext::is_loop(code), instr.is_loop());
		CHECK_EQ(code_ext::is_loopcc(code), instr.is_loopcc());
	}
}

TEST_CASE("core/info/verify_negate_condition_code") {
	const auto& data = get_misc_tests_data();

	std::unordered_map<Code, Code> to_negated_code_value;
	for (const auto& a : data.jcc_short_infos)
		extend(to_negated_code_value, std::get<0>(a), std::get<1>(a));
	for (const auto& a : data.jcc_near_infos)
		extend(to_negated_code_value, std::get<0>(a), std::get<1>(a));
	for (const auto& a : data.setcc_infos)
		extend(to_negated_code_value, std::get<0>(a), std::get<1>(a));
	for (const auto& a : data.cmovcc_infos)
		extend(to_negated_code_value, std::get<0>(a), std::get<1>(a));
	for (const auto& a : data.cmpccxadd_infos)
		extend(to_negated_code_value, std::get<0>(a), std::get<1>(a));
	for (const auto& a : data.loopcc_infos)
		extend(to_negated_code_value, std::get<0>(a), std::get<1>(a));
	for (const auto& a : data.jkcc_short_infos)
		extend(to_negated_code_value, std::get<0>(a), std::get<1>(a));
	for (const auto& a : data.jkcc_near_infos)
		extend(to_negated_code_value, std::get<0>(a), std::get<1>(a));
	CHECK(!to_negated_code_value.empty());

	for (Code code : all_codes()) {
		Instruction instr;
		instr.set_code(code);

		const Code negated = get_or(to_negated_code_value, code, code);

		CHECK(code_ext::negate_condition_code(code) == negated);
		instr.negate_condition_code();
		CHECK(instr.code() == negated);
	}
}

TEST_CASE("core/info/verify_to_short_branch") {
	const auto& data = get_misc_tests_data();

	std::unordered_map<Code, Code> as_short_branch;
	for (const auto& a : data.jcc_near_infos)
		extend(as_short_branch, std::get<0>(a), std::get<2>(a));
	for (const auto& a : data.jmp_infos)
		extend(as_short_branch, a.second, a.first);
	for (const auto& a : data.jkcc_near_infos)
		extend(as_short_branch, std::get<0>(a), std::get<2>(a));
	CHECK(!as_short_branch.empty());

	for (Code code : all_codes()) {
		Instruction instr;
		instr.set_code(code);

		const Code short_code = get_or(as_short_branch, code, code);

		CHECK(code_ext::as_short_branch(code) == short_code);
		instr.as_short_branch();
		CHECK(instr.code() == short_code);
	}
}

TEST_CASE("core/info/verify_to_near_branch") {
	const auto& data = get_misc_tests_data();

	std::unordered_map<Code, Code> as_near_branch;
	for (const auto& a : data.jcc_short_infos)
		extend(as_near_branch, std::get<0>(a), std::get<2>(a));
	for (const auto& a : data.jmp_infos)
		extend(as_near_branch, a.first, a.second);
	for (const auto& a : data.jkcc_short_infos)
		extend(as_near_branch, std::get<0>(a), std::get<2>(a));
	CHECK(!as_near_branch.empty());

	for (Code code : all_codes()) {
		Instruction instr;
		instr.set_code(code);

		const Code near_code = get_or(as_near_branch, code, code);

		CHECK(code_ext::as_near_branch(code) == near_code);
		instr.as_near_branch();
		CHECK(instr.code() == near_code);
	}
}

TEST_CASE("core/info/verify_condition_code") {
	const auto& data = get_misc_tests_data();

	std::unordered_map<Code, ConditionCode> to_condition_code;
	for (const auto& a : data.jcc_short_infos)
		extend(to_condition_code, std::get<0>(a), std::get<3>(a));
	for (const auto& a : data.jcc_near_infos)
		extend(to_condition_code, std::get<0>(a), std::get<3>(a));
	for (const auto& a : data.setcc_infos)
		extend(to_condition_code, std::get<0>(a), std::get<2>(a));
	for (const auto& a : data.cmovcc_infos)
		extend(to_condition_code, std::get<0>(a), std::get<2>(a));
	for (const auto& a : data.cmpccxadd_infos)
		extend(to_condition_code, std::get<0>(a), std::get<2>(a));
	for (const auto& a : data.loopcc_infos)
		extend(to_condition_code, std::get<0>(a), std::get<2>(a));
	for (const auto& a : data.jkcc_short_infos)
		extend(to_condition_code, std::get<0>(a), std::get<3>(a));
	for (const auto& a : data.jkcc_near_infos)
		extend(to_condition_code, std::get<0>(a), std::get<3>(a));
	CHECK(!to_condition_code.empty());

	for (Code code : all_codes()) {
		Instruction instr;
		instr.set_code(code);

		const ConditionCode cc = get_or(to_condition_code, code, ConditionCode::None);

		CHECK(code_ext::condition_code(code) == cc);
		CHECK(instr.condition_code() == cc);
	}
}

TEST_CASE("core/info/verify_string_instr") {
	const auto& data = get_misc_tests_data();
	CHECK(!data.string.empty());

	for (Code code : all_codes()) {
		Instruction instr;
		instr.set_code(code);

		CHECK_EQ(code_ext::is_string_instruction(code), contains_code(data.string, code));
		CHECK_EQ(instr.is_string_instruction(), code_ext::is_string_instruction(code));
	}
}

TEST_CASE("core/info/verify_condition_code_values_are_in_correct_order") {
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
	CHECK(true);
}
