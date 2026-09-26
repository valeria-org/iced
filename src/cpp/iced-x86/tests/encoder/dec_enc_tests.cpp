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

// DEC_ENC_PART2

} // namespace iced_x86::tests
