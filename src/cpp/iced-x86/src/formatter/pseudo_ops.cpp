// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "internal/formatter/pseudo_ops.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "internal/formatter/pseudo_ops_defs.hpp"
#include "internal/iced_assert.hpp"

namespace iced_x86::internal {

namespace {
struct PseudoOpsTable {
	std::array<std::vector<FormatterString>, pseudo_ops_defs::PSEUDO_OPS_KIND_COUNT> pseudo_ops;

	PseudoOpsTable() {
		constexpr std::size_t CAP = 14;
		std::string sb;
		sb.reserve(CAP);
		for (const auto& def : pseudo_ops_defs::PSEUDO_OPS_DEFS) {
			auto& strings = pseudo_ops[static_cast<std::size_t>(def.kind)];
			strings.reserve(def.size);
			for (std::size_t i = 0; i < def.size; i++) {
				sb.clear();
				sb.append(def.prefix.data(), def.prefix.size());
				sb.append(def.cc[i].data(), def.cc[i].size());
				sb.append(def.suffix.data(), def.suffix.size());
				strings.emplace_back(sb);
			}
		}
	}
};
} // namespace

const std::vector<FormatterString>& get_pseudo_ops(PseudoOpsKind kind) {
	static const PseudoOpsTable table;
	const auto index = static_cast<std::size_t>(kind);
	ICED_ASSERT(index < table.pseudo_ops.size());
	return table.pseudo_ops[index];
}

} // namespace iced_x86::internal
