// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's encoder/tests/dec_enc.rs

#include "encoder/encoder_test_utils.hpp"
#include "generated/decoder_test_options.hpp"
#include "iced_x86/code_ext.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/decoder_error.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/encoder.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/op_code_info.hpp"
#include "iced_x86/register_ext.hpp"
#include "test_framework.hpp"
#include "test_utils/decoder_test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace iced_x86::tests {

namespace {

std::uint32_t get_memory_register_size(const Instruction& instruction) {
	for (const OpKind op_kind : instruction.op_kinds()) {
		switch (op_kind) {
		case OpKind::Register:
		case OpKind::NearBranch16:
		case OpKind::NearBranch32:
		case OpKind::NearBranch64:
		case OpKind::FarBranch16:
		case OpKind::FarBranch32:
		case OpKind::Immediate8:
		case OpKind::Immediate8_2nd:
		case OpKind::Immediate16:
		case OpKind::Immediate32:
		case OpKind::Immediate64:
		case OpKind::Immediate8to16:
		case OpKind::Immediate8to32:
		case OpKind::Immediate8to64:
		case OpKind::Immediate32to64:
			break;
		case OpKind::MemorySegSI:
		case OpKind::MemorySegDI:
		case OpKind::MemoryESDI:
			return 16;
		case OpKind::MemorySegESI:
		case OpKind::MemorySegEDI:
		case OpKind::MemoryESEDI:
			return 32;
		case OpKind::MemorySegRSI:
		case OpKind::MemorySegRDI:
		case OpKind::MemoryESRDI:
			return 64;
		case OpKind::Memory: {
			Register reg = instruction.memory_base();
			if (reg == Register::None)
				reg = instruction.memory_index();
			if (reg != Register::None)
				return static_cast<std::uint32_t>(register_ext::size(reg)) * 8;
			if (instruction.memory_displ_size() == 4)
				return 32;
			if (instruction.memory_displ_size() == 8)
				return 64;
			break;
		}
		}
	}
	return 0;
}

std::uint32_t reg_number(Register reg) { return static_cast<std::uint32_t>(register_ext::number(reg)); }

// Returns (index of first non-prefix byte, REX prefix or 0)
std::pair<std::size_t, std::uint32_t> skip_prefixes(const std::vector<std::uint8_t>& bytes, std::uint32_t bitness) {
	std::uint32_t rex = 0;
	for (std::size_t i = 0; i < bytes.size(); i++) {
		const std::uint8_t b = bytes[i];
		switch (b) {
		case 0x26:
		case 0x2E:
		case 0x36:
		case 0x3E:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0xF0:
		case 0xF2:
		case 0xF3:
			rex = 0;
			break;
		default:
			if (bitness == 64 && (b & 0xF0) == 0x40)
				rex = b;
			else
				return {i, rex};
			break;
		}
	}
	return {bytes.size(), rex};
}

std::size_t get_evex_index(const std::vector<std::uint8_t>& bytes) {
	const auto it = std::find(bytes.begin(), bytes.end(), static_cast<std::uint8_t>(0x62));
	if (it == bytes.end())
		throw std::runtime_error("Couldn't find EVEX byte");
	return static_cast<std::size_t>(it - bytes.begin());
}

std::size_t get_vex_xop_index(const std::vector<std::uint8_t>& bytes) {
	const auto it = std::find_if(bytes.begin(), bytes.end(), [](std::uint8_t b) { return b == 0xC4 || b == 0xC5 || b == 0x8F; });
	if (it == bytes.end())
		throw std::runtime_error("Couldn't find VEX/XOP byte");
	return static_cast<std::size_t>(it - bytes.begin());
}

Instruction decode_one(std::uint32_t bitness, const std::vector<std::uint8_t>& bytes, std::uint32_t options) {
	Decoder decoder(bitness, bytes, options);
	return decoder.decode();
}

// Returns the decoded instruction and the decoder's last error
std::pair<Instruction, DecoderError> decode_one_err(std::uint32_t bitness, const std::vector<std::uint8_t>& bytes, std::uint32_t options) {
	Decoder decoder(bitness, bytes, options);
	const Instruction instruction = decoder.decode();
	return {instruction, decoder.last_error()};
}

bool has_is4_or_is5_operands(const OpCodeInfo& op_code) {
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::xmm_is4:
		case OpCodeOperandKind::xmm_is5:
		case OpCodeOperandKind::ymm_is4:
		case OpCodeOperandKind::ymm_is5:
			return true;
		default:
			break;
		}
	}
	return false;
}

std::optional<Code> get_sae_er_instruction(const OpCodeInfo& op_code) {
	if (op_code.encoding() == EncodingKind::EVEX && !(op_code.can_suppress_all_exceptions() || op_code.can_use_rounding_control())) {
		const Mnemonic mnemonic = op_code.mnemonic();
		std::size_t j = 0;
		for (std::size_t i = static_cast<std::size_t>(op_code.code()) + 1; i < IcedConstants::CODE_ENUM_COUNT; i++, j++) {
			if (j > 1)
				break;
			const Code next_code = static_cast<Code>(i);
			if (code_ext::mnemonic(next_code) != mnemonic)
				break;
			const OpCodeInfo& next_op_code = code_ext::op_code(next_code);
			if (next_op_code.encoding() != op_code.encoding())
				break;
			if (next_op_code.can_suppress_all_exceptions() || next_op_code.can_use_rounding_control())
				return next_code;
		}
	}
	return std::nullopt;
}

} // namespace

TEST_CASE("encoder/verify_invalid_and_valid_lock_prefix") {
	const auto add_lock = [](const std::string& hex_bytes, bool has_lock) { return has_lock ? hex_bytes : "F0" + hex_bytes; };
	const auto has_modrm_memory_operand = [](const Instruction& instruction) {
		const std::uint32_t op_count = instruction.op_count();
		for (std::uint32_t i = 0; i < op_count; i++) {
			if (instruction.op_kind(i) == OpKind::Memory)
				return true;
		}
		return false;
	};

	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;

		bool has_lock;
		bool can_use_lock;

		{
			const auto bytes = to_vec_u8(info.hex_bytes());
			const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(instruction.code(), info.code());
			has_lock = instruction.has_lock_prefix();
			const OpCodeInfo& op_code = code_ext::op_code(info.code());
			can_use_lock = op_code.can_use_lock_prefix() && has_modrm_memory_operand(instruction);
			if (op_code.amd_lock_reg_bit())
				continue;
		}

		if (can_use_lock) {
			const auto bytes = to_vec_u8(add_lock(info.hex_bytes(), has_lock));
			const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(instruction.code(), info.code());
			CHECK(instruction.has_lock_prefix());
		}
		else {
			CHECK(!has_lock);
			{
				const auto bytes = to_vec_u8(add_lock(info.hex_bytes(), has_lock));
				const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), Code::INVALID);
				CHECK(error != DecoderError::None);
				CHECK(!instruction.has_lock_prefix());
			}
			{
				const auto bytes = to_vec_u8(add_lock(info.hex_bytes(), has_lock));
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
				CHECK_EQ(instruction.code(), info.code());
				CHECK(instruction.has_lock_prefix());
			}
		}
	}
}

TEST_CASE("encoder/verify_invalid_rex_mandatory_prefixes_vex_evex_xop_mvex") {
	const std::vector<std::string> prefixes1632 = {"66", "F3", "F2"};
	const std::vector<std::string> prefixes64 = {"66", "F3", "F2", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4A", "4B", "4C",
		"4D", "4E", "4F"};
	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;

		switch (code_ext::op_code(info.code()).encoding()) {
		case EncodingKind::Legacy:
		case EncodingKind::D3NOW:
			continue;
		case EncodingKind::VEX:
		case EncodingKind::EVEX:
		case EncodingKind::XOP:
		case EncodingKind::MVEX:
			break;
		}

		const std::vector<std::string>* prefixes;
		switch (info.bitness()) {
		case 16:
		case 32:
			prefixes = &prefixes1632;
			break;
		case 64:
			prefixes = &prefixes64;
			break;
		default:
			FAIL("unreachable");
		}
		for (const std::string& prefix : *prefixes) {
			Instruction orig_instr;
			{
				const auto bytes = to_vec_u8(info.hex_bytes());
				orig_instr = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(orig_instr.code(), info.code());
				// Mandatory prefix must be right before the opcode. If it has a seg override, there's also
				// a test without a seg override so just skip this.
				if (orig_instr.segment_prefix() != Register::None)
					continue;
				const std::uint32_t mem_reg_size = get_memory_register_size(orig_instr);
				// 67h prefix
				if (mem_reg_size != 0 && mem_reg_size != info.bitness())
					continue;
				const std::size_t non_prefix_index = skip_prefixes(bytes, info.bitness()).first;
				if (std::find(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(non_prefix_index), static_cast<std::uint8_t>(0x67)) !=
					bytes.begin() + static_cast<std::ptrdiff_t>(non_prefix_index))
					continue;
			}
			const std::string hex_bytes = prefix + info.hex_bytes();
			{
				const auto bytes = to_vec_u8(hex_bytes);
				Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
				CHECK_EQ(instruction.code(), info.code());

				const std::size_t len = instruction.len();
				instruction.set_len(len - 1);
				const std::uint64_t next_ip = instruction.next_ip();
				instruction.set_next_ip(next_ip - 1);
				if (prefix == "F3") {
					CHECK(instruction.has_rep_prefix());
					CHECK(instruction.has_repe_prefix());
					instruction.set_has_rep_prefix(false);
				}
				else if (prefix == "F2") {
					CHECK(instruction.has_repne_prefix());
					instruction.set_has_repne_prefix(false);
				}
				if (instruction.op1_kind() == OpKind::NearBranch64)
					instruction.set_near_branch64(instruction.near_branch64() - 1);
				CHECK(instruction.eq_all_bits(orig_instr));
			}
			{
				const auto bytes = to_vec_u8(hex_bytes);
				const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), Code::INVALID);
				CHECK(error != DecoderError::None);
			}
		}
	}
}

TEST_CASE("encoder/test_evex_reserved_bits") {
	for (const auto& info : decoder_tests(false, false)) {
		if (code_ext::op_code(info.code()).encoding() != EncodingKind::EVEX)
			continue;
		auto bytes = to_vec_u8(info.hex_bytes());
		const std::size_t evex_index = get_evex_index(bytes);
		for (std::uint32_t i = 1; i < 2; i++) {
			bytes[evex_index + 1] = static_cast<std::uint8_t>((bytes[evex_index + 1] & ~8U) | (i << 3));
			{
				const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), Code::INVALID);
				CHECK(error != DecoderError::None);
			}
			{
				const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options() ^ DecoderOptions::NO_INVALID_CHECK);
				CHECK_EQ(instruction.code(), Code::INVALID);
				CHECK(error != DecoderError::None);
			}
		}
	}
}

TEST_CASE("encoder/test_wig_instructions_ignore_w") {
	for (const auto& info : decoder_tests(false, false)) {
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		const EncodingKind encoding = op_code.encoding();
		const bool is_wig = op_code.is_wig() || (op_code.is_wig32() && info.bitness() != 64);
		if (encoding == EncodingKind::EVEX || encoding == EncodingKind::MVEX) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t evex_index = get_evex_index(bytes);

			if (is_wig) {
				const Instruction instruction1 = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction1.code(), info.code());
				bytes[evex_index + 2] ^= 0x80;
				const Instruction instruction2 = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction2.code(), info.code());
				CHECK(instruction1.eq_all_bits(instruction2));
			}
			else {
				{
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
				}
				{
					bytes[evex_index + 2] ^= 0x80;
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK(instruction.code() != info.code());
				}
			}
		}
		else if (encoding == EncodingKind::VEX || encoding == EncodingKind::XOP) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t vex_index = get_vex_xop_index(bytes);
			if (bytes[vex_index] == 0xC5)
				continue;

			if (is_wig) {
				const Instruction instruction1 = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction1.code(), info.code());
				bytes[vex_index + 2] ^= 0x80;
				const Instruction instruction2 = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction2.code(), info.code());
				CHECK(instruction1.eq_all_bits(instruction2));
			}
			else {
				{
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
				}
				{
					bytes[vex_index + 2] ^= 0x80;
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK(instruction.code() != info.code());
				}
			}
		}
		else if (encoding == EncodingKind::Legacy || encoding == EncodingKind::D3NOW)
			continue;
		else
			FAIL("unreachable");
	}
}

TEST_CASE("encoder/test_lig_instructions_ignore_l") {
	for (const auto& info : decoder_tests(false, false)) {
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		const EncodingKind encoding = op_code.encoding();
		if (encoding == EncodingKind::EVEX) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t evex_index = get_evex_index(bytes);

			const bool is_reg_only = (bytes[evex_index + 5] >> 6) == 3;
			const bool evex_b = (bytes[evex_index + 3] & 0x10) != 0;
			if (op_code.can_use_rounding_control() && is_reg_only && evex_b)
				continue;
			const bool is_sae = op_code.can_suppress_all_exceptions() && is_reg_only && evex_b;

			if (op_code.is_lig()) {
				const Instruction instruction1 = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction1.code(), info.code());
				const std::uint8_t orig_byte = bytes[evex_index + 3];
				for (std::uint32_t i = 1; i < 4; i++) {
					bytes[evex_index + 3] = static_cast<std::uint8_t>(orig_byte ^ (i << 5));
					const std::uint32_t ll = (bytes[evex_index + 3] >> 5) & 3;
					const bool invalid = (info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) == 0 && ll == 3 &&
						(bytes[evex_index + 5] < 0xC0 || (bytes[evex_index + 3] & 0x10) == 0);
					if (invalid) {
						const auto [instruction2, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
						CHECK_EQ(instruction2.code(), Code::INVALID);
						CHECK(error != DecoderError::None);

						const Instruction instruction3 = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
						CHECK_EQ(instruction3.code(), info.code());
						CHECK(instruction1.eq_all_bits(instruction3));
					}
					else {
						const Instruction instruction2 = decode_one(info.bitness(), bytes, info.decoder_options());
						CHECK_EQ(instruction2.code(), info.code());
						CHECK(instruction1.eq_all_bits(instruction2));
					}
				}
			}
			else {
				const Instruction instruction1 = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction1.code(), info.code());
				const std::uint8_t orig_byte = bytes[evex_index + 3];
				for (std::uint32_t i = 1; i < 4; i++) {
					bytes[evex_index + 3] = static_cast<std::uint8_t>(orig_byte ^ (i << 5));
					const Instruction instruction2 = decode_one(info.bitness(), bytes, info.decoder_options());
					if (is_sae) {
						CHECK_EQ(instruction2.code(), info.code());
						CHECK(instruction1.eq_all_bits(instruction2));
					}
					else
						CHECK(instruction2.code() != info.code());
				}
			}
		}
		else if (encoding == EncodingKind::VEX || encoding == EncodingKind::XOP) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t vex_index = get_vex_xop_index(bytes);
			const std::size_t l_index = bytes[vex_index] == 0xC5 ? vex_index + 1 : vex_index + 2;

			if (op_code.is_lig()) {
				const Instruction instruction1 = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction1.code(), info.code());
				bytes[l_index] ^= 4;
				const Instruction instruction2 = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction2.code(), info.code());
				CHECK(instruction1.eq_all_bits(instruction2));
			}
			else {
				{
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
				}
				{
					bytes[l_index] ^= 4;
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK(instruction.code() != info.code());
				}
			}
		}
		else if (encoding == EncodingKind::Legacy || encoding == EncodingKind::D3NOW || encoding == EncodingKind::MVEX)
			continue;
		else
			FAIL("unreachable");
	}
}

TEST_CASE("encoder/test_is4_is5_instructions_ignore_bit7_in_1632mode") {
	for (const auto& info : decoder_tests(false, false)) {
		if (info.bitness() != 16 && info.bitness() != 32)
			continue;
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		if (!has_is4_or_is5_operands(op_code))
			continue;
		auto bytes = to_vec_u8(info.hex_bytes());
		const Instruction instruction1 = decode_one(info.bitness(), bytes, info.decoder_options());
		bytes.back() ^= 0x80;
		const Instruction instruction2 = decode_one(info.bitness(), bytes, info.decoder_options());
		CHECK_EQ(instruction1.code(), info.code());
		CHECK(instruction1.eq_all_bits(instruction2));
	}
}

TEST_CASE("encoder/test_evex_k1_z_bits") {
	const std::vector<std::pair<bool, std::uint8_t>> p2_values_k1z = {{true, 0x00}, {true, 0x01}, {false, 0x80}, {true, 0x86}};
	const std::vector<std::pair<bool, std::uint8_t>> p2_values_k1 = {{true, 0x00}, {true, 0x01}, {false, 0x80}, {false, 0x86}};
	const std::vector<std::pair<bool, std::uint8_t>> p2_values_k1_fk = {{false, 0x00}, {true, 0x01}, {false, 0x80}, {false, 0x86}};
	const std::vector<std::pair<bool, std::uint8_t>> p2_values_nothing = {{true, 0x00}, {false, 0x01}, {false, 0x80}, {false, 0x86}};
	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;

		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		if (op_code.encoding() != EncodingKind::EVEX)
			continue;
		auto bytes = to_vec_u8(info.hex_bytes());
		const std::size_t evex_index = get_evex_index(bytes);
		const std::vector<std::pair<bool, std::uint8_t>>* p2_values;
		if (op_code.can_use_zeroing_masking()) {
			CHECK(op_code.can_use_op_mask_register());
			const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
			CHECK(instruction.code() != Code::INVALID);
			if (instruction.op0_kind() == OpKind::Memory)
				p2_values = &p2_values_k1;
			else
				p2_values = &p2_values_k1z;
		}
		else if (op_code.can_use_op_mask_register()) {
			if (op_code.require_op_mask_register())
				p2_values = &p2_values_k1_fk;
			else
				p2_values = &p2_values_k1;
		}
		else
			p2_values = &p2_values_nothing;

		const std::uint8_t b = bytes[evex_index + 3];
		for (const auto& p2v : *p2_values) {
			for (std::uint32_t i = 0; i < 2; i++) {
				bytes[evex_index + 3] = static_cast<std::uint8_t>((b & ~0x87U) | p2v.second);
				const std::uint32_t options = i == 0 ? info.decoder_options() : info.decoder_options() | DecoderOptions::NO_INVALID_CHECK;
				const auto [instruction, error] = decode_one_err(info.bitness(), bytes, options);
				if (p2v.first || (options & DecoderOptions::NO_INVALID_CHECK) != 0) {
					CHECK_EQ(instruction.code(), info.code());
					CHECK_EQ(instruction.zeroing_masking(), (p2v.second & 0x80) != 0);
					if ((p2v.second & 7) != 0) {
						const Register expected_reg = Register::K0 + static_cast<std::uint32_t>(p2v.second & 7);
						CHECK_EQ(instruction.op_mask(), expected_reg);
					}
					else
						CHECK_EQ(instruction.op_mask(), Register::None);
				}
				else {
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
				}
			}
		}
	}
}

TEST_CASE("encoder/test_evex_b_bit") {
	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;

		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		if (op_code.encoding() != EncodingKind::EVEX)
			continue;
		auto bytes = to_vec_u8(info.hex_bytes());
		const std::size_t evex_index = get_evex_index(bytes);

		const bool is_reg_only = (bytes[evex_index + 5] >> 6) == 3;
		const bool is_sae_or_er = is_reg_only && (op_code.can_use_rounding_control() || op_code.can_suppress_all_exceptions());
		const std::optional<Code> new_code = get_sae_er_instruction(op_code);

		if (op_code.can_broadcast() && !is_reg_only) {
			{
				bytes[evex_index + 3] &= 0xEF;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), info.code());
				CHECK(!instruction.is_broadcast());
			}
			{
				bytes[evex_index + 3] |= 0x10;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), info.code());
				CHECK(instruction.is_broadcast());
			}
		}
		else {
			if (!is_sae_or_er) {
				bytes[evex_index + 3] &= 0xEF;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), info.code());
				CHECK(!instruction.is_broadcast());
			}
			{
				bytes[evex_index + 3] |= 0x10;
				const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
				if (is_sae_or_er)
					CHECK_EQ(instruction.code(), info.code());
				else if (new_code && is_reg_only)
					CHECK_EQ(instruction.code(), *new_code);
				else {
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
				}
				CHECK(!instruction.is_broadcast());
			}
			{
				bytes[evex_index + 3] |= 0x10;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
				if (new_code && is_reg_only)
					CHECK_EQ(instruction.code(), *new_code);
				else
					CHECK_EQ(instruction.code(), info.code());
				CHECK(!instruction.is_broadcast());
			}
		}
	}
}

TEST_CASE("encoder/verify_tuple_type_bcst") {
	const auto names = code_names();
	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		if (is_ignored_code(names[i]))
			continue;
		const OpCodeInfo& op_code = code_ext::op_code(static_cast<Code>(i));
		bool expected_bcst;
		switch (op_code.tuple_type()) {
		case TupleType::N8b4:
		case TupleType::N16b4:
		case TupleType::N32b4:
		case TupleType::N64b4:
		case TupleType::N16b8:
		case TupleType::N32b8:
		case TupleType::N64b8:
		case TupleType::N4b2:
		case TupleType::N8b2:
		case TupleType::N16b2:
		case TupleType::N32b2:
		case TupleType::N64b2:
			expected_bcst = true;
			break;
		default:
			expected_bcst = false;
			break;
		}
		CHECK_EQ(op_code.can_broadcast(), expected_bcst);
	}
}

namespace {

struct VvvvvInfo {
	bool uses_vvvv;
	bool is_vsib;
	std::uint8_t vvvv_mask;
};

VvvvvInfo get_vvvvv_info(const OpCodeInfo& op_code) {
	bool uses_vvvv = false;
	bool is_vsib = false;
	std::uint8_t vvvv_mask;
	switch (op_code.encoding()) {
	case EncodingKind::EVEX:
	case EncodingKind::MVEX:
		vvvv_mask = 0x1F;
		break;
	case EncodingKind::VEX:
	case EncodingKind::XOP:
		vvvv_mask = 0xF;
		break;
	default:
		throw std::runtime_error("unreachable");
	}
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::mem_vsib32x:
		case OpCodeOperandKind::mem_vsib64x:
		case OpCodeOperandKind::mem_vsib32y:
		case OpCodeOperandKind::mem_vsib64y:
		case OpCodeOperandKind::mem_vsib32z:
		case OpCodeOperandKind::mem_vsib64z:
			is_vsib = true;
			break;
		case OpCodeOperandKind::k_vvvv:
		case OpCodeOperandKind::tmm_vvvv:
			uses_vvvv = true;
			vvvv_mask = 0x7;
			break;
		case OpCodeOperandKind::r32_vvvv:
		case OpCodeOperandKind::r64_vvvv:
		case OpCodeOperandKind::xmm_vvvv:
		case OpCodeOperandKind::xmmp3_vvvv:
		case OpCodeOperandKind::ymm_vvvv:
		case OpCodeOperandKind::zmm_vvvv:
		case OpCodeOperandKind::zmmp3_vvvv:
			uses_vvvv = true;
			break;
		default:
			break;
		}
	}
	return {uses_vvvv, is_vsib, vvvv_mask};
}

} // namespace

TEST_CASE("encoder/verify_invalid_vvvv") {
	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;

		const OpCodeInfo& op_code = code_ext::op_code(info.code());

		switch (op_code.encoding()) {
		case EncodingKind::Legacy:
		case EncodingKind::D3NOW:
			continue;
		case EncodingKind::VEX:
		case EncodingKind::EVEX:
		case EncodingKind::XOP:
		case EncodingKind::MVEX:
			break;
		}

		const auto [uses_vvvv, is_vsib, vvvv_mask] = get_vvvvv_info(op_code);

		if (op_code.encoding() == EncodingKind::VEX || op_code.encoding() == EncodingKind::XOP) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t vex_index = get_vex_xop_index(bytes);
			std::size_t b2i = vex_index + 1;
			if (bytes[vex_index] != 0xC5)
				b2i++;
			const std::uint8_t b2 = bytes[b2i];
			const Instruction orig_instr = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(orig_instr.code(), info.code());
			const bool is_vex2 = bytes[vex_index] == 0xC5;
			const std::uint32_t b2_mask = info.bitness() == 64 || !is_vex2 ? 0x78 : 0x38;
			if (uses_vvvv) {
				bytes[b2i] = static_cast<std::uint8_t>((b2 & ~b2_mask) | (b2_mask & ~(static_cast<std::uint32_t>(vvvv_mask) << 3)));
				{
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
				}
				if (info.bitness() != 64 && !is_vex2) {
					// vvvv[3] is ignored in 16/32-bit modes, clear it (it's inverted, so 'set' it)
					bytes[b2i] = static_cast<std::uint8_t>(b2 & ~0x40U);
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					CHECK(orig_instr.eq_all_bits(instruction));
				}
				if (info.bitness() == 64 && vvvv_mask != 0xF) {
					bytes[b2i] = static_cast<std::uint8_t>(b2 & ~b2_mask);
					{
						const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
						CHECK_EQ(instruction.code(), Code::INVALID);
						CHECK(error != DecoderError::None);
					}
					{
						const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
						CHECK_EQ(instruction.code(), info.code());
					}
				}
			}
			else {
				bytes[b2i] = static_cast<std::uint8_t>(b2 & ~b2_mask);
				{
					const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
				}
				{
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
					CHECK_EQ(instruction.code(), info.code());
					CHECK(orig_instr.eq_all_bits(instruction));
				}
			}
		}
		else if (op_code.encoding() == EncodingKind::EVEX || op_code.encoding() == EncodingKind::MVEX) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t evex_index = get_evex_index(bytes);
			const std::uint8_t b2 = bytes[evex_index + 2];
			const std::uint8_t b3 = bytes[evex_index + 3];
			const Instruction orig_instr = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(orig_instr.code(), info.code());

			bytes[evex_index + 2] = static_cast<std::uint8_t>(b2 & 0x87);
			if (!is_vsib)
				bytes[evex_index + 3] = static_cast<std::uint8_t>(b3 & 0xF7);
			{
				const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
				if (info.bitness() != 64) {
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
				}
				else if (uses_vvvv) {
					if (vvvv_mask != 0x1F)
						CHECK_EQ(instruction.code(), Code::INVALID);
					else
						CHECK_EQ(instruction.code(), info.code());
				}
				else {
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
					const Instruction instruction2 = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
					CHECK_EQ(instruction2.code(), info.code());
				}
			}
			if (!uses_vvvv && info.bitness() == 64) {
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
				CHECK_EQ(instruction.code(), info.code());
				CHECK(orig_instr.eq_all_bits(instruction));
			}

			// vvvv[3] isn't ignored in 16/32-bit mode if the operand doesn't use the vvvv bits
			bytes[evex_index + 2] = static_cast<std::uint8_t>(b2 & 0xBF);
			bytes[evex_index + 3] = b3;
			{
				const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
				if (uses_vvvv) {
					if (vvvv_mask != 0x1F)
						CHECK_EQ(instruction.code(), Code::INVALID);
					else
						CHECK_EQ(instruction.code(), info.code());
				}
				else {
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
					const Instruction instruction2 = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
					CHECK_EQ(instruction2.code(), info.code());
				}
			}
			if (!uses_vvvv) {
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
				CHECK_EQ(instruction.code(), info.code());
				CHECK(orig_instr.eq_all_bits(instruction));
			}

			// V' must be 1 in 16/32-bit modes
			bytes[evex_index + 2] = b2;
			bytes[evex_index + 3] = static_cast<std::uint8_t>(b3 & 0xF7);
			if (info.bitness() != 64) {
				const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), Code::INVALID);
				CHECK(error != DecoderError::None);
			}
		}
		else
			FAIL("unreachable");
	}
}

TEST_CASE("encoder/verify_gpr_rrxb_bits") {
	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;

		const OpCodeInfo& op_code = code_ext::op_code(info.code());

		switch (op_code.encoding()) {
		case EncodingKind::Legacy:
		case EncodingKind::D3NOW:
			continue;
		case EncodingKind::VEX:
		case EncodingKind::EVEX:
		case EncodingKind::XOP:
		case EncodingKind::MVEX:
			break;
		}

		bool uses_rm = false;
		bool uses_reg = false;
		bool other_rm = false;
		bool other_reg = false;
		bool mem_only = false;
		for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
			switch (op_kind) {
			case OpCodeOperandKind::mem:
			case OpCodeOperandKind::mem_mpx:
			case OpCodeOperandKind::mem_mib:
			case OpCodeOperandKind::mem_vsib32x:
			case OpCodeOperandKind::mem_vsib64x:
			case OpCodeOperandKind::mem_vsib32y:
			case OpCodeOperandKind::mem_vsib64y:
			case OpCodeOperandKind::mem_vsib32z:
			case OpCodeOperandKind::mem_vsib64z:
			case OpCodeOperandKind::sibmem:
				mem_only = true;
				break;
			case OpCodeOperandKind::r32_or_mem:
			case OpCodeOperandKind::r64_or_mem:
			case OpCodeOperandKind::r32_or_mem_mpx:
			case OpCodeOperandKind::r64_or_mem_mpx:
			case OpCodeOperandKind::r32_rm:
			case OpCodeOperandKind::r64_rm:
				uses_rm = true;
				break;
			case OpCodeOperandKind::r32_reg:
			case OpCodeOperandKind::r64_reg:
				uses_reg = true;
				break;
			case OpCodeOperandKind::k_or_mem:
			case OpCodeOperandKind::k_rm:
			case OpCodeOperandKind::xmm_or_mem:
			case OpCodeOperandKind::ymm_or_mem:
			case OpCodeOperandKind::zmm_or_mem:
			case OpCodeOperandKind::xmm_rm:
			case OpCodeOperandKind::ymm_rm:
			case OpCodeOperandKind::zmm_rm:
			case OpCodeOperandKind::tmm_rm:
				other_rm = true;
				break;
			case OpCodeOperandKind::k_reg:
			case OpCodeOperandKind::kp1_reg:
			case OpCodeOperandKind::xmm_reg:
			case OpCodeOperandKind::ymm_reg:
			case OpCodeOperandKind::zmm_reg:
			case OpCodeOperandKind::tmm_reg:
				other_reg = true;
				break;
			default:
				break;
			}
		}
		if (mem_only) {
			if (uses_reg)
				uses_rm = true;
			if (other_reg)
				other_rm = true;
		}
		if (!uses_rm && !uses_reg && op_code.op_count() > 0)
			continue;

		if (op_code.encoding() == EncodingKind::VEX || op_code.encoding() == EncodingKind::XOP) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t vex_index = get_vex_xop_index(bytes);
			const bool is_vex2 = bytes[vex_index] == 0xC5;
			const std::size_t mrmi = vex_index + 3 + (is_vex2 ? 0 : 1);
			const bool is_reg_only = mrmi >= bytes.size() || (bytes[mrmi] >> 6) == 3;
			const std::uint8_t b1 = bytes[vex_index + 1];

			const Instruction orig_instr = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(orig_instr.code(), info.code());
			if (uses_rm && !is_vex2) {
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x20);
				{
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					if (is_reg_only && info.bitness() != 64)
						CHECK(orig_instr.eq_all_bits(instruction));
				}
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x40);
				if (info.bitness() == 64) {
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					if (is_reg_only)
						CHECK(orig_instr.eq_all_bits(instruction));
				}
			}
			else if (!other_rm && !is_vex2) {
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x60);
				if (info.bitness() != 64)
					bytes[vex_index + 1] |= 0x40;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), info.code());
				CHECK(orig_instr.eq_all_bits(instruction));
			}
			if (uses_reg) {
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x80);
				if (info.bitness() == 64) {
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					if (is_reg_only)
						CHECK(!orig_instr.eq_all_bits(instruction));
				}
			}
			else if (!other_reg) {
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x80);
				if (info.bitness() != 64)
					bytes[vex_index + 1] |= 0x80;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), info.code());
				CHECK(orig_instr.eq_all_bits(instruction));
			}
		}
		else if (op_code.encoding() == EncodingKind::EVEX || op_code.encoding() == EncodingKind::MVEX) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t evex_index = get_evex_index(bytes);
			const bool is_reg_only = (bytes[evex_index + 5] >> 6) == 3;
			const std::uint8_t b1 = bytes[evex_index + 1];

			const Instruction orig_instr = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(orig_instr.code(), info.code());
			if (uses_rm) {
				bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x20);
				{
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					if (is_reg_only && info.bitness() != 64)
						CHECK(orig_instr.eq_all_bits(instruction));
				}
				bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x40);
				if (info.bitness() == 64) {
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					if (is_reg_only)
						CHECK(orig_instr.eq_all_bits(instruction));
				}
			}
			else if (!other_rm) {
				bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x60);
				if (info.bitness() != 64)
					bytes[evex_index + 1] |= 0x40;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), info.code());
				CHECK(orig_instr.eq_all_bits(instruction));
			}
			if (uses_reg) {
				if (info.bitness() == 64) {
					bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x10);
					{
						const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
						CHECK_EQ(instruction.code(), Code::INVALID);
						CHECK(error != DecoderError::None);
					}
					{
						const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
						CHECK_EQ(instruction.code(), info.code());
						CHECK(orig_instr.eq_all_bits(instruction));
					}
					bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x80);
					{
						const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
						CHECK_EQ(instruction.code(), info.code());
					}
				}
				else {
					bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x10);
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					CHECK(orig_instr.eq_all_bits(instruction));
				}
			}
		}
		else
			FAIL("unreachable");
	}
}

TEST_CASE("encoder/verify_k_reg_rrxb_bits") {
	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;

		const OpCodeInfo& op_code = code_ext::op_code(info.code());

		switch (op_code.encoding()) {
		case EncodingKind::Legacy:
		case EncodingKind::D3NOW:
			continue;
		case EncodingKind::VEX:
		case EncodingKind::EVEX:
		case EncodingKind::XOP:
		case EncodingKind::MVEX:
			break;
		}

		bool uses_rm = false;
		bool maybe_uses_rm = false;
		bool uses_reg = false;
		bool other_rm = false;
		bool other_reg = false;
		for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
			switch (op_kind) {
			case OpCodeOperandKind::mem:
				maybe_uses_rm = true;
				break;
			case OpCodeOperandKind::k_or_mem:
			case OpCodeOperandKind::k_rm:
				uses_rm = true;
				break;
			case OpCodeOperandKind::k_reg:
			case OpCodeOperandKind::kp1_reg:
				uses_reg = true;
				break;
			case OpCodeOperandKind::r32_or_mem:
			case OpCodeOperandKind::r64_or_mem:
			case OpCodeOperandKind::r32_or_mem_mpx:
			case OpCodeOperandKind::r64_or_mem_mpx:
			case OpCodeOperandKind::r32_rm:
			case OpCodeOperandKind::r64_rm:
			case OpCodeOperandKind::xmm_or_mem:
			case OpCodeOperandKind::ymm_or_mem:
			case OpCodeOperandKind::zmm_or_mem:
			case OpCodeOperandKind::xmm_rm:
			case OpCodeOperandKind::ymm_rm:
			case OpCodeOperandKind::zmm_rm:
			case OpCodeOperandKind::tmm_rm:
				other_rm = true;
				break;
			case OpCodeOperandKind::xmm_reg:
			case OpCodeOperandKind::ymm_reg:
			case OpCodeOperandKind::zmm_reg:
			case OpCodeOperandKind::tmm_reg:
			case OpCodeOperandKind::r32_reg:
			case OpCodeOperandKind::r64_reg:
				other_reg = true;
				break;
			default:
				break;
			}
		}
		if (uses_reg && maybe_uses_rm)
			uses_rm = true;
		if (!uses_rm && !uses_reg && op_code.op_count() > 0)
			continue;

		if (op_code.encoding() == EncodingKind::VEX || op_code.encoding() == EncodingKind::XOP) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t vex_index = get_vex_xop_index(bytes);
			const bool is_vex2 = bytes[vex_index] == 0xC5;
			const std::size_t mrmi = vex_index + 3 + (is_vex2 ? 0 : 1);
			const bool is_reg_only = mrmi >= bytes.size() || (bytes[mrmi] >> 6) == 3;
			const std::uint8_t b1 = bytes[vex_index + 1];

			const Instruction orig_instr = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(orig_instr.code(), info.code());
			if (uses_rm && !is_vex2) {
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x20);
				{
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					if (is_reg_only && info.bitness() != 64)
						CHECK(orig_instr.eq_all_bits(instruction));
				}
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x40);
				if (info.bitness() == 64) {
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					if (is_reg_only)
						CHECK(orig_instr.eq_all_bits(instruction));
				}
			}
			else if (!other_rm && !is_vex2) {
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x60);
				if (info.bitness() != 64)
					bytes[vex_index + 1] |= 0x40;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), info.code());
				CHECK(orig_instr.eq_all_bits(instruction));
			}
			if (uses_reg) {
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x80);
				if (info.bitness() == 64) {
					{
						const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
						CHECK_EQ(instruction.code(), Code::INVALID);
						CHECK(error != DecoderError::None);
					}
					{
						const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
						CHECK_EQ(instruction.code(), info.code());
						if (is_reg_only)
							CHECK(orig_instr.eq_all_bits(instruction));
					}
				}
			}
			else if (!other_reg) {
				bytes[vex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x80);
				if (info.bitness() != 64)
					bytes[vex_index + 1] |= 0x80;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), info.code());
				CHECK(orig_instr.eq_all_bits(instruction));
			}
		}
		else if (op_code.encoding() == EncodingKind::EVEX || op_code.encoding() == EncodingKind::MVEX) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t evex_index = get_evex_index(bytes);
			const bool is_reg_only = (bytes[evex_index + 5] >> 6) == 3;
			const std::uint8_t b1 = bytes[evex_index + 1];

			const Instruction orig_instr = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(orig_instr.code(), info.code());
			if (uses_rm) {
				bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x20);
				{
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					if (is_reg_only && info.bitness() != 64)
						CHECK(orig_instr.eq_all_bits(instruction));
				}
				bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x40);
				if (info.bitness() == 64) {
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					if (is_reg_only)
						CHECK(orig_instr.eq_all_bits(instruction));
				}
			}
			else if (!other_rm) {
				bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x60);
				if (info.bitness() != 64)
					bytes[evex_index + 1] |= 0x40;
				const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
				CHECK_EQ(instruction.code(), info.code());
				CHECK(orig_instr.eq_all_bits(instruction));
			}
			if (uses_reg) {
				if (info.bitness() == 64) {
					bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x10);
					{
						const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
						CHECK_EQ(instruction.code(), Code::INVALID);
						CHECK(error != DecoderError::None);
					}
					{
						const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
						CHECK_EQ(instruction.code(), info.code());
						CHECK(orig_instr.eq_all_bits(instruction));
					}
					bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x80);
					{
						const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
						CHECK_EQ(instruction.code(), Code::INVALID);
						CHECK(error != DecoderError::None);
					}
					{
						const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
						CHECK_EQ(instruction.code(), info.code());
						CHECK(orig_instr.eq_all_bits(instruction));
					}
				}
				else {
					bytes[evex_index + 1] = static_cast<std::uint8_t>(b1 ^ 0x10);
					const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), info.code());
					CHECK(orig_instr.eq_all_bits(instruction));
				}
			}
		}
		else
			FAIL("unreachable");
	}
}

namespace {

// All Vk_VSIB instructions, eg. EVEX_Vpgatherdd_xmm_k1_vm32x
bool can_have_invalid_index_register_evex(const OpCodeInfo& op_code) {
	if (op_code.encoding() != EncodingKind::EVEX && op_code.encoding() != EncodingKind::MVEX)
		return false;

	switch (op_code.op0_kind()) {
	case OpCodeOperandKind::xmm_reg:
	case OpCodeOperandKind::ymm_reg:
	case OpCodeOperandKind::zmm_reg:
		break;
	default:
		return false;
	}
	return op_code.requires_unique_reg_nums();
}

// All VX_VSIB_HX instructions, eg. VEX_Vpgatherdd_xmm_vm32x_xmm
bool can_have_invalid_index_mask_dest_register_vex(const OpCodeInfo& op_code) {
	if (op_code.encoding() != EncodingKind::VEX && op_code.encoding() != EncodingKind::XOP)
		return false;

	switch (op_code.op0_kind()) {
	case OpCodeOperandKind::xmm_reg:
	case OpCodeOperandKind::ymm_reg:
	case OpCodeOperandKind::zmm_reg:
		break;
	default:
		return false;
	}

	return op_code.requires_unique_reg_nums();
}

// Returns (is_vsib32, is_vsib64) if it's a VSIB instruction
std::optional<std::pair<bool, bool>> get_vsib(const OpCodeInfo& op_code) {
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::mem_vsib32x:
		case OpCodeOperandKind::mem_vsib32y:
		case OpCodeOperandKind::mem_vsib32z:
			return std::make_pair(true, false);
		case OpCodeOperandKind::mem_vsib64x:
		case OpCodeOperandKind::mem_vsib64y:
		case OpCodeOperandKind::mem_vsib64z:
			return std::make_pair(false, true);
		default:
			break;
		}
	}
	return std::nullopt;
}

bool is_vsib(const OpCodeInfo& op_code) { return get_vsib(op_code).has_value(); }

} // namespace

TEST_CASE("encoder/verify_vsib_with_invalid_index_register_evex") {
	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		if (!can_have_invalid_index_register_evex(op_code))
			continue;

		if (op_code.encoding() == EncodingKind::EVEX || op_code.encoding() == EncodingKind::MVEX) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t evex_index = get_evex_index(bytes);
			const std::uint8_t p0 = bytes[evex_index + 1];
			const std::uint8_t p2 = bytes[evex_index + 3];
			const std::uint8_t m = bytes[evex_index + 5];
			const std::uint8_t s = bytes[evex_index + 6];
			for (std::uint32_t i = 0; i < 32; i++) {
				const std::uint32_t reg_num = info.bitness() == 64 ? i : i & 7;
				const bool always_invalid = info.bitness() != 64 && (i & 0x10) != 0;
				const std::uint32_t t = i ^ 0x1F;
				// reg  = R' R modrm.reg
				// vidx = V' X sib.index
				bytes[evex_index + 1] = static_cast<std::uint8_t>((p0 & ~0xD0U) | /*R'*/ (t & 0x10) | /*R*/ ((t & 0x08) << 4) | /*X*/ ((t & 0x08) << 3));
				if (info.bitness() != 64)
					bytes[evex_index + 1] |= 0xC0;
				bytes[evex_index + 3] = static_cast<std::uint8_t>((p2 & ~0x08U) | /*V'*/ ((t & 0x10) >> 1));
				bytes[evex_index + 5] = static_cast<std::uint8_t>((m & 0xC7) | /*modrm.reg*/ ((i & 7) << 3));
				bytes[evex_index + 6] = static_cast<std::uint8_t>((s & 0xC7) | /*sib.index*/ ((i & 7) << 3));

				{
					const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
				}
				{
					const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
					if (always_invalid) {
						CHECK_EQ(instruction.code(), Code::INVALID);
						CHECK(error != DecoderError::None);
					}
					else {
						CHECK_EQ(instruction.code(), info.code());
						CHECK_EQ(instruction.op0_kind(), OpKind::Register);
						CHECK_EQ(instruction.op1_kind(), OpKind::Memory);
						CHECK(instruction.memory_index() != Register::None);
						CHECK_EQ(reg_number(instruction.op0_register()), reg_num);
						CHECK_EQ(reg_number(instruction.memory_index()), reg_num);
					}
				}
			}
		}
		else
			FAIL("unreachable");
	}
}

TEST_CASE("encoder/verify_vsib_with_invalid_index_mask_dest_register_vex") {
	enum class TestKind {
		reg_eq_vvvv,
		reg_eq_vidx,
		vvvv_eq_vidx,
		all_eq_all,
	};
	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		if (!can_have_invalid_index_mask_dest_register_vex(op_code))
			continue;

		if (op_code.encoding() == EncodingKind::VEX || op_code.encoding() == EncodingKind::XOP) {
			auto bytes = to_vec_u8(info.hex_bytes());
			const std::size_t vex_index = get_vex_xop_index(bytes);

			const bool is_vex2 = bytes[vex_index] == 0xC5;
			const std::size_t r_index = vex_index + 1;
			const std::size_t v_index = is_vex2 ? r_index : r_index + 1;
			const std::size_t m_index = v_index + 2;
			const std::size_t s_index = v_index + 3;

			const std::uint8_t r = bytes[r_index];
			const std::uint8_t v = bytes[v_index];
			const std::uint8_t m = bytes[m_index];
			const std::uint8_t s = bytes[s_index];

			for (const TestKind test_kind : {TestKind::reg_eq_vvvv, TestKind::reg_eq_vidx, TestKind::vvvv_eq_vidx, TestKind::all_eq_all}) {
				for (std::uint32_t i = 0; i < 16; i++) {
					const std::uint32_t reg_num = info.bitness() == 64 ? i : i & 7;
					// Use a small number (0-7) in case it's vex2 and 'other' is vidx (uses VEX.X bit)
					const std::uint32_t other = reg_num == 0 ? 1 : 0;
					std::uint32_t new_reg;
					std::uint32_t new_vvvv;
					std::uint32_t new_vidx;

					switch (test_kind) {
					case TestKind::reg_eq_vvvv:
						new_vvvv = reg_num;
						new_reg = reg_num;
						new_vidx = other;
						break;
					case TestKind::reg_eq_vidx:
						new_vidx = reg_num;
						new_reg = reg_num;
						new_vvvv = other;
						break;
					case TestKind::vvvv_eq_vidx:
						new_vidx = reg_num;
						new_vvvv = reg_num;
						new_reg = other;
						break;
					case TestKind::all_eq_all:
					default:
						new_vidx = reg_num;
						new_vvvv = reg_num;
						new_reg = reg_num;
						break;
					}

					// reg  = R modrm.reg
					// vidx = X sib.index
					if (is_vex2) {
						if (new_vidx >= 8)
							continue;
						bytes[r_index] = static_cast<std::uint8_t>((r & 0x07) | /*R*/ (((new_reg ^ 8) & 0x8) << 4) | /*vvvv*/ (((new_vvvv ^ 0xF) & 0xF) << 3));
					}
					else {
						bytes[r_index] = static_cast<std::uint8_t>((r & 0x3F) | /*R*/ (((new_reg ^ 8) & 8) << 4) | /*X*/ (((new_vidx ^ 8) & 8) << 3));
						bytes[v_index] = static_cast<std::uint8_t>((v & 0x87) | /*vvvv*/ (((new_vvvv ^ 0xF) & 0xF) << 3));
					}
					bytes[m_index] = static_cast<std::uint8_t>((m & 0xC7) | /*modrm.reg*/ ((new_reg & 7) << 3));
					bytes[s_index] = static_cast<std::uint8_t>((s & 0xC7) | /*sib.index*/ ((new_vidx & 7) << 3));

					{
						const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
						CHECK_EQ(instruction.code(), Code::INVALID);
						CHECK(error != DecoderError::None);
					}
					{
						const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
						CHECK_EQ(instruction.code(), info.code());
						CHECK_EQ(instruction.op0_kind(), OpKind::Register);
						CHECK_EQ(instruction.op1_kind(), OpKind::Memory);
						CHECK_EQ(instruction.op2_kind(), OpKind::Register);
						CHECK(instruction.memory_index() != Register::None);
						CHECK_EQ(reg_number(instruction.op0_register()), new_reg);
						CHECK_EQ(reg_number(instruction.memory_index()), new_vidx);
						CHECK_EQ(reg_number(instruction.op2_register()), new_vvvv);
					}
				}
			}
		}
		else
			FAIL("unreachable");
	}
}

TEST_CASE("encoder/test_vsib_props") {
	for (const auto& info : decoder_tests(false, false)) {
		const auto bytes = to_vec_u8(info.hex_bytes());
		const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
		CHECK_EQ(instruction.code(), info.code());

		const auto vsib = get_vsib(code_ext::op_code(info.code()));
		const bool is_vsib_ = vsib.has_value();
		const bool is_vsib32 = vsib ? vsib->first : false;
		const bool is_vsib64 = vsib ? vsib->second : false;
		CHECK_EQ(is_vsib_, instruction.is_vsib());
		CHECK_EQ(is_vsib32, instruction.is_vsib32());
		CHECK_EQ(is_vsib64, instruction.is_vsib64());
	}
}

namespace {

struct TestedInfo {
	// VEX/XOP.L and EVEX.L'L values
	std::uint32_t l_bits = 0; // bit 0 = L0/L128, bit 1 = L1/L256, etc
	std::uint32_t vex2_l_bits = 0;

	// REX/VEX/XOP/EVEX/MVEX: W values
	std::uint32_t w_bits = 0; // bit 0 = W0, bit 1 = W1

	// REX/VEX/XOP/EVEX/MVEX.R
	std::uint32_t r_bits = 0;
	std::uint32_t vex2_r_bits = 0;
	// REX/VEX/XOP/EVEX/MVEX.X
	std::uint32_t x_bits = 0;
	// REX/VEX/XOP/EVEX/MVEX.B
	std::uint32_t b_bits = 0;
	// EVEX/MVEX.R'
	std::uint32_t r2_bits = 0;
	// EVEX/MVEX.V'
	std::uint32_t v2_bits = 0;

	// mod=11
	bool reg_reg = false;
	// mod=00,01,10
	bool reg_mem = false;

	// EVEX/MVEX only
	bool mem_disp8 = false;

	// Tested vex2 prefix
	bool vex2 = false;
	// Tested vex3 prefix
	bool vex3 = false;

	// EVEX/MVEX: tested opmask
	bool op_mask = false;
	// EVEX/MVEX: tested no opmask
	bool no_op_mask = false;

	bool prefix_xacquire = false;
	bool prefix_no_xacquire = false;
	bool prefix_xrelease = false;
	bool prefix_no_xrelease = false;
	bool prefix_lock = false;
	bool prefix_no_lock = false;
	bool prefix_hnt = false;
	bool prefix_no_hnt = false;
	bool prefix_ht = false;
	bool prefix_no_ht = false;
	bool prefix_rep = false;
	bool prefix_no_rep = false;
	bool prefix_repne = false;
	bool prefix_no_repne = false;
	bool prefix_notrack = false;
	bool prefix_no_notrack = false;
	bool prefix_bnd = false;
	bool prefix_no_bnd = false;
};

bool can_use_modrm_rm_reg(const OpCodeInfo& op_code) {
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::r8_or_mem:
		case OpCodeOperandKind::r16_or_mem:
		case OpCodeOperandKind::r32_or_mem:
		case OpCodeOperandKind::r32_or_mem_mpx:
		case OpCodeOperandKind::r64_or_mem:
		case OpCodeOperandKind::r64_or_mem_mpx:
		case OpCodeOperandKind::mm_or_mem:
		case OpCodeOperandKind::xmm_or_mem:
		case OpCodeOperandKind::ymm_or_mem:
		case OpCodeOperandKind::zmm_or_mem:
		case OpCodeOperandKind::bnd_or_mem_mpx:
		case OpCodeOperandKind::k_or_mem:
		case OpCodeOperandKind::r16_rm:
		case OpCodeOperandKind::r32_rm:
		case OpCodeOperandKind::r64_rm:
		case OpCodeOperandKind::k_rm:
		case OpCodeOperandKind::mm_rm:
		case OpCodeOperandKind::xmm_rm:
		case OpCodeOperandKind::ymm_rm:
		case OpCodeOperandKind::zmm_rm:
		case OpCodeOperandKind::tmm_rm:
			return true;
		default:
			break;
		}
	}
	return false;
}

bool can_use_modrm_rm_mem(const OpCodeInfo& op_code) {
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::mem:
		case OpCodeOperandKind::sibmem:
		case OpCodeOperandKind::mem_mpx:
		case OpCodeOperandKind::mem_mib:
		case OpCodeOperandKind::mem_vsib32x:
		case OpCodeOperandKind::mem_vsib64x:
		case OpCodeOperandKind::mem_vsib32y:
		case OpCodeOperandKind::mem_vsib64y:
		case OpCodeOperandKind::mem_vsib32z:
		case OpCodeOperandKind::mem_vsib64z:
		case OpCodeOperandKind::r8_or_mem:
		case OpCodeOperandKind::r16_or_mem:
		case OpCodeOperandKind::r32_or_mem:
		case OpCodeOperandKind::r32_or_mem_mpx:
		case OpCodeOperandKind::r64_or_mem:
		case OpCodeOperandKind::r64_or_mem_mpx:
		case OpCodeOperandKind::mm_or_mem:
		case OpCodeOperandKind::xmm_or_mem:
		case OpCodeOperandKind::ymm_or_mem:
		case OpCodeOperandKind::zmm_or_mem:
		case OpCodeOperandKind::bnd_or_mem_mpx:
		case OpCodeOperandKind::k_or_mem:
			return true;
		default:
			break;
		}
	}
	return false;
}

bool can_use_vex2(const OpCodeInfo& op_code) { return op_code.table() == OpCodeTableKind::T0F && op_code.w() == 0; }

bool can_use_b(std::uint32_t bitness, const OpCodeInfo& op_code) {
	switch (op_code.code()) {
	case Code::Nopw:
	case Code::Nopd:
	case Code::Nopq:
	case Code::Bndmov_bnd_bndm128:
	case Code::Bndmov_bndm128_bnd:
		return false;
	default:
		break;
	}

	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::mem:
		case OpCodeOperandKind::sibmem:
		case OpCodeOperandKind::mem_mpx:
		case OpCodeOperandKind::mem_mib:
		case OpCodeOperandKind::mem_vsib32x:
		case OpCodeOperandKind::mem_vsib32y:
		case OpCodeOperandKind::mem_vsib32z:
		case OpCodeOperandKind::mem_vsib64x:
		case OpCodeOperandKind::mem_vsib64y:
		case OpCodeOperandKind::mem_vsib64z:
			// The memory test tests all combinations
			return false;
		default:
			break;
		}
	}
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::tmm_rm:
			return false;
		case OpCodeOperandKind::k_rm:
		case OpCodeOperandKind::mm_rm:
		case OpCodeOperandKind::r16_rm:
		case OpCodeOperandKind::r32_rm:
		case OpCodeOperandKind::r64_rm:
		case OpCodeOperandKind::xmm_rm:
		case OpCodeOperandKind::ymm_rm:
		case OpCodeOperandKind::zmm_rm:
		case OpCodeOperandKind::bnd_or_mem_mpx:
		case OpCodeOperandKind::k_or_mem:
		case OpCodeOperandKind::mm_or_mem:
		case OpCodeOperandKind::r16_or_mem:
		case OpCodeOperandKind::r32_or_mem:
		case OpCodeOperandKind::r32_or_mem_mpx:
		case OpCodeOperandKind::r64_or_mem:
		case OpCodeOperandKind::r64_or_mem_mpx:
		case OpCodeOperandKind::r8_or_mem:
		case OpCodeOperandKind::xmm_or_mem:
		case OpCodeOperandKind::ymm_or_mem:
		case OpCodeOperandKind::zmm_or_mem:
			if (op_code.encoding() == EncodingKind::Legacy || op_code.encoding() == EncodingKind::D3NOW)
				return bitness == 64;
			return true;
		default:
			break;
		}
	}
	if (op_code.encoding() == EncodingKind::Legacy || op_code.encoding() == EncodingKind::D3NOW)
		return bitness == 64;
	return true;
}

bool can_use_x(const OpCodeInfo& op_code) {
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::k_rm:
		case OpCodeOperandKind::mm_rm:
		case OpCodeOperandKind::r16_rm:
		case OpCodeOperandKind::r32_rm:
		case OpCodeOperandKind::r64_rm:
		case OpCodeOperandKind::xmm_rm:
		case OpCodeOperandKind::ymm_rm:
		case OpCodeOperandKind::zmm_rm:
		case OpCodeOperandKind::tmm_rm:
		case OpCodeOperandKind::bnd_or_mem_mpx:
		case OpCodeOperandKind::k_or_mem:
		case OpCodeOperandKind::mm_or_mem:
		case OpCodeOperandKind::r16_or_mem:
		case OpCodeOperandKind::r32_or_mem:
		case OpCodeOperandKind::r32_or_mem_mpx:
		case OpCodeOperandKind::r64_or_mem:
		case OpCodeOperandKind::r64_or_mem_mpx:
		case OpCodeOperandKind::r8_or_mem:
		case OpCodeOperandKind::xmm_or_mem:
		case OpCodeOperandKind::ymm_or_mem:
		case OpCodeOperandKind::zmm_or_mem:
			return true;
		case OpCodeOperandKind::mem:
		case OpCodeOperandKind::sibmem:
		case OpCodeOperandKind::mem_mpx:
		case OpCodeOperandKind::mem_mib:
		case OpCodeOperandKind::mem_vsib32x:
		case OpCodeOperandKind::mem_vsib32y:
		case OpCodeOperandKind::mem_vsib32z:
		case OpCodeOperandKind::mem_vsib64x:
		case OpCodeOperandKind::mem_vsib64y:
		case OpCodeOperandKind::mem_vsib64z:
			// The memory test tests all combinations
			return false;
		default:
			break;
		}
	}
	return true;
}

bool can_use_r(const OpCodeInfo& op_code) {
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::k_reg:
		case OpCodeOperandKind::kp1_reg:
		case OpCodeOperandKind::tr_reg:
		case OpCodeOperandKind::bnd_reg:
		case OpCodeOperandKind::tmm_reg:
			return false;
		case OpCodeOperandKind::cr_reg:
		case OpCodeOperandKind::dr_reg:
		case OpCodeOperandKind::mm_reg:
		case OpCodeOperandKind::r16_reg:
		case OpCodeOperandKind::r32_reg:
		case OpCodeOperandKind::r64_reg:
		case OpCodeOperandKind::r8_reg:
		case OpCodeOperandKind::seg_reg:
		case OpCodeOperandKind::xmm_reg:
		case OpCodeOperandKind::ymm_reg:
		case OpCodeOperandKind::zmm_reg:
			return true;
		default:
			break;
		}
	}
	return true;
}

bool can_use_r2(const OpCodeInfo& op_code) {
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::k_reg:
		case OpCodeOperandKind::kp1_reg:
		case OpCodeOperandKind::tr_reg:
		case OpCodeOperandKind::bnd_reg:
		case OpCodeOperandKind::cr_reg:
		case OpCodeOperandKind::dr_reg:
		case OpCodeOperandKind::mm_reg:
		case OpCodeOperandKind::r16_reg:
		case OpCodeOperandKind::r32_reg:
		case OpCodeOperandKind::r64_reg:
		case OpCodeOperandKind::r8_reg:
		case OpCodeOperandKind::seg_reg:
		case OpCodeOperandKind::tmm_reg:
			return false;
		case OpCodeOperandKind::xmm_reg:
		case OpCodeOperandKind::ymm_reg:
		case OpCodeOperandKind::zmm_reg:
			return true;
		default:
			break;
		}
	}
	return true;
}

bool can_use_v2(const OpCodeInfo& op_code) {
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::k_vvvv:
		case OpCodeOperandKind::r32_vvvv:
		case OpCodeOperandKind::r64_vvvv:
		case OpCodeOperandKind::tmm_vvvv:
			return false;
		case OpCodeOperandKind::xmm_vvvv:
		case OpCodeOperandKind::xmmp3_vvvv:
		case OpCodeOperandKind::ymm_vvvv:
		case OpCodeOperandKind::zmm_vvvv:
		case OpCodeOperandKind::zmmp3_vvvv:
			return true;
		case OpCodeOperandKind::mem_vsib32x:
		case OpCodeOperandKind::mem_vsib32y:
		case OpCodeOperandKind::mem_vsib32z:
		case OpCodeOperandKind::mem_vsib64x:
		case OpCodeOperandKind::mem_vsib64y:
		case OpCodeOperandKind::mem_vsib64z:
			// The memory test tests all combinations
			return false;
		default:
			break;
		}
	}
	return false;
}

bool has_modrm(const OpCodeInfo& op_code) {
	for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
		switch (op_kind) {
		case OpCodeOperandKind::mem:
		case OpCodeOperandKind::sibmem:
		case OpCodeOperandKind::mem_mpx:
		case OpCodeOperandKind::mem_mib:
		case OpCodeOperandKind::mem_vsib32x:
		case OpCodeOperandKind::mem_vsib64x:
		case OpCodeOperandKind::mem_vsib32y:
		case OpCodeOperandKind::mem_vsib64y:
		case OpCodeOperandKind::mem_vsib32z:
		case OpCodeOperandKind::mem_vsib64z:
		case OpCodeOperandKind::r8_or_mem:
		case OpCodeOperandKind::r16_or_mem:
		case OpCodeOperandKind::r32_or_mem:
		case OpCodeOperandKind::r32_or_mem_mpx:
		case OpCodeOperandKind::r64_or_mem:
		case OpCodeOperandKind::r64_or_mem_mpx:
		case OpCodeOperandKind::mm_or_mem:
		case OpCodeOperandKind::xmm_or_mem:
		case OpCodeOperandKind::ymm_or_mem:
		case OpCodeOperandKind::zmm_or_mem:
		case OpCodeOperandKind::bnd_or_mem_mpx:
		case OpCodeOperandKind::k_or_mem:
		case OpCodeOperandKind::r8_reg:
		case OpCodeOperandKind::r16_reg:
		case OpCodeOperandKind::r16_rm:
		case OpCodeOperandKind::r32_reg:
		case OpCodeOperandKind::r32_rm:
		case OpCodeOperandKind::r64_reg:
		case OpCodeOperandKind::r64_rm:
		case OpCodeOperandKind::seg_reg:
		case OpCodeOperandKind::k_reg:
		case OpCodeOperandKind::kp1_reg:
		case OpCodeOperandKind::k_rm:
		case OpCodeOperandKind::mm_reg:
		case OpCodeOperandKind::mm_rm:
		case OpCodeOperandKind::xmm_reg:
		case OpCodeOperandKind::xmm_rm:
		case OpCodeOperandKind::ymm_reg:
		case OpCodeOperandKind::ymm_rm:
		case OpCodeOperandKind::zmm_reg:
		case OpCodeOperandKind::zmm_rm:
		case OpCodeOperandKind::tmm_reg:
		case OpCodeOperandKind::tmm_rm:
		case OpCodeOperandKind::cr_reg:
		case OpCodeOperandKind::dr_reg:
		case OpCodeOperandKind::tr_reg:
		case OpCodeOperandKind::bnd_reg:
			return true;
		default:
			break;
		}
	}
	return false;
}

} // namespace

TEST_CASE("encoder/verify_that_test_cases_test_enough_bits") {
	std::vector<TestedInfo> tested_infos_16(IcedConstants::CODE_ENUM_COUNT);
	std::vector<TestedInfo> tested_infos_32(IcedConstants::CODE_ENUM_COUNT);
	std::vector<TestedInfo> tested_infos_64(IcedConstants::CODE_ENUM_COUNT);

	std::vector<bool> can_use_w(IcedConstants::CODE_ENUM_COUNT, false);
	{
		std::unordered_set<std::uint64_t> uses_w;
		const auto key = [](const OpCodeInfo& op_code) {
			return (static_cast<std::uint64_t>(op_code.table()) << 32) | op_code.op_code();
		};
		for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
			const OpCodeInfo& op_code = code_ext::op_code(static_cast<Code>(i));
			if (op_code.encoding() != EncodingKind::Legacy)
				continue;
			if (op_code.operand_size() != 0)
				uses_w.insert(key(op_code));
		}
		for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
			const OpCodeInfo& op_code = code_ext::op_code(static_cast<Code>(i));
			switch (op_code.encoding()) {
			case EncodingKind::Legacy:
			case EncodingKind::D3NOW:
				can_use_w[i] = uses_w.count(key(op_code)) == 0;
				break;
			case EncodingKind::VEX:
			case EncodingKind::EVEX:
			case EncodingKind::XOP:
			case EncodingKind::MVEX:
				break;
			}
		}
	}

	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;
		std::vector<TestedInfo>* tested_infos;
		switch (info.bitness()) {
		case 16:
			tested_infos = &tested_infos_16;
			break;
		case 32:
			tested_infos = &tested_infos_32;
			break;
		case 64:
			tested_infos = &tested_infos_64;
			break;
		default:
			FAIL("unreachable");
		}

		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		TestedInfo& tested = (*tested_infos)[static_cast<std::size_t>(info.code())];

		const auto bytes = to_vec_u8(info.hex_bytes());
		const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
		CHECK_EQ(instruction.code(), info.code());

		if (op_code.encoding() == EncodingKind::EVEX || op_code.encoding() == EncodingKind::MVEX) {
			const std::size_t evex_index = get_evex_index(bytes);

			if (op_code.encoding() == EncodingKind::EVEX) {
				if (instruction.rounding_control() == RoundingControl::None)
					tested.l_bits |= 1U << ((bytes[evex_index + 3] >> 5) & 3);

				const std::uint32_t ll = (bytes[evex_index + 3] >> 5) & 3;
				const bool invalid = (info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) == 0 && ll == 3 &&
					(bytes[evex_index + 5] < 0xC0 || (bytes[evex_index + 3] & 0x10) == 0);
				if (!invalid)
					tested.l_bits |= 1U << 3;
			}

			tested.w_bits |= 1U << (bytes[evex_index + 2] >> 7);
			tested.r_bits |= 1U << ((bytes[evex_index + 1] >> 7) ^ 1);
			tested.x_bits |= 1U << (((bytes[evex_index + 1] >> 6) & 1) ^ 1);
			tested.b_bits |= 1U << (((bytes[evex_index + 1] >> 5) & 1) ^ 1);
			tested.r2_bits |= 1U << (((bytes[evex_index + 1] >> 4) & 1) ^ 1);
			tested.v2_bits |= 1U << (((bytes[evex_index + 3] >> 3) & 1) ^ 1);
			if ((bytes[evex_index + 5] >> 6) != 3) {
				tested.reg_mem = true;
				if (instruction.memory_displ_size() == 1 && instruction.memory_displacement64() != 0)
					tested.mem_disp8 = true;
			}
			else
				tested.reg_reg = true;
			if (instruction.op_mask() != Register::None)
				tested.op_mask = true;
			else
				tested.no_op_mask = true;
		}
		else if (op_code.encoding() == EncodingKind::VEX || op_code.encoding() == EncodingKind::XOP) {
			const std::size_t vex_index = get_vex_xop_index(bytes);
			std::size_t mrmi;
			if (bytes[vex_index] == 0xC5) {
				mrmi = vex_index + 3;
				tested.vex2 = true;
				tested.vex2_r_bits |= 1U << ((bytes[vex_index + 1] >> 7) ^ 1);
				tested.vex2_l_bits |= 1U << ((bytes[vex_index + 1] >> 2) & 1);
			}
			else {
				mrmi = vex_index + 4;
				if (op_code.encoding() == EncodingKind::VEX)
					tested.vex3 = true;
				tested.r_bits |= 1U << ((bytes[vex_index + 1] >> 7) ^ 1);
				tested.x_bits |= 1U << (((bytes[vex_index + 1] >> 6) & 1) ^ 1);
				tested.b_bits |= 1U << (((bytes[vex_index + 1] >> 5) & 1) ^ 1);
				tested.w_bits |= 1U << (bytes[vex_index + 2] >> 7);
				tested.l_bits |= 1U << ((bytes[vex_index + 2] >> 2) & 1);
			}
			if (has_modrm(op_code)) {
				if ((bytes[mrmi] >> 6) != 3)
					tested.reg_mem = true;
				else
					tested.reg_reg = true;
			}
		}
		else if (op_code.encoding() == EncodingKind::Legacy || op_code.encoding() == EncodingKind::D3NOW) {
			auto [i, rex] = skip_prefixes(bytes, info.bitness());
			if (info.bitness() == 64) {
				tested.w_bits |= 1U << ((rex >> 3) & 1);
				tested.r_bits |= 1U << ((rex >> 2) & 1);
				tested.x_bits |= 1U << ((rex >> 1) & 1);
				tested.b_bits |= 1U << (rex & 1);
				// Can't access regs dr8-dr15
				if (info.code() == Code::Mov_r64_dr || info.code() == Code::Mov_dr_r64)
					tested.r_bits |= 1U << 1;
			}
			else {
				tested.w_bits |= 1;
				tested.r_bits |= 1;
				tested.x_bits |= 1;
				tested.b_bits |= 1;
			}
			if (has_modrm(op_code)) {
				switch (op_code.table()) {
				case OpCodeTableKind::Normal:
					break;
				case OpCodeTableKind::T0F:
					REQUIRE(bytes[i] == 0x0F);
					i++;
					break;
				case OpCodeTableKind::T0F38:
					REQUIRE(bytes[i] == 0x0F);
					i++;
					REQUIRE(bytes[i] == 0x38);
					i++;
					break;
				case OpCodeTableKind::T0F3A:
					REQUIRE(bytes[i] == 0x0F);
					i++;
					REQUIRE(bytes[i] == 0x3A);
					i++;
					break;
				default:
					break;
				}
				i++;
				if ((bytes[i] >> 6) != 3)
					tested.reg_mem = true;
				else
					tested.reg_reg = true;
			}
			if (op_code.can_use_xacquire_prefix()) {
				if (instruction.has_xacquire_prefix())
					tested.prefix_xacquire = true;
				else
					tested.prefix_no_xacquire = true;
			}
			if (op_code.can_use_xrelease_prefix()) {
				if (instruction.has_xrelease_prefix())
					tested.prefix_xrelease = true;
				else
					tested.prefix_no_xrelease = true;
			}
			if (op_code.can_use_lock_prefix()) {
				if (instruction.has_lock_prefix())
					tested.prefix_lock = true;
				else
					tested.prefix_no_lock = true;
			}
			if (op_code.can_use_hint_taken_prefix()) {
				if (instruction.segment_prefix() == Register::CS)
					tested.prefix_hnt = true;
				else
					tested.prefix_no_hnt = true;
			}
			if (op_code.can_use_hint_taken_prefix()) {
				if (instruction.segment_prefix() == Register::DS)
					tested.prefix_ht = true;
				else
					tested.prefix_no_ht = true;
			}
			if (op_code.can_use_rep_prefix()) {
				if (instruction.has_rep_prefix())
					tested.prefix_rep = true;
				else
					tested.prefix_no_rep = true;
			}
			if (op_code.can_use_repne_prefix()) {
				if (instruction.has_repne_prefix())
					tested.prefix_repne = true;
				else
					tested.prefix_no_repne = true;
			}
			if (op_code.can_use_notrack_prefix()) {
				if (instruction.segment_prefix() == Register::DS)
					tested.prefix_notrack = true;
				else
					tested.prefix_no_notrack = true;
			}
			if (op_code.can_use_bnd_prefix()) {
				if (instruction.has_repne_prefix())
					tested.prefix_bnd = true;
				else
					tested.prefix_no_bnd = true;
			}
		}
		else
			FAIL("unreachable");
	}

	// Rust uses one Vec<Code> per name, eg. `wig32_16`. Key = name, eg. "wig32_16"
	std::unordered_map<std::string, std::vector<Code>> lists;
	const auto add = [&lists](const char* name, std::uint32_t bitness, Code code) {
		lists[std::string(name) + "_" + std::to_string(bitness)].push_back(code);
	};

	const auto names = code_names();
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		const std::vector<TestedInfo>* tested_infos;
		switch (bitness) {
		case 16:
			tested_infos = &tested_infos_16;
			break;
		case 32:
			tested_infos = &tested_infos_32;
			break;
		default:
			tested_infos = &tested_infos_64;
			break;
		}

		for (std::size_t ci = 0; ci < IcedConstants::CODE_ENUM_COUNT; ci++) {
			const Code code = static_cast<Code>(ci);
			if (is_ignored_code(names[ci]))
				continue;
			if (code == Code::Montmul_16 || code == Code::Montmul_64)
				continue;
			const OpCodeInfo& op_code = code_ext::op_code(code);
			if (!op_code.is_instruction() || op_code.code() == Code::Popw_CS)
				continue;
			if (op_code.fwait())
				continue;

			if (!op_code.is_available_in_mode(bitness))
				continue;

			const TestedInfo& tested = (*tested_infos)[ci];

			if ((bitness == 16 || bitness == 32) && op_code.is_wig32()) {
				if (tested.w_bits != 3)
					add("wig32", bitness, code);
			}
			if (op_code.is_wig()) {
				if (tested.w_bits != 3)
					add("wig", bitness, code);
			}
			if (bitness == 64 && op_code.mode64() && (op_code.encoding() == EncodingKind::Legacy || op_code.encoding() == EncodingKind::D3NOW)) {
				CHECK(!op_code.is_wig());
				CHECK(!op_code.is_wig32());
				if (can_use_w[ci] && tested.w_bits != 3)
					add("w", bitness, code);
			}
			if (op_code.is_lig()) {
				std::uint32_t all_l_bits;
				switch (op_code.encoding()) {
				case EncodingKind::VEX:
				case EncodingKind::XOP:
					all_l_bits = 3; // 1 bit = 2 values
					break;
				case EncodingKind::EVEX:
					all_l_bits = 0xF; // 2 bits = 4 values
					break;
				default:
					FAIL("unreachable");
				}
				if (tested.l_bits != all_l_bits)
					add("lig", bitness, code);
			}
			if (op_code.is_lig() && op_code.encoding() == EncodingKind::VEX) {
				if (tested.vex2_l_bits != 3 && can_use_vex2(op_code))
					add("vex2_lig", bitness, code);
			}
			if (can_use_modrm_rm_mem(op_code)) {
				if (!tested.reg_mem)
					add("rm", bitness, code);
			}
			if (can_use_modrm_rm_reg(op_code)) {
				if (!tested.reg_reg)
					add("rr", bitness, code);
			}
			switch (op_code.encoding()) {
			case EncodingKind::Legacy:
			case EncodingKind::VEX:
			case EncodingKind::XOP:
			case EncodingKind::D3NOW:
				break;
			case EncodingKind::EVEX:
			case EncodingKind::MVEX:
				if (!tested.mem_disp8 && can_use_modrm_rm_mem(op_code))
					add("disp8", bitness, code);
				break;
			}
			if (op_code.encoding() == EncodingKind::VEX) {
				if (!tested.vex3)
					add("vex3", bitness, code);
				if (!tested.vex2 && can_use_vex2(op_code))
					add("vex2", bitness, code);
			}
			if (op_code.can_use_op_mask_register()) {
				if (!tested.op_mask)
					add("opmask", bitness, code);
				if (!tested.no_op_mask && !op_code.require_op_mask_register())
					add("noopmask", bitness, code);
			}
			if (can_use_b(bitness, op_code)) {
				if (tested.b_bits != 3)
					add("b", bitness, code);
			}
			else {
				if ((tested.b_bits & 1) == 0)
					add("b", bitness, code);
			}
			switch (op_code.encoding()) {
			case EncodingKind::EVEX:
			case EncodingKind::MVEX:
				if (can_use_r2(op_code)) {
					if (tested.r2_bits != 3)
						add("r2", bitness, code);
				}
				else {
					if ((tested.r2_bits & 1) == 0)
						add("r2", bitness, code);
				}
				break;
			case EncodingKind::Legacy:
			case EncodingKind::VEX:
			case EncodingKind::XOP:
			case EncodingKind::D3NOW:
				break;
			}
			if (bitness == 64 && op_code.mode64()) {
				if (tested.vex2_r_bits != 3 && op_code.encoding() == EncodingKind::VEX && can_use_vex2(op_code) && can_use_r(op_code))
					add("vex2_r", bitness, code);
				if (can_use_r(op_code)) {
					if (tested.r_bits != 3)
						add("r", bitness, code);
				}
				else {
					if ((tested.r_bits & 1) == 0)
						add("r", bitness, code);
				}
				if (is_vsib(op_code)) {
					// The memory tests test vsib memory operands
				}
				else if (can_use_x(op_code)) {
					if (tested.x_bits != 3)
						add("x", bitness, code);
				}
				else {
					if ((tested.x_bits & 1) == 0)
						add("x", bitness, code);
				}
				switch (op_code.encoding()) {
				case EncodingKind::EVEX:
				case EncodingKind::MVEX:
					if (is_vsib(op_code)) {
						// The memory tests test vsib memory operands
					}
					else if (can_use_v2(op_code)) {
						if (tested.v2_bits != 3)
							add("v2", bitness, code);
					}
					else {
						if ((tested.v2_bits & 1) == 0)
							add("v2", bitness, code);
					}
					break;
				case EncodingKind::Legacy:
				case EncodingKind::VEX:
				case EncodingKind::XOP:
				case EncodingKind::D3NOW:
					break;
				}
			}
			if (op_code.can_use_xacquire_prefix()) {
				if (!tested.prefix_xacquire)
					add("pfx_xacquire", bitness, code);
				if (!tested.prefix_no_xacquire)
					add("pfx_no_xacquire", bitness, code);
			}
			if (op_code.can_use_xrelease_prefix()) {
				if (!tested.prefix_xrelease)
					add("pfx_xrelease", bitness, code);
				if (!tested.prefix_no_xrelease)
					add("pfx_no_xrelease", bitness, code);
			}
			if (op_code.can_use_lock_prefix()) {
				if (!tested.prefix_lock)
					add("pfx_lock", bitness, code);
				if (!tested.prefix_no_lock)
					add("pfx_no_lock", bitness, code);
			}
			if (op_code.can_use_hint_taken_prefix()) {
				if (!tested.prefix_hnt)
					add("pfx_hnt", bitness, code);
				if (!tested.prefix_no_hnt)
					add("pfx_no_hnt", bitness, code);
			}
			if (op_code.can_use_hint_taken_prefix()) {
				if (!tested.prefix_ht)
					add("pfx_ht", bitness, code);
				if (!tested.prefix_no_ht)
					add("pfx_no_ht", bitness, code);
			}
			if (op_code.can_use_rep_prefix()) {
				if (!tested.prefix_rep)
					add("pfx_rep", bitness, code);
				if (!tested.prefix_no_rep)
					add("pfx_no_rep", bitness, code);
			}
			if (op_code.can_use_repne_prefix()) {
				if (!tested.prefix_repne)
					add("pfx_repne", bitness, code);
				if (!tested.prefix_no_repne)
					add("pfx_no_repne", bitness, code);
			}
			if (op_code.can_use_notrack_prefix()) {
				if (!tested.prefix_notrack)
					add("pfx_notrack", bitness, code);
				if (!tested.prefix_no_notrack)
					add("pfx_no_notrack", bitness, code);
			}
			if (op_code.can_use_bnd_prefix()) {
				if (!tested.prefix_bnd)
					add("pfx_bnd", bitness, code);
				if (!tested.prefix_no_bnd)
					add("pfx_no_bnd", bitness, code);
			}
		}
	}

	static const char* const LIST_NAMES[] = {
		"wig32_16", "wig32_32", "wig_16", "wig_32", "wig_64", "w_64", "lig_16", "lig_32", "lig_64", "vex2_lig_16", "vex2_lig_32", "vex2_lig_64",
		"rr_16", "rr_32", "rr_64", "rm_16", "rm_32", "rm_64", "disp8_16", "disp8_32", "disp8_64", "vex2_16", "vex2_32", "vex2_64", "vex3_16",
		"vex3_32", "vex3_64", "opmask_16", "opmask_32", "opmask_64", "noopmask_16", "noopmask_32", "noopmask_64", "b_16", "b_32", "b_64", "r2_16",
		"r2_32", "r2_64", "r_64", "vex2_r_64", "x_64", "v2_64", "pfx_xacquire_16", "pfx_xacquire_32", "pfx_xacquire_64", "pfx_xrelease_16",
		"pfx_xrelease_32", "pfx_xrelease_64", "pfx_lock_16", "pfx_lock_32", "pfx_lock_64", "pfx_hnt_16", "pfx_hnt_32", "pfx_hnt_64", "pfx_ht_16",
		"pfx_ht_32", "pfx_ht_64", "pfx_rep_16", "pfx_rep_32", "pfx_rep_64", "pfx_repne_16", "pfx_repne_32", "pfx_repne_64", "pfx_notrack_16",
		"pfx_notrack_32", "pfx_notrack_64", "pfx_bnd_16", "pfx_bnd_32", "pfx_bnd_64", "pfx_no_xacquire_16", "pfx_no_xacquire_32",
		"pfx_no_xacquire_64", "pfx_no_xrelease_16", "pfx_no_xrelease_32", "pfx_no_xrelease_64", "pfx_no_lock_16", "pfx_no_lock_32",
		"pfx_no_lock_64", "pfx_no_hnt_16", "pfx_no_hnt_32", "pfx_no_hnt_64", "pfx_no_ht_16", "pfx_no_ht_32", "pfx_no_ht_64", "pfx_no_rep_16",
		"pfx_no_rep_32", "pfx_no_rep_64", "pfx_no_repne_16", "pfx_no_repne_32", "pfx_no_repne_64", "pfx_no_notrack_16", "pfx_no_notrack_32",
		"pfx_no_notrack_64", "pfx_no_bnd_16", "pfx_no_bnd_32", "pfx_no_bnd_64",
	};
	for (const char* list_name : LIST_NAMES) {
		std::string s = std::string(list_name) + ":";
		const auto it = lists.find(list_name);
		if (it != lists.end()) {
			for (std::size_t i = 0; i < it->second.size(); i++) {
				if (i > 0)
					s.push_back(',');
				s.append(to_string(it->second[i]));
			}
		}
		CHECK_EQ(s, std::string(list_name) + ":");
	}
	// Make sure all lists are checked
	for (const auto& kv : lists)
		CHECK(std::find_if(std::begin(LIST_NAMES), std::end(LIST_NAMES), [&kv](const char* n) { return kv.first == n; }) != std::end(LIST_NAMES));
}

TEST_CASE("encoder/test_invalid_zero_opmask_reg") {
	for (const auto& info : decoder_tests(false, false)) {
		if ((info.decoder_options() & DecoderOptions::NO_INVALID_CHECK) != 0)
			continue;
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		if (!op_code.require_op_mask_register())
			continue;

		auto bytes = to_vec_u8(info.hex_bytes());
		Instruction orig_instr = decode_one(info.bitness(), bytes, info.decoder_options());
		CHECK_EQ(orig_instr.code(), info.code());

		const std::size_t evex_index = get_evex_index(bytes);
		bytes[evex_index + 3] &= 0xF8;
		{
			const auto [instruction, error] = decode_one_err(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(instruction.code(), Code::INVALID);
			CHECK(error != DecoderError::None);
		}
		{
			const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() | DecoderOptions::NO_INVALID_CHECK);
			CHECK_EQ(instruction.code(), info.code());
			CHECK_EQ(instruction.op_mask(), Register::None);
			orig_instr.set_op_mask(Register::None);
			CHECK(orig_instr.eq_all_bits(instruction));
		}
	}
}

TEST_CASE("encoder/verify_cpu_mode") {
	std::unordered_set<Code> hash1632(code32_only().begin(), code32_only().end());
	hash1632.insert(not_decoded32_only().begin(), not_decoded32_only().end());
	std::unordered_set<Code> hash64(code64_only().begin(), code64_only().end());
	hash64.insert(not_decoded64_only().begin(), not_decoded64_only().end());
	const auto names = code_names();
	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++) {
		if (is_ignored_code(names[i]))
			continue;
		const Code code = static_cast<Code>(i);
		const OpCodeInfo& op_code = code_ext::op_code(code);
		if (hash1632.count(code) != 0) {
			CHECK(op_code.mode16());
			CHECK(op_code.mode32());
			CHECK(!op_code.mode64());
		}
		else if (hash64.count(code) != 0) {
			CHECK(!op_code.mode16());
			CHECK(!op_code.mode32());
			CHECK(op_code.mode64());
		}
		else {
			CHECK(op_code.mode16());
			CHECK(op_code.mode32());
			CHECK(op_code.mode64());
		}
	}
}

TEST_CASE("encoder/verify_can_only_decode_in_correct_mode") {
	const std::string extra_bytes((IcedConstants::MAX_INSTRUCTION_LENGTH - 1) * 2, '0');
	for (const auto& info : decoder_tests(false, false)) {
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		const std::string new_hex_bytes = info.hex_bytes() + extra_bytes;
		if (!op_code.mode16()) {
			const auto bytes = to_vec_u8(new_hex_bytes);
			const Instruction instruction = decode_one(16, bytes, info.decoder_options());
			CHECK(instruction.code() != info.code());
		}
		if (!op_code.mode32()) {
			const auto bytes = to_vec_u8(new_hex_bytes);
			const Instruction instruction = decode_one(32, bytes, info.decoder_options());
			CHECK(instruction.code() != info.code());
		}
		if (!op_code.mode64()) {
			const auto bytes = to_vec_u8(new_hex_bytes);
			const Instruction instruction = decode_one(64, bytes, info.decoder_options());
			CHECK(instruction.code() != info.code());
		}
	}
}

TEST_CASE("encoder/verify_invalid_table_encoding") {
	for (const auto& info : decoder_tests(false, false)) {
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		if (op_code.encoding() == EncodingKind::EVEX || op_code.encoding() == EncodingKind::MVEX) {
			auto hex_bytes = to_vec_u8(info.hex_bytes());
			const std::size_t evex_index = get_evex_index(hex_bytes);
			const std::uint32_t max_table = op_code.encoding() == EncodingKind::EVEX ? 0x08 : 0x10;
			for (std::uint32_t i = 0; i < max_table; i++) {
				if (op_code.encoding() == EncodingKind::EVEX) {
					if (i == 1 || i == 2 || i == 3 || i == 5 || i == 6)
						continue;
				}
				else {
					if (i >= 1 && i <= 3)
						continue;
				}
				hex_bytes[evex_index + 1] = static_cast<std::uint8_t>((hex_bytes[evex_index + 1] & ~(max_table - 1)) | i);
				{
					const auto [instruction, error] = decode_one_err(info.bitness(), hex_bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
				}
				{
					const auto [instruction, error] = decode_one_err(info.bitness(), hex_bytes, info.decoder_options() ^ DecoderOptions::NO_INVALID_CHECK);
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
				}
			}
		}
		else if (op_code.encoding() == EncodingKind::VEX) {
			auto hex_bytes = to_vec_u8(info.hex_bytes());
			const std::size_t vex_index = get_vex_xop_index(hex_bytes);
			if (hex_bytes[vex_index] == 0xC5)
				continue;
			for (std::uint32_t i = 0; i < 32; i++) {
				// 0: MVEX support is always compiled (Rust: #[cfg(feature = "mvex")] continue)
				if (i <= 3)
					continue;
				hex_bytes[vex_index + 1] = static_cast<std::uint8_t>((hex_bytes[vex_index + 1] & 0xE0) | i);
				{
					const auto [instruction, error] = decode_one_err(info.bitness(), hex_bytes, info.decoder_options());
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
				}
				{
					const auto [instruction, error] = decode_one_err(info.bitness(), hex_bytes, info.decoder_options() ^ DecoderOptions::NO_INVALID_CHECK);
					CHECK_EQ(instruction.code(), Code::INVALID);
					CHECK(error != DecoderError::None);
				}
			}
		}
		else if (op_code.encoding() == EncodingKind::XOP) {
			auto hex_bytes = to_vec_u8(info.hex_bytes());
			const std::size_t vex_index = get_vex_xop_index(hex_bytes);
			for (std::uint32_t i = 0; i < 32; i++) {
				if (i >= 8 && i <= 10)
					continue;
				hex_bytes[vex_index + 1] = static_cast<std::uint8_t>((hex_bytes[vex_index + 1] & 0xE0) | i);
				{
					const auto [instruction, error] = decode_one_err(info.bitness(), hex_bytes, info.decoder_options());
					if (i < 8)
						CHECK(instruction.code() != info.code());
					else {
						CHECK_EQ(instruction.code(), Code::INVALID);
						CHECK(error != DecoderError::None);
					}
				}
				{
					const auto [instruction, error] = decode_one_err(info.bitness(), hex_bytes, info.decoder_options() ^ DecoderOptions::NO_INVALID_CHECK);
					if (i < 8)
						CHECK(instruction.code() != info.code());
					else {
						CHECK_EQ(instruction.code(), Code::INVALID);
						CHECK(error != DecoderError::None);
					}
				}
			}
		}
		else if (op_code.encoding() == EncodingKind::Legacy || op_code.encoding() == EncodingKind::D3NOW) {
		}
		else
			FAIL("unreachable");
	}
}

TEST_CASE("encoder/verify_invalid_pp_field") {
	for (const auto& info : decoder_tests(false, false)) {
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		if (op_code.encoding() == EncodingKind::EVEX || op_code.encoding() == EncodingKind::MVEX) {
			auto hex_bytes = to_vec_u8(info.hex_bytes());
			const std::size_t evex_index = get_evex_index(hex_bytes);
			const std::uint8_t b = hex_bytes[evex_index + 2];
			for (std::uint32_t i = 1; i < 4; i++) {
				hex_bytes[evex_index + 2] = static_cast<std::uint8_t>(b ^ i);
				{
					const Instruction instruction = decode_one(info.bitness(), hex_bytes, info.decoder_options());
					CHECK(instruction.code() != info.code());
				}
				{
					const Instruction instruction = decode_one(info.bitness(), hex_bytes, info.decoder_options() ^ DecoderOptions::NO_INVALID_CHECK);
					CHECK(instruction.code() != info.code());
				}
			}
		}
		else if (op_code.encoding() == EncodingKind::VEX || op_code.encoding() == EncodingKind::XOP) {
			auto hex_bytes = to_vec_u8(info.hex_bytes());
			const std::size_t vex_index = get_vex_xop_index(hex_bytes);
			const std::size_t pp_index = hex_bytes[vex_index] == 0xC5 ? vex_index + 1 : vex_index + 2;
			const std::uint8_t b = hex_bytes[pp_index];
			for (std::uint32_t i = 1; i < 4; i++) {
				hex_bytes[pp_index] = static_cast<std::uint8_t>(b ^ i);
				{
					const Instruction instruction = decode_one(info.bitness(), hex_bytes, info.decoder_options());
					CHECK(instruction.code() != info.code());
				}
				{
					const Instruction instruction = decode_one(info.bitness(), hex_bytes, info.decoder_options() ^ DecoderOptions::NO_INVALID_CHECK);
					CHECK(instruction.code() != info.code());
				}
			}
		}
		else if (op_code.encoding() == EncodingKind::Legacy || op_code.encoding() == EncodingKind::D3NOW) {
		}
		else
			FAIL("unreachable");
	}
}

TEST_CASE("encoder/verify_regonly_or_regmemonly_mod_bits") {
	const auto is_reg_only_or_reg_mem_only_mod_rm = [](const OpCodeInfo& op_code) {
		for (const OpCodeOperandKind op_kind : op_code.op_kinds()) {
			switch (op_kind) {
			case OpCodeOperandKind::mem:
			case OpCodeOperandKind::sibmem:
			case OpCodeOperandKind::mem_mpx:
			case OpCodeOperandKind::mem_mib:
			case OpCodeOperandKind::mem_vsib32x:
			case OpCodeOperandKind::mem_vsib64x:
			case OpCodeOperandKind::mem_vsib32y:
			case OpCodeOperandKind::mem_vsib64y:
			case OpCodeOperandKind::mem_vsib32z:
			case OpCodeOperandKind::mem_vsib64z:
			case OpCodeOperandKind::r16_rm:
			case OpCodeOperandKind::r32_rm:
			case OpCodeOperandKind::r64_rm:
			case OpCodeOperandKind::k_rm:
			case OpCodeOperandKind::mm_rm:
			case OpCodeOperandKind::xmm_rm:
			case OpCodeOperandKind::ymm_rm:
			case OpCodeOperandKind::zmm_rm:
			case OpCodeOperandKind::tmm_rm:
				return true;
			default:
				break;
			}
		}
		return false;
	};

	const std::string extra_bytes((IcedConstants::MAX_INSTRUCTION_LENGTH - 1) * 2, '0');
	for (const auto& info : decoder_tests(false, false)) {
		const OpCodeInfo& op_code = code_ext::op_code(info.code());
		if (!is_reg_only_or_reg_mem_only_mod_rm(op_code))
			continue;
		// There are a few instructions that ignore the mod bits...
		if (op_code.ignores_mod_bits())
			continue;

		auto bytes = to_vec_u8(info.hex_bytes() + extra_bytes);
		std::size_t m_index;
		if (op_code.encoding() == EncodingKind::EVEX || op_code.encoding() == EncodingKind::MVEX)
			m_index = get_evex_index(bytes) + 5;
		else if (op_code.encoding() == EncodingKind::VEX || op_code.encoding() == EncodingKind::XOP) {
			const std::size_t vex_index = get_vex_xop_index(bytes);
			m_index = bytes[vex_index] == 0xC5 ? vex_index + 3 : vex_index + 4;
		}
		else if (op_code.encoding() == EncodingKind::Legacy || op_code.encoding() == EncodingKind::D3NOW) {
			m_index = skip_prefixes(bytes, info.bitness()).first;
			switch (op_code.table()) {
			case OpCodeTableKind::Normal:
				break;
			case OpCodeTableKind::T0F:
				REQUIRE(bytes[m_index] == 0x0F);
				m_index++;
				break;
			case OpCodeTableKind::T0F38:
				REQUIRE(bytes[m_index] == 0x0F);
				m_index++;
				REQUIRE(bytes[m_index] == 0x38);
				m_index++;
				break;
			case OpCodeTableKind::T0F3A:
				REQUIRE(bytes[m_index] == 0x0F);
				m_index++;
				REQUIRE(bytes[m_index] == 0x3A);
				m_index++;
				break;
			default:
				FAIL("unreachable");
			}
			m_index++;
		}
		else
			FAIL("unreachable");

		if (bytes[m_index] >= 0xC0)
			bytes[m_index] &= 0x3F;
		else
			bytes[m_index] |= 0xC0;
		{
			const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK(instruction.code() != info.code());
		}
		{
			const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options() ^ DecoderOptions::NO_INVALID_CHECK);
			CHECK(instruction.code() != info.code());
		}
	}
}

TEST_CASE("encoder/disable_decoder_option_disables_instruction") {
	const std::string extra_bytes((IcedConstants::MAX_INSTRUCTION_LENGTH - 1) * 2, '0');
	for (const auto& info : decoder_tests(false, false)) {
		if (info.decoder_options() == DecoderOptions::NONE)
			continue;
		constexpr std::uint32_t NO_OPTIONS = DecoderOptions::NO_INVALID_CHECK | DecoderOptions::NO_PAUSE | DecoderOptions::NO_WBNOINVD |
			DecoderOptions::NO_MPFX_0FBC | DecoderOptions::NO_MPFX_0FBD | DecoderOptions::NO_LAHF_SAHF_64;
		if ((info.decoder_options() & NO_OPTIONS) != 0)
			continue;
		// is_power_of_two()
		if ((info.decoder_options() & (info.decoder_options() - 1)) != 0)
			continue;
		if (info.decoder_options() == DecoderOptions::FORCE_RESERVED_NOP)
			continue;
		if ((info.decoder_test_options() & DecoderTestOptions::NO_OPT_DISABLE_TEST) != 0)
			continue;

		{
			const auto bytes = to_vec_u8(info.hex_bytes());
			const Instruction instruction = decode_one(info.bitness(), bytes, info.decoder_options());
			CHECK_EQ(instruction.code(), info.code());
		}
		{
			const auto bytes = to_vec_u8(info.hex_bytes() + extra_bytes);
			const Instruction instruction = decode_one(info.bitness(), bytes, DecoderOptions::NONE);
			CHECK(instruction.code() != info.code());
		}
	}
}

} // namespace iced_x86::tests
