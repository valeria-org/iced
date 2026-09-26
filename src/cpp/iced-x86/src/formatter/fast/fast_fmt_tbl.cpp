// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Fast formatter tables (Rust: formatter/fast/{fmt_tbl,mem_size_tbl,regs,pseudo_ops_fast}.rs)

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "iced_x86/internal/fast_fmt.hpp"
#include "iced_x86/register.hpp"
#include "internal/formatter/fast/fast_fmt_flags.hpp"
#include "internal/formatter/fast/fmt_data.hpp"
#include "internal/formatter/fast/mem_size_tbl_data.hpp"
#include "internal/formatter/pseudo_ops_defs.hpp"
#include "internal/formatter/pseudo_ops_kind.hpp"
#include "internal/formatter/regs_tbl.hpp"
#include "internal/formatter/strings_data.hpp"
#include "internal/data_reader.hpp"
#include "internal/iced_assert.hpp"
#include "internal/instruction_internal.hpp"

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
	// Mnemonics with a 'v' prefix (the strings table doesn't store the 'v'). std::deque never moves its elements.
	std::deque<FastStringMnemonicData> v_mnemonics;
	FastFmtTables tables;

	FastFmtTablesHolder() {
		init_registers();
		init_mnemonics();
		init_memory_sizes();
		tables.registers = registers.data();
		tables.mnemonics = mnemonics.data();
		tables.flags = flags.data();
		tables.memory_sizes = memory_sizes.data();
		tables.reg_to_addr_size = REG_TO_ADDR_SIZE;
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
		DataReader reader(STRINGS_TBL_DATA, STRINGS_TBL_DATA_SIZE);
		std::vector<FastStringMnemonic> strings;
		strings.reserve(STRINGS_COUNT);
		for (std::size_t i = 0; i < STRINGS_COUNT; i++) {
			// It's safe to read FastStringMnemonic::SIZE bytes from the last string since the
			// table includes extra padding. See the static_asserts above and the table.
			std::size_t size;
			const std::uint8_t* len_data = reader.read_len_data(size);
			ICED_ASSERT(size >= 1 + FastStringMnemonic::SIZE);
			strings.push_back(FastStringMnemonic{len_data});
		}
		ICED_DEBUG_ASSERT(reader.len_left() == PADDING_SIZE);
		return strings;
	}

	void init_mnemonics() {
		const auto strings = get_strings_table();
		DataReader reader(FORMATTER_TBL_DATA, FORMATTER_TBL_DATA_SIZE);
		std::size_t prev_index = 0;
		bool has_prev_index = false;
		std::uint32_t prev_flags = FastFmtFlags::NONE;
		for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
			const auto f = static_cast<std::uint32_t>(reader.read_u8());
			std::size_t current_index = 0;
			bool restore_index = false;
			if ((f & FastFmtFlags::SAME_AS_PREV) != 0) {
				ICED_ASSERT(has_prev_index);
				current_index = reader.index();
				restore_index = true;
				reader.set_index(prev_index);
			}
			else {
				prev_index = reader.index();
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
				reader.set_index(current_index);
		}
		ICED_DEBUG_ASSERT(!reader.can_read());
	}

	void init_memory_sizes() {
		for (std::size_t i = 0; i < memory_sizes.size(); i++) {
			const std::size_t mem_keywords = MEM_SIZE_TBL_DATA[i];
			ICED_ASSERT(mem_keywords < MEM_SIZE_TBL_STRINGS_COUNT);
			memory_sizes[i] = FastStringMemorySize{reinterpret_cast<const std::uint8_t*>(MEM_SIZE_TBL_STRINGS[mem_keywords])};
		}
	}
};

struct FastPseudoOps {
	std::array<std::vector<FastStringMnemonic>, pseudo_ops_defs::PSEUDO_OPS_KIND_COUNT> pseudo_ops;
	// std::deque never moves its elements
	std::deque<FastStringMnemonicData> strings;

	FastPseudoOps() {
		for (const auto& def : pseudo_ops_defs::PSEUDO_OPS_DEFS) {
			auto& result = pseudo_ops[static_cast<std::size_t>(def.kind)];
			result.reserve(def.size);
			for (std::size_t i = 0; i < def.size; i++) {
				const auto cc_s = def.cc[i];
				const std::size_t new_len = def.prefix.size() + cc_s.size() + def.suffix.size();
				ICED_ASSERT(new_len <= FastStringMnemonic::SIZE);
				auto& data = strings.emplace_back();
				data.fill(' ');
				data[0] = static_cast<std::uint8_t>(new_len);
				std::size_t index = 1;
				for (const char c : def.prefix)
					data[index++] = static_cast<std::uint8_t>(c);
				for (const char c : cc_s)
					data[index++] = static_cast<std::uint8_t>(c);
				for (const char c : def.suffix)
					data[index++] = static_cast<std::uint8_t>(c);
				result.push_back(FastStringMnemonic{data.data()});
			}
		}
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
