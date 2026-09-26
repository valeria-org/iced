// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Fast formatter tables (Rust: formatter/fast/{fmt_tbl,mem_size_tbl,regs,pseudo_ops_fast}.rs)

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string_view>
#include <vector>

#include "iced_x86/internal/fast_fmt.hpp"
#include "iced_x86/register.hpp"
#include "internal/formatter/fast/fast_fmt_flags.hpp"
#include "internal/formatter/fast/fmt_data.hpp"
#include "internal/formatter/fast/mem_size_tbl_data.hpp"
#include "internal/formatter/pseudo_ops_kind.hpp"
#include "internal/formatter/regs_tbl.hpp"
#include "internal/formatter/strings_data.hpp"
#include "internal/iced_assert.hpp"

namespace iced_x86::internal::fast {

// If this fails, the generator was updated and now the constant must be updated too
static_assert(MAX_MNEMONIC_STRING_LEN == strings_data::MAX_STRING_LEN, "");
// If this fails, change FastString20 to eg. FastString24 (multiple of 4 or 8 depending on what's best for PERF)
static_assert(strings_data::MAX_STRING_LEN <= FastStringMnemonic::SIZE, "");
// If this fails, the generator was updated and now FastStringMnemonic must be changed to the correct type
static_assert(FastStringMnemonic::SIZE == strings_data::VALID_STRING_LENGTH, "");
// If this fails, the generator was updated and now FastStringRegister must be changed to the correct type
static_assert(FastStringRegister::SIZE == regs_tbl::VALID_STRING_LENGTH, "");
// If this fails, update the FastStringMemorySize type
static_assert(MAX_MEMORY_SIZE_STR_LEN <= FastStringMemorySize::SIZE, "");
static_assert(MAX_MEMORY_SIZE_STR_LEN > FastStringMemorySize::SIZE - 4, "");
static_assert(MEM_SIZE_TBL_STRING_SIZE == FastStringMemorySize::SIZE, "");
static_assert(FAST_FMT_FLAGS_FORCE_MEM_SIZE == FastFmtFlags::FORCE_MEM_SIZE, "");
static_assert(FAST_FMT_FLAGS_PSEUDO_OPS_KIND_SHIFT == FastFmtFlags::PSEUDO_OPS_KIND_SHIFT, "");
static_assert(sizeof(MEM_SIZE_TBL_DATA) / sizeof(MEM_SIZE_TBL_DATA[0]) == IcedConstants::MEMORY_SIZE_ENUM_COUNT, "");

namespace {

using FastStringMnemonicData = std::array<std::uint8_t, 1 + FastStringMnemonic::SIZE>;

struct FastFmtTablesHolder {
	std::array<FastStringRegister, IcedConstants::REGISTER_ENUM_COUNT> registers;
	std::array<FastStringMnemonic, IcedConstants::CODE_ENUM_COUNT> mnemonics;
	std::array<std::uint8_t, IcedConstants::CODE_ENUM_COUNT> flags;
	std::array<FastStringMemorySize, IcedConstants::MEMORY_SIZE_ENUM_COUNT> memory_sizes;
	std::array<std::uint8_t, IcedConstants::REGISTER_ENUM_COUNT> reg_to_addr_size;
	// Mnemonics with a 'v' prefix (the strings table doesn't store the 'v'). std::deque never moves its elements.
	std::deque<FastStringMnemonicData> v_mnemonics;
	FastFmtTables tables;

	FastFmtTablesHolder() {
		init_registers();
		init_mnemonics();
		init_memory_sizes();
		init_reg_to_addr_size();
		tables.registers = registers.data();
		tables.mnemonics = mnemonics.data();
		tables.flags = flags.data();
		tables.memory_sizes = memory_sizes.data();
		tables.reg_to_addr_size = reg_to_addr_size.data();
	}

	void init_registers() {
		using namespace regs_tbl;
		std::size_t index = 0;
		for (auto& reg : registers) {
			const std::size_t len = REGS_DATA[index];
			// It's safe to read FastStringRegister::SIZE bytes from the last string since the
			// table includes extra padding. See the static_asserts above and the table.
			ICED_ASSERT(index + 1 + FastStringRegister::SIZE <= REGS_DATA_SIZE);
			reg = FastStringRegister{&REGS_DATA[index]};
			index += 1 + len;
		}
		ICED_DEBUG_ASSERT(REGS_DATA_SIZE - index == PADDING_SIZE);
	}

	static std::vector<FastStringMnemonic> get_strings_table() {
		using namespace strings_data;
		std::vector<FastStringMnemonic> strings;
		strings.reserve(STRINGS_COUNT);
		std::size_t index = 0;
		for (std::size_t i = 0; i < STRINGS_COUNT; i++) {
			// It's safe to read FastStringMnemonic::SIZE bytes from the last string since the
			// table includes extra padding. See the static_asserts above and the table.
			ICED_ASSERT(index + 1 + FastStringMnemonic::SIZE <= STRINGS_TBL_DATA_SIZE);
			const std::size_t len = STRINGS_TBL_DATA[index];
			strings.push_back(FastStringMnemonic{&STRINGS_TBL_DATA[index]});
			index += 1 + len;
		}
		ICED_DEBUG_ASSERT(STRINGS_TBL_DATA_SIZE - index == PADDING_SIZE);
		return strings;
	}

	// Same as Rust's DataReader
	struct Reader {
		const std::uint8_t* data;
		std::size_t size;
		std::size_t index;

		std::uint32_t read_u8() {
			ICED_ASSERT(index < size);
			return data[index++];
		}

		std::uint32_t read_compressed_u32() {
			std::uint32_t result = 0;
			std::uint32_t shift = 0;
			for (;;) {
				ICED_DEBUG_ASSERT(shift < 32);
				const std::uint32_t b = read_u8();
				if ((b & 0x80) == 0)
					return result | (b << shift);
				result |= (b & 0x7F) << shift;
				shift += 7;
			}
		}
	};

	void init_mnemonics() {
		const auto strings = get_strings_table();
		Reader reader{FORMATTER_TBL_DATA, FORMATTER_TBL_DATA_SIZE, 0};
		std::size_t prev_index = 0;
		bool has_prev_index = false;
		std::uint32_t prev_flags = FastFmtFlags::NONE;
		for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
			const std::uint32_t f = reader.read_u8();
			std::size_t current_index = 0;
			bool restore_index = false;
			if ((f & FastFmtFlags::SAME_AS_PREV) != 0) {
				ICED_ASSERT(has_prev_index);
				current_index = reader.index;
				restore_index = true;
				reader.index = prev_index;
			}
			else {
				prev_index = reader.index;
				has_prev_index = true;
			}
			FastStringMnemonic mnemonic;
			if ((prev_flags & FastFmtFlags::HAS_VPREFIX) == (f & FastFmtFlags::HAS_VPREFIX) && (f & FastFmtFlags::SAME_AS_PREV) != 0) {
				ICED_ASSERT(i != 0);
				mnemonic = mnemonics[i - 1];
			}
			else if ((f & FastFmtFlags::HAS_VPREFIX) != 0) {
				const std::size_t string_index = reader.read_compressed_u32();
				ICED_ASSERT(string_index < strings.size());
				const auto old_str = strings[string_index];
				const std::size_t new_len = 1 + old_str.len();
				ICED_DEBUG_ASSERT(new_len <= strings_data::MAX_STRING_LEN);
				ICED_ASSERT(new_len <= FastStringMnemonic::SIZE);
				auto& data = v_mnemonics.emplace_back();
				data[0] = static_cast<std::uint8_t>(new_len);
				data[1] = 'v';
				for (std::size_t j = 0; j < FastStringMnemonic::SIZE - 1; j++)
					data[2 + j] = j < old_str.len() ? old_str.utf8_data()[j] : static_cast<std::uint8_t>(' ');
				mnemonic = FastStringMnemonic{data.data()};
			}
			else {
				const std::size_t string_index = reader.read_compressed_u32();
				ICED_ASSERT(string_index < strings.size());
				mnemonic = strings[string_index];
			}

			flags[i] = static_cast<std::uint8_t>(f);
			mnemonics[i] = mnemonic;
			prev_flags = f;

			if (restore_index)
				reader.index = current_index;
		}
		ICED_DEBUG_ASSERT(reader.index == reader.size);
	}

	void init_memory_sizes() {
		for (std::size_t i = 0; i < memory_sizes.size(); i++) {
			const std::size_t mem_keywords = MEM_SIZE_TBL_DATA[i];
			ICED_ASSERT(mem_keywords < MEM_SIZE_TBL_STRINGS_COUNT);
			memory_sizes[i] = FastStringMemorySize{reinterpret_cast<const std::uint8_t*>(MEM_SIZE_TBL_STRINGS[mem_keywords])};
		}
	}

	void init_reg_to_addr_size() {
		for (std::size_t i = 0; i < reg_to_addr_size.size(); i++) {
			const auto reg = static_cast<Register>(i);
			std::uint8_t size = 0;
			if (reg >= Register::AX && reg <= Register::R15W)
				size = 2;
			else if (reg >= Register::EAX && reg <= Register::R15D)
				size = 4;
			else if (reg >= Register::RAX && reg <= Register::R15)
				size = 8;
			else if (reg == Register::EIP)
				size = 4;
			else if (reg == Register::RIP)
				size = 8;
			reg_to_addr_size[i] = size;
		}
	}
};

// Keep this in sync with pseudo_ops.cpp
struct FastPseudoOps {
	static constexpr std::size_t PSEUDO_OPS_KIND_COUNT = static_cast<std::size_t>(PseudoOpsKind::vpcmpud6) + 1;

	std::array<std::vector<FastStringMnemonic>, PSEUDO_OPS_KIND_COUNT> pseudo_ops;
	// std::deque never moves its elements
	std::deque<FastStringMnemonicData> strings;

	template <std::size_t N>
	ICED_NOINLINE std::vector<FastStringMnemonic> create(const std::string_view (&cc)[N], std::size_t size, std::string_view prefix, std::string_view suffix) {
		std::vector<FastStringMnemonic> result;
		result.reserve(size);
		for (const auto cc_s : cc) {
			if (result.size() == size)
				break;
			const std::size_t new_len = prefix.size() + cc_s.size() + suffix.size();
			ICED_ASSERT(new_len <= FastStringMnemonic::SIZE);
			auto& data = strings.emplace_back();
			data.fill(' ');
			data[0] = static_cast<std::uint8_t>(new_len);
			std::size_t index = 1;
			for (const char c : prefix)
				data[index++] = static_cast<std::uint8_t>(c);
			for (const char c : cc_s)
				data[index++] = static_cast<std::uint8_t>(c);
			for (const char c : suffix)
				data[index++] = static_cast<std::uint8_t>(c);
			result.push_back(FastStringMnemonic{data.data()});
		}
		return result;
	}

	template <std::size_t N>
	ICED_NOINLINE void create_strings(PseudoOpsKind kind, const char* const (&values)[N]) {
		std::vector<FastStringMnemonic> result;
		result.reserve(N);
		for (const char* value : values) {
			const std::string_view s(value);
			ICED_ASSERT(s.size() <= FastStringMnemonic::SIZE);
			auto& data = strings.emplace_back();
			data.fill(' ');
			data[0] = static_cast<std::uint8_t>(s.size());
			for (std::size_t i = 0; i < s.size(); i++)
				data[1 + i] = static_cast<std::uint8_t>(s[i]);
			result.push_back(FastStringMnemonic{data.data()});
		}
		set(kind, std::move(result));
	}

	ICED_NOINLINE void set(PseudoOpsKind kind, std::vector<FastStringMnemonic> value) { pseudo_ops[static_cast<std::size_t>(kind)] = std::move(value); }

	FastPseudoOps() {
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
		set(PseudoOpsKind::cmpps, create(cc, 8, "cmp", "ps"));
		set(PseudoOpsKind::vcmpps, create(cc, 32, "vcmp", "ps"));
		set(PseudoOpsKind::cmppd, create(cc, 8, "cmp", "pd"));
		set(PseudoOpsKind::vcmppd, create(cc, 32, "vcmp", "pd"));
		set(PseudoOpsKind::cmpss, create(cc, 8, "cmp", "ss"));
		set(PseudoOpsKind::vcmpss, create(cc, 32, "vcmp", "ss"));
		set(PseudoOpsKind::cmpsd, create(cc, 8, "cmp", "sd"));
		set(PseudoOpsKind::vcmpsd, create(cc, 32, "vcmp", "sd"));
		set(PseudoOpsKind::vcmpph, create(cc, 32, "vcmp", "ph"));
		set(PseudoOpsKind::vcmpsh, create(cc, 32, "vcmp", "sh"));
		set(PseudoOpsKind::vcmpps8, create(cc, 8, "vcmp", "ps"));
		set(PseudoOpsKind::vcmppd8, create(cc, 8, "vcmp", "pd"));

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
		set(PseudoOpsKind::vpcmpd6, create(cc6, 8, "vpcmp", "d"));
		set(PseudoOpsKind::vpcmpud6, create(cc6, 8, "vpcmp", "ud"));

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
		set(PseudoOpsKind::vpcomb, create(xopcc, 8, "vpcom", "b"));
		set(PseudoOpsKind::vpcomw, create(xopcc, 8, "vpcom", "w"));
		set(PseudoOpsKind::vpcomd, create(xopcc, 8, "vpcom", "d"));
		set(PseudoOpsKind::vpcomq, create(xopcc, 8, "vpcom", "q"));
		set(PseudoOpsKind::vpcomub, create(xopcc, 8, "vpcom", "ub"));
		set(PseudoOpsKind::vpcomuw, create(xopcc, 8, "vpcom", "uw"));
		set(PseudoOpsKind::vpcomud, create(xopcc, 8, "vpcom", "ud"));
		set(PseudoOpsKind::vpcomuq, create(xopcc, 8, "vpcom", "uq"));

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
		set(PseudoOpsKind::vpcmpb, create(pcmpcc, 8, "vpcmp", "b"));
		set(PseudoOpsKind::vpcmpw, create(pcmpcc, 8, "vpcmp", "w"));
		set(PseudoOpsKind::vpcmpd, create(pcmpcc, 8, "vpcmp", "d"));
		set(PseudoOpsKind::vpcmpq, create(pcmpcc, 8, "vpcmp", "q"));
		set(PseudoOpsKind::vpcmpub, create(pcmpcc, 8, "vpcmp", "ub"));
		set(PseudoOpsKind::vpcmpuw, create(pcmpcc, 8, "vpcmp", "uw"));
		set(PseudoOpsKind::vpcmpud, create(pcmpcc, 8, "vpcmp", "ud"));
		set(PseudoOpsKind::vpcmpuq, create(pcmpcc, 8, "vpcmp", "uq"));

		static constexpr const char* pclmulqdq[4] = {
			"pclmullqlqdq",
			"pclmulhqlqdq",
			"pclmullqhqdq",
			"pclmulhqhqdq",
		};
		create_strings(PseudoOpsKind::pclmulqdq, pclmulqdq);
		static constexpr const char* vpclmulqdq[4] = {
			"vpclmullqlqdq",
			"vpclmulhqlqdq",
			"vpclmullqhqdq",
			"vpclmulhqhqdq",
		};
		create_strings(PseudoOpsKind::vpclmulqdq, vpclmulqdq);
	}
};

const FastPseudoOps& get_fast_pseudo_ops() {
	static const FastPseudoOps pseudo_ops;
	return pseudo_ops;
}

} // namespace

const FastFmtTables& get_fast_fmt_tables() {
	static const FastFmtTablesHolder holder;
	return holder.tables;
}

bool try_get_pseudo_op(Code code, std::uint32_t pseudo_ops_num, std::uint32_t imm8, FastStringMnemonic& mnemonic) {
	ICED_DEBUG_ASSERT(pseudo_ops_num != 0);
	std::size_t index = imm8;
	// The generator generates only valid values (1-based)
	auto pseudo_ops_kind = static_cast<PseudoOpsKind>(pseudo_ops_num - 1);
	// Not enough bits to store all values so some are mapped to the same value. Fix that here.
	if (pseudo_ops_kind == PseudoOpsKind::vpcmpd6 && code == Code::MVEX_Vpcmpud_kr_k1_zmm_zmmmt_imm8)
		pseudo_ops_kind = PseudoOpsKind::vpcmpud6;
	const auto& pseudo_ops_table = get_fast_pseudo_ops().pseudo_ops;
	ICED_ASSERT(static_cast<std::size_t>(pseudo_ops_kind) < pseudo_ops_table.size());
	const auto& pseudo_ops = pseudo_ops_table[static_cast<std::size_t>(pseudo_ops_kind)];
	if (pseudo_ops_kind == PseudoOpsKind::pclmulqdq || pseudo_ops_kind == PseudoOpsKind::vpclmulqdq) {
		if (index <= 1) {
			// nothing
		}
		else if (index == 0x10)
			index = 2;
		else if (index == 0x11)
			index = 3;
		else
			index = static_cast<std::size_t>(-1);
	}
	if (index < pseudo_ops.size()) {
		mnemonic = pseudo_ops[index];
		return true;
	}
	return false;
}

} // namespace iced_x86::internal::fast
