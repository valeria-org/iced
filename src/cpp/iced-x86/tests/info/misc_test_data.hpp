// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

#include "iced_x86/code.hpp"
#include "iced_x86/condition_code.hpp"
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace iced_x86::tests::instr_info {

struct MiscTestsData {
	std::unordered_set<Code> jcc_short;
	std::unordered_set<Code> jmp_near;
	std::unordered_set<Code> jmp_far;
	std::unordered_set<Code> jmp_short;
	std::unordered_set<Code> jmp_near_indirect;
	std::unordered_set<Code> jmp_far_indirect;
	std::unordered_set<Code> jcc_near;
	std::unordered_set<Code> call_far;
	std::unordered_set<Code> call_near;
	std::unordered_set<Code> call_near_indirect;
	std::unordered_set<Code> call_far_indirect;
	std::unordered_set<Code> jmpe_near;
	std::unordered_set<Code> jmpe_near_indirect;
	std::unordered_set<Code> loop_;
	std::unordered_set<Code> jrcxz;
	std::unordered_set<Code> xbegin;
	std::unordered_set<Code> string;
	std::unordered_set<Code> jkcc_short;
	std::unordered_set<Code> jkcc_near;
	std::vector<std::pair<Code, Code>> jmp_infos;
	std::vector<std::tuple<Code, Code, Code, ConditionCode>> jcc_short_infos;
	std::vector<std::tuple<Code, Code, Code, ConditionCode>> jcc_near_infos;
	std::vector<std::tuple<Code, Code, Code, ConditionCode>> jkcc_short_infos;
	std::vector<std::tuple<Code, Code, Code, ConditionCode>> jkcc_near_infos;
	std::vector<std::tuple<Code, Code, ConditionCode>> setcc_infos;
	std::vector<std::tuple<Code, Code, ConditionCode>> cmovcc_infos;
	std::vector<std::tuple<Code, Code, ConditionCode>> cmpccxadd_infos;
	std::vector<std::tuple<Code, Code, ConditionCode>> loopcc_infos;
};

// Reads InstructionInfo/Misc.txt (cached)
const MiscTestsData& get_misc_tests_data();

} // namespace iced_x86::tests::instr_info
