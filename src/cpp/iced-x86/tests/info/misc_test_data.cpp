// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "info/misc_test_data.hpp"
#include "generated/misc_section_names.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/section_file_reader.hpp"
#include "test_utils/str_utils.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace iced_x86::tests::instr_info {

namespace {

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

class MiscTestsDataReader {
public:
	MiscTestsData data;

	void read() {
		std::vector<std::pair<std::string, std::uint32_t>> infos = {
			{MiscSectionNames::JCC_SHORT, MiscSectionNameIds::JCC_SHORT},
			{MiscSectionNames::JMP_NEAR, MiscSectionNameIds::JMP_NEAR},
			{MiscSectionNames::JMP_FAR, MiscSectionNameIds::JMP_FAR},
			{MiscSectionNames::JMP_SHORT, MiscSectionNameIds::JMP_SHORT},
			{MiscSectionNames::JMP_NEAR_INDIRECT, MiscSectionNameIds::JMP_NEAR_INDIRECT},
			{MiscSectionNames::JMP_FAR_INDIRECT, MiscSectionNameIds::JMP_FAR_INDIRECT},
			{MiscSectionNames::JCC_NEAR, MiscSectionNameIds::JCC_NEAR},
			{MiscSectionNames::CALL_FAR, MiscSectionNameIds::CALL_FAR},
			{MiscSectionNames::CALL_NEAR, MiscSectionNameIds::CALL_NEAR},
			{MiscSectionNames::CALL_NEAR_INDIRECT, MiscSectionNameIds::CALL_NEAR_INDIRECT},
			{MiscSectionNames::CALL_FAR_INDIRECT, MiscSectionNameIds::CALL_FAR_INDIRECT},
			{MiscSectionNames::JMPE_NEAR, MiscSectionNameIds::JMPE_NEAR},
			{MiscSectionNames::JMPE_NEAR_INDIRECT, MiscSectionNameIds::JMPE_NEAR_INDIRECT},
			{MiscSectionNames::LOOP, MiscSectionNameIds::LOOP},
			{MiscSectionNames::JRCXZ, MiscSectionNameIds::JRCXZ},
			{MiscSectionNames::XBEGIN, MiscSectionNameIds::XBEGIN},
			{MiscSectionNames::JMP_INFO, MiscSectionNameIds::JMP_INFO},
			{MiscSectionNames::JCC_SHORT_INFO, MiscSectionNameIds::JCC_SHORT_INFO},
			{MiscSectionNames::JCC_NEAR_INFO, MiscSectionNameIds::JCC_NEAR_INFO},
			{MiscSectionNames::SETCC_INFO, MiscSectionNameIds::SETCC_INFO},
			{MiscSectionNames::CMOVCC_INFO, MiscSectionNameIds::CMOVCC_INFO},
			{MiscSectionNames::CMPCCXADD_INFO, MiscSectionNameIds::CMPCCXADD_INFO},
			{MiscSectionNames::LOOPCC_INFO, MiscSectionNameIds::LOOPCC_INFO},
			{MiscSectionNames::STRING_INSTRUCTION, MiscSectionNameIds::STRING_INSTRUCTION},
			{MiscSectionNames::JKCC_SHORT, MiscSectionNameIds::JKCC_SHORT},
			{MiscSectionNames::JKCC_NEAR, MiscSectionNameIds::JKCC_NEAR},
			{MiscSectionNames::JKCC_SHORT_INFO, MiscSectionNameIds::JKCC_SHORT_INFO},
			{MiscSectionNames::JKCC_NEAR_INFO, MiscSectionNameIds::JKCC_NEAR_INFO},
		};
		SectionFileReader reader(std::move(infos));
		reader.read(get_instr_info_unit_tests_dir() + "/Misc.txt", [this](std::uint32_t id, std::string_view line) { this->line(id, line); });
	}

	void line(std::uint32_t id, std::string_view line) {
		switch (id) {
		case MiscSectionNameIds::JCC_SHORT:
			add_code(data.jcc_short, line);
			break;
		case MiscSectionNameIds::JMP_NEAR:
			add_code(data.jmp_near, line);
			break;
		case MiscSectionNameIds::JMP_FAR:
			add_code(data.jmp_far, line);
			break;
		case MiscSectionNameIds::JMP_SHORT:
			add_code(data.jmp_short, line);
			break;
		case MiscSectionNameIds::JMP_NEAR_INDIRECT:
			add_code(data.jmp_near_indirect, line);
			break;
		case MiscSectionNameIds::JMP_FAR_INDIRECT:
			add_code(data.jmp_far_indirect, line);
			break;
		case MiscSectionNameIds::JCC_NEAR:
			add_code(data.jcc_near, line);
			break;
		case MiscSectionNameIds::CALL_FAR:
			add_code(data.call_far, line);
			break;
		case MiscSectionNameIds::CALL_NEAR:
			add_code(data.call_near, line);
			break;
		case MiscSectionNameIds::CALL_NEAR_INDIRECT:
			add_code(data.call_near_indirect, line);
			break;
		case MiscSectionNameIds::CALL_FAR_INDIRECT:
			add_code(data.call_far_indirect, line);
			break;
		case MiscSectionNameIds::JMPE_NEAR:
			add_code(data.jmpe_near, line);
			break;
		case MiscSectionNameIds::JMPE_NEAR_INDIRECT:
			add_code(data.jmpe_near_indirect, line);
			break;
		case MiscSectionNameIds::LOOP:
			add_code(data.loop_, line);
			break;
		case MiscSectionNameIds::JRCXZ:
			add_code(data.jrcxz, line);
			break;
		case MiscSectionNameIds::XBEGIN:
			add_code(data.xbegin, line);
			break;
		case MiscSectionNameIds::JMP_INFO:
			add_jmp_info(data.jmp_infos, line);
			break;
		case MiscSectionNameIds::JCC_SHORT_INFO:
			add_jcc_info(data.jcc_short_infos, line);
			break;
		case MiscSectionNameIds::JCC_NEAR_INFO:
			add_jcc_info(data.jcc_near_infos, line);
			break;
		case MiscSectionNameIds::SETCC_INFO:
			add_instr_cc_info(data.setcc_infos, line);
			break;
		case MiscSectionNameIds::CMOVCC_INFO:
			add_instr_cc_info(data.cmovcc_infos, line);
			break;
		case MiscSectionNameIds::CMPCCXADD_INFO:
			add_instr_cc_info(data.cmpccxadd_infos, line);
			break;
		case MiscSectionNameIds::LOOPCC_INFO:
			add_instr_cc_info(data.loopcc_infos, line);
			break;
		case MiscSectionNameIds::STRING_INSTRUCTION:
			add_code(data.string, line);
			break;
		case MiscSectionNameIds::JKCC_SHORT:
			add_code(data.jkcc_short, line);
			break;
		case MiscSectionNameIds::JKCC_NEAR:
			add_code(data.jkcc_near, line);
			break;
		case MiscSectionNameIds::JKCC_SHORT_INFO:
			add_jcc_info(data.jkcc_short_infos, line);
			break;
		case MiscSectionNameIds::JKCC_NEAR_INFO:
			add_jcc_info(data.jkcc_near_infos, line);
			break;
		default:
			throw std::runtime_error("unreachable");
		}
	}

private:
	static void add_code(std::unordered_set<Code>& h, std::string_view line) {
		if (!is_ignored_code(line))
			h.insert(to_code(line));
	}

	static void add_jmp_info(std::vector<std::pair<Code, Code>>& v, std::string_view line) {
		constexpr std::size_t ELEMS = 2;
		const std::vector<std::string_view> elems = split(line, ',');
		if (elems.size() != ELEMS)
			throw std::runtime_error("Expected " + std::to_string(ELEMS) + " elements, found " + std::to_string(elems.size()));
		if (!is_ignored_code(elems[0]) && !is_ignored_code(elems[1]))
			v.emplace_back(to_code(elems[0]), to_code(elems[1]));
	}

	static void add_jcc_info(std::vector<std::tuple<Code, Code, Code, ConditionCode>>& v, std::string_view line) {
		constexpr std::size_t ELEMS = 4;
		const std::vector<std::string_view> elems = split(line, ',');
		if (elems.size() != ELEMS)
			throw std::runtime_error("Expected " + std::to_string(ELEMS) + " elements, found " + std::to_string(elems.size()));
		if (!is_ignored_code(elems[0]) && !is_ignored_code(elems[1]) && !is_ignored_code(elems[2]))
			v.emplace_back(to_code(elems[0]), to_code(elems[1]), to_code(elems[2]), to_condition_code(elems[3]));
	}

	static void add_instr_cc_info(std::vector<std::tuple<Code, Code, ConditionCode>>& v, std::string_view line) {
		constexpr std::size_t ELEMS = 3;
		const std::vector<std::string_view> elems = split(line, ',');
		if (elems.size() != ELEMS)
			throw std::runtime_error("Expected " + std::to_string(ELEMS) + " elements, found " + std::to_string(elems.size()));
		if (!is_ignored_code(elems[0]) && !is_ignored_code(elems[1]))
			v.emplace_back(to_code(elems[0]), to_code(elems[1]), to_condition_code(elems[2]));
	}
};

MiscTestsData read_misc_tests_data() {
	MiscTestsDataReader reader;
	reader.read();
	return std::move(reader.data);
}

} // namespace

const MiscTestsData& get_misc_tests_data() {
	static const MiscTestsData data = read_misc_tests_data();
	return data;
}

} // namespace iced_x86::tests::instr_info
