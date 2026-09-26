// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "internal/formatter/pseudo_ops.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "internal/iced_assert.hpp"

namespace iced_x86::internal {

static constexpr std::size_t PSEUDO_OPS_KIND_COUNT = static_cast<std::size_t>(PseudoOpsKind::vpcmpud6) + 1;

using PseudoOps = std::array<std::vector<FormatterString>, PSEUDO_OPS_KIND_COUNT>;

template <std::size_t N>
static ICED_NOINLINE std::vector<FormatterString> create(std::string& sb, const std::string_view (&cc)[N], std::size_t size, std::string_view prefix,
										   std::string_view suffix) {
	std::vector<FormatterString> strings;
	strings.reserve(size);
	for (const auto cc_s : cc) {
		if (strings.size() == size)
			break;
		sb.clear();
		sb.append(prefix.data(), prefix.size());
		sb.append(cc_s.data(), cc_s.size());
		sb.append(suffix.data(), suffix.size());
		strings.emplace_back(sb);
	}
	return strings;
}

static ICED_NOINLINE void set(PseudoOps& pseudo_ops, PseudoOpsKind kind, std::vector<FormatterString> strings) {
	pseudo_ops[static_cast<std::size_t>(kind)] = std::move(strings);
}

static void init_pseudo_ops(PseudoOps& pseudo_ops) {
	constexpr std::size_t CAP = 14;
	std::string sb;
	sb.reserve(CAP);

	static constexpr std::string_view cc[32] = {
		"eq",
		"lt",
		"le",
		"unord",
		"neq",
		"nlt",
		"nle",
		"ord",
		"eq_uq",
		"nge",
		"ngt",
		"false",
		"neq_oq",
		"ge",
		"gt",
		"true",
		"eq_os",
		"lt_oq",
		"le_oq",
		"unord_s",
		"neq_us",
		"nlt_uq",
		"nle_uq",
		"ord_s",
		"eq_us",
		"nge_uq",
		"ngt_uq",
		"false_os",
		"neq_os",
		"ge_oq",
		"gt_oq",
		"true_us",
	};
	set(pseudo_ops, PseudoOpsKind::cmpps, create(sb, cc, 8, "cmp", "ps"));
	set(pseudo_ops, PseudoOpsKind::vcmpps, create(sb, cc, 32, "vcmp", "ps"));
	set(pseudo_ops, PseudoOpsKind::cmppd, create(sb, cc, 8, "cmp", "pd"));
	set(pseudo_ops, PseudoOpsKind::vcmppd, create(sb, cc, 32, "vcmp", "pd"));
	set(pseudo_ops, PseudoOpsKind::cmpss, create(sb, cc, 8, "cmp", "ss"));
	set(pseudo_ops, PseudoOpsKind::vcmpss, create(sb, cc, 32, "vcmp", "ss"));
	set(pseudo_ops, PseudoOpsKind::cmpsd, create(sb, cc, 8, "cmp", "sd"));
	set(pseudo_ops, PseudoOpsKind::vcmpsd, create(sb, cc, 32, "vcmp", "sd"));
	set(pseudo_ops, PseudoOpsKind::vcmpph, create(sb, cc, 32, "vcmp", "ph"));
	set(pseudo_ops, PseudoOpsKind::vcmpsh, create(sb, cc, 32, "vcmp", "sh"));
	set(pseudo_ops, PseudoOpsKind::vcmpps8, create(sb, cc, 8, "vcmp", "ps"));
	set(pseudo_ops, PseudoOpsKind::vcmppd8, create(sb, cc, 8, "vcmp", "pd"));

	static constexpr std::string_view cc6[8] = {
		"eq",
		"lt",
		"le",
		"??",
		"neq",
		"nlt",
		"nle",
		"???",
	};
	set(pseudo_ops, PseudoOpsKind::vpcmpd6, create(sb, cc6, 8, "vpcmp", "d"));
	set(pseudo_ops, PseudoOpsKind::vpcmpud6, create(sb, cc6, 8, "vpcmp", "ud"));

	static constexpr std::string_view xopcc[8] = {
		"lt",
		"le",
		"gt",
		"ge",
		"eq",
		"neq",
		"false",
		"true",
	};
	set(pseudo_ops, PseudoOpsKind::vpcomb, create(sb, xopcc, 8, "vpcom", "b"));
	set(pseudo_ops, PseudoOpsKind::vpcomw, create(sb, xopcc, 8, "vpcom", "w"));
	set(pseudo_ops, PseudoOpsKind::vpcomd, create(sb, xopcc, 8, "vpcom", "d"));
	set(pseudo_ops, PseudoOpsKind::vpcomq, create(sb, xopcc, 8, "vpcom", "q"));
	set(pseudo_ops, PseudoOpsKind::vpcomub, create(sb, xopcc, 8, "vpcom", "ub"));
	set(pseudo_ops, PseudoOpsKind::vpcomuw, create(sb, xopcc, 8, "vpcom", "uw"));
	set(pseudo_ops, PseudoOpsKind::vpcomud, create(sb, xopcc, 8, "vpcom", "ud"));
	set(pseudo_ops, PseudoOpsKind::vpcomuq, create(sb, xopcc, 8, "vpcom", "uq"));

	static constexpr std::string_view pcmpcc[8] = {
		"eq",
		"lt",
		"le",
		"false",
		"neq",
		"nlt",
		"nle",
		"true",
	};
	set(pseudo_ops, PseudoOpsKind::vpcmpb, create(sb, pcmpcc, 8, "vpcmp", "b"));
	set(pseudo_ops, PseudoOpsKind::vpcmpw, create(sb, pcmpcc, 8, "vpcmp", "w"));
	set(pseudo_ops, PseudoOpsKind::vpcmpd, create(sb, pcmpcc, 8, "vpcmp", "d"));
	set(pseudo_ops, PseudoOpsKind::vpcmpq, create(sb, pcmpcc, 8, "vpcmp", "q"));
	set(pseudo_ops, PseudoOpsKind::vpcmpub, create(sb, pcmpcc, 8, "vpcmp", "ub"));
	set(pseudo_ops, PseudoOpsKind::vpcmpuw, create(sb, pcmpcc, 8, "vpcmp", "uw"));
	set(pseudo_ops, PseudoOpsKind::vpcmpud, create(sb, pcmpcc, 8, "vpcmp", "ud"));
	set(pseudo_ops, PseudoOpsKind::vpcmpuq, create(sb, pcmpcc, 8, "vpcmp", "uq"));

	static constexpr std::string_view pclmulqdq[4] = {
		"pclmullqlqdq",
		"pclmulhqlqdq",
		"pclmullqhqdq",
		"pclmulhqhqdq",
	};
	set(pseudo_ops, PseudoOpsKind::pclmulqdq, create(sb, pclmulqdq, 4, "", ""));
	static constexpr std::string_view vpclmulqdq[4] = {
		"vpclmullqlqdq",
		"vpclmulhqlqdq",
		"vpclmullqhqdq",
		"vpclmulhqhqdq",
	};
	set(pseudo_ops, PseudoOpsKind::vpclmulqdq, create(sb, vpclmulqdq, 4, "", ""));

	ICED_DEBUG_ASSERT(sb.capacity() >= CAP);
}

namespace {
struct PseudoOpsTable {
	PseudoOps pseudo_ops;
	PseudoOpsTable() { init_pseudo_ops(pseudo_ops); }
};
} // namespace

const std::vector<FormatterString>& get_pseudo_ops(PseudoOpsKind kind) {
	static const PseudoOpsTable table;
	const auto index = static_cast<std::size_t>(kind);
	ICED_ASSERT(index < table.pseudo_ops.size());
	return table.pseudo_ops[index];
}

} // namespace iced_x86::internal
