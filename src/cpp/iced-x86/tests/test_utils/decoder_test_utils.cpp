// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_utils/decoder_test_utils.hpp"
#include "generated/decoder_test_options.hpp"
#include "generated/decoder_test_parser_constants.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/str_utils.hpp"

#include "iced_x86/decoder_options.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace iced_x86::tests {

namespace {

// Rust: TO_DECODER_TEST_PARSER_CONSTANTS (the u32 values of `DecoderTestParserConstants`)
enum class Key {
	DecoderError,
	Broadcast,
	Xacquire,
	Xrelease,
	Rep,
	Repe,
	Repne,
	Lock,
	ZeroingMasking,
	SuppressAllExceptions,
	Vsib32,
	Vsib64,
	RoundToNearest,
	RoundDown,
	RoundUp,
	RoundTowardZero,
	Op0Kind,
	Op1Kind,
	Op2Kind,
	Op3Kind,
	Op4Kind,
	EncodedHexBytes,
	Code_,
	DecoderOptions_,
	SegmentPrefixES,
	SegmentPrefixCS,
	SegmentPrefixSS,
	SegmentPrefixDS,
	SegmentPrefixFS,
	SegmentPrefixGS,
	OpMaskK1,
	OpMaskK2,
	OpMaskK3,
	OpMaskK4,
	OpMaskK5,
	OpMaskK6,
	OpMaskK7,
	ConstantOffsets_,
	OpKindRegister,
	OpKindNearBranch16,
	OpKindNearBranch32,
	OpKindNearBranch64,
	OpKindFarBranch16,
	OpKindFarBranch32,
	OpKindImmediate8,
	OpKindImmediate16,
	OpKindImmediate32,
	OpKindImmediate64,
	OpKindImmediate8to16,
	OpKindImmediate8to32,
	OpKindImmediate8to64,
	OpKindImmediate32to64,
	OpKindImmediate8_2nd,
	OpKindMemorySegSI,
	OpKindMemorySegESI,
	OpKindMemorySegRSI,
	OpKindMemorySegDI,
	OpKindMemorySegEDI,
	OpKindMemorySegRDI,
	OpKindMemoryESDI,
	OpKindMemoryESEDI,
	OpKindMemoryESRDI,
	OpKindMemory,
	DecoderTestOptionsNoEncode,
	DecoderTestOptionsNoOptDisableTest,
	Ip,
	EvictionHint,
	MvexRegSwizzleNone,
	MvexRegSwizzleCdab,
	MvexRegSwizzleBadc,
	MvexRegSwizzleDacb,
	MvexRegSwizzleAaaa,
	MvexRegSwizzleBbbb,
	MvexRegSwizzleCccc,
	MvexRegSwizzleDddd,
	MvexMemConvNone,
	MvexMemConvBroadcast1,
	MvexMemConvBroadcast4,
	MvexMemConvFloat16,
	MvexMemConvUint8,
	MvexMemConvSint8,
	MvexMemConvUint16,
	MvexMemConvSint16,
	Invalid,
};

const std::unordered_map<std::string_view, Key>& get_keys() {
	using C = DecoderTestParserConstants;
	static const std::unordered_map<std::string_view, Key> keys = {
		{C::DECODER_ERROR, Key::DecoderError},
		{C::BROADCAST, Key::Broadcast},
		{C::XACQUIRE, Key::Xacquire},
		{C::XRELEASE, Key::Xrelease},
		{C::REP, Key::Rep},
		{C::REPE, Key::Repe},
		{C::REPNE, Key::Repne},
		{C::LOCK, Key::Lock},
		{C::ZEROING_MASKING, Key::ZeroingMasking},
		{C::SUPPRESS_ALL_EXCEPTIONS, Key::SuppressAllExceptions},
		{C::VSIB32, Key::Vsib32},
		{C::VSIB64, Key::Vsib64},
		{C::ROUND_TO_NEAREST, Key::RoundToNearest},
		{C::ROUND_DOWN, Key::RoundDown},
		{C::ROUND_UP, Key::RoundUp},
		{C::ROUND_TOWARD_ZERO, Key::RoundTowardZero},
		{C::OP0_KIND, Key::Op0Kind},
		{C::OP1_KIND, Key::Op1Kind},
		{C::OP2_KIND, Key::Op2Kind},
		{C::OP3_KIND, Key::Op3Kind},
		{C::OP4_KIND, Key::Op4Kind},
		{C::ENCODED_HEX_BYTES, Key::EncodedHexBytes},
		{C::CODE, Key::Code_},
		{C::DECODER_OPTIONS, Key::DecoderOptions_},
		{C::SEGMENT_PREFIX_ES, Key::SegmentPrefixES},
		{C::SEGMENT_PREFIX_CS, Key::SegmentPrefixCS},
		{C::SEGMENT_PREFIX_SS, Key::SegmentPrefixSS},
		{C::SEGMENT_PREFIX_DS, Key::SegmentPrefixDS},
		{C::SEGMENT_PREFIX_FS, Key::SegmentPrefixFS},
		{C::SEGMENT_PREFIX_GS, Key::SegmentPrefixGS},
		{C::OP_MASK_K1, Key::OpMaskK1},
		{C::OP_MASK_K2, Key::OpMaskK2},
		{C::OP_MASK_K3, Key::OpMaskK3},
		{C::OP_MASK_K4, Key::OpMaskK4},
		{C::OP_MASK_K5, Key::OpMaskK5},
		{C::OP_MASK_K6, Key::OpMaskK6},
		{C::OP_MASK_K7, Key::OpMaskK7},
		{C::CONSTANT_OFFSETS, Key::ConstantOffsets_},
		{C::OP_KIND_REGISTER, Key::OpKindRegister},
		{C::OP_KIND_NEAR_BRANCH16, Key::OpKindNearBranch16},
		{C::OP_KIND_NEAR_BRANCH32, Key::OpKindNearBranch32},
		{C::OP_KIND_NEAR_BRANCH64, Key::OpKindNearBranch64},
		{C::OP_KIND_FAR_BRANCH16, Key::OpKindFarBranch16},
		{C::OP_KIND_FAR_BRANCH32, Key::OpKindFarBranch32},
		{C::OP_KIND_IMMEDIATE8, Key::OpKindImmediate8},
		{C::OP_KIND_IMMEDIATE16, Key::OpKindImmediate16},
		{C::OP_KIND_IMMEDIATE32, Key::OpKindImmediate32},
		{C::OP_KIND_IMMEDIATE64, Key::OpKindImmediate64},
		{C::OP_KIND_IMMEDIATE8TO16, Key::OpKindImmediate8to16},
		{C::OP_KIND_IMMEDIATE8TO32, Key::OpKindImmediate8to32},
		{C::OP_KIND_IMMEDIATE8TO64, Key::OpKindImmediate8to64},
		{C::OP_KIND_IMMEDIATE32TO64, Key::OpKindImmediate32to64},
		{C::OP_KIND_IMMEDIATE8_2ND, Key::OpKindImmediate8_2nd},
		{C::OP_KIND_MEMORY_SEG_SI, Key::OpKindMemorySegSI},
		{C::OP_KIND_MEMORY_SEG_ESI, Key::OpKindMemorySegESI},
		{C::OP_KIND_MEMORY_SEG_RSI, Key::OpKindMemorySegRSI},
		{C::OP_KIND_MEMORY_SEG_DI, Key::OpKindMemorySegDI},
		{C::OP_KIND_MEMORY_SEG_EDI, Key::OpKindMemorySegEDI},
		{C::OP_KIND_MEMORY_SEG_RDI, Key::OpKindMemorySegRDI},
		{C::OP_KIND_MEMORY_ES_DI, Key::OpKindMemoryESDI},
		{C::OP_KIND_MEMORY_ES_EDI, Key::OpKindMemoryESEDI},
		{C::OP_KIND_MEMORY_ES_RDI, Key::OpKindMemoryESRDI},
		{C::OP_KIND_MEMORY, Key::OpKindMemory},
		{C::DECODER_TEST_OPTIONS_NO_ENCODE, Key::DecoderTestOptionsNoEncode},
		{C::DECODER_TEST_OPTIONS_NO_OPT_DISABLE_TEST, Key::DecoderTestOptionsNoOptDisableTest},
		{C::IP, Key::Ip},
		{C::EVICTION_HINT, Key::EvictionHint},
		{C::MVEX_REG_SWIZZLE_NONE, Key::MvexRegSwizzleNone},
		{C::MVEX_REG_SWIZZLE_CDAB, Key::MvexRegSwizzleCdab},
		{C::MVEX_REG_SWIZZLE_BADC, Key::MvexRegSwizzleBadc},
		{C::MVEX_REG_SWIZZLE_DACB, Key::MvexRegSwizzleDacb},
		{C::MVEX_REG_SWIZZLE_AAAA, Key::MvexRegSwizzleAaaa},
		{C::MVEX_REG_SWIZZLE_BBBB, Key::MvexRegSwizzleBbbb},
		{C::MVEX_REG_SWIZZLE_CCCC, Key::MvexRegSwizzleCccc},
		{C::MVEX_REG_SWIZZLE_DDDD, Key::MvexRegSwizzleDddd},
		{C::MVEX_MEM_CONV_NONE, Key::MvexMemConvNone},
		{C::MVEX_MEM_CONV_BROADCAST1, Key::MvexMemConvBroadcast1},
		{C::MVEX_MEM_CONV_BROADCAST4, Key::MvexMemConvBroadcast4},
		{C::MVEX_MEM_CONV_FLOAT16, Key::MvexMemConvFloat16},
		{C::MVEX_MEM_CONV_UINT8, Key::MvexMemConvUint8},
		{C::MVEX_MEM_CONV_SINT8, Key::MvexMemConvSint8},
		{C::MVEX_MEM_CONV_UINT16, Key::MvexMemConvUint16},
		{C::MVEX_MEM_CONV_SINT16, Key::MvexMemConvSint16},
	};
	return keys;
}

Key to_key(std::string_view s) {
	const auto& keys = get_keys();
	auto it = keys.find(s);
	return it == keys.end() ? Key::Invalid : it->second;
}

[[noreturn]] void throw_error(const std::string& message) { throw std::runtime_error(message); }

void check_parts(std::size_t operand, const std::vector<std::string_view>& parts, std::size_t expected) {
	if (parts.size() != expected)
		throw_error("Operand " + std::to_string(operand) + ": expected " + std::to_string(expected) +
					" values, actual = " + std::to_string(parts.size()));
}

void read_op_kind(DecoderTestCase& tc, std::size_t operand, std::string_view value) {
	auto parts = split(value, ';');
	switch (to_key(parts[0])) {
	case Key::OpKindRegister:
		check_parts(operand, parts, 2);
		tc.op_registers[operand] = to_register(parts[1]);
		tc.op_kinds[operand] = OpKind::Register;
		break;

	case Key::OpKindNearBranch16:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::NearBranch16;
		tc.near_branch = to_u16(parts[1]);
		break;

	case Key::OpKindNearBranch32:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::NearBranch32;
		tc.near_branch = to_u32(parts[1]);
		break;

	case Key::OpKindNearBranch64:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::NearBranch64;
		tc.near_branch = to_u64(parts[1]);
		break;

	case Key::OpKindFarBranch16:
		check_parts(operand, parts, 3);
		tc.op_kinds[operand] = OpKind::FarBranch16;
		tc.far_branch_selector = to_u16(parts[1]);
		tc.far_branch = to_u16(parts[2]);
		break;

	case Key::OpKindFarBranch32:
		check_parts(operand, parts, 3);
		tc.op_kinds[operand] = OpKind::FarBranch32;
		tc.far_branch_selector = to_u16(parts[1]);
		tc.far_branch = to_u32(parts[2]);
		break;

	case Key::OpKindImmediate8:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::Immediate8;
		tc.immediate = to_u8(parts[1]);
		break;

	case Key::OpKindImmediate16:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::Immediate16;
		tc.immediate = to_u16(parts[1]);
		break;

	case Key::OpKindImmediate32:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::Immediate32;
		tc.immediate = to_u32(parts[1]);
		break;

	case Key::OpKindImmediate64:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::Immediate64;
		tc.immediate = to_u64(parts[1]);
		break;

	case Key::OpKindImmediate8to16:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::Immediate8to16;
		tc.immediate = to_u16(parts[1]);
		break;

	case Key::OpKindImmediate8to32:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::Immediate8to32;
		tc.immediate = to_u32(parts[1]);
		break;

	case Key::OpKindImmediate8to64:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::Immediate8to64;
		tc.immediate = to_u64(parts[1]);
		break;

	case Key::OpKindImmediate32to64:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::Immediate32to64;
		tc.immediate = to_u64(parts[1]);
		break;

	case Key::OpKindImmediate8_2nd:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::Immediate8_2nd;
		tc.immediate_2nd = to_u8(parts[1]);
		break;

	case Key::OpKindMemorySegSI:
	case Key::OpKindMemorySegESI:
	case Key::OpKindMemorySegRSI:
	case Key::OpKindMemorySegDI:
	case Key::OpKindMemorySegEDI:
	case Key::OpKindMemorySegRDI: {
		check_parts(operand, parts, 3);
		OpKind op_kind;
		switch (to_key(parts[0])) {
		case Key::OpKindMemorySegSI:
			op_kind = OpKind::MemorySegSI;
			break;
		case Key::OpKindMemorySegESI:
			op_kind = OpKind::MemorySegESI;
			break;
		case Key::OpKindMemorySegRSI:
			op_kind = OpKind::MemorySegRSI;
			break;
		case Key::OpKindMemorySegDI:
			op_kind = OpKind::MemorySegDI;
			break;
		case Key::OpKindMemorySegEDI:
			op_kind = OpKind::MemorySegEDI;
			break;
		default:
			op_kind = OpKind::MemorySegRDI;
			break;
		}
		tc.op_kinds[operand] = op_kind;
		tc.memory_segment = to_register(parts[1]);
		tc.memory_size = to_memory_size(parts[2]);
		break;
	}

	case Key::OpKindMemoryESDI:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::MemoryESDI;
		tc.memory_size = to_memory_size(parts[1]);
		break;

	case Key::OpKindMemoryESEDI:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::MemoryESEDI;
		tc.memory_size = to_memory_size(parts[1]);
		break;

	case Key::OpKindMemoryESRDI:
		check_parts(operand, parts, 2);
		tc.op_kinds[operand] = OpKind::MemoryESRDI;
		tc.memory_size = to_memory_size(parts[1]);
		break;

	case Key::OpKindMemory:
		check_parts(operand, parts, 8);
		tc.op_kinds[operand] = OpKind::Memory;
		tc.memory_segment = to_register(parts[1]);
		tc.memory_base = to_register(parts[2]);
		tc.memory_index = to_register(parts[3]);
		tc.memory_index_scale = to_u32(parts[4]);
		tc.memory_displacement = to_u64(parts[5]);
		tc.memory_displ_size = to_u32(parts[6]);
		tc.memory_size = to_memory_size(parts[7]);
		break;

	default:
		throw_error("Invalid opkind: '" + std::string(parts[0]) + "'");
	}
}

std::uint32_t parse_decoder_options(std::string_view value) {
	std::uint32_t decoder_options = 0;
	for (auto option_str : split(value, ';')) {
		try {
			decoder_options |= to_decoder_options(option_str);
		}
		catch (const std::exception&) {
			throw_error("Invalid decoder options: " + std::string(option_str));
		}
	}
	return decoder_options;
}

void read_op_kind_checked(DecoderTestCase& tc, std::size_t operand, std::string_view value) {
	if (tc.op_count < operand + 1)
		throw_error("Invalid OpCount: " + std::to_string(tc.op_count) + " < " + std::to_string(operand + 1));
	read_op_kind(tc, operand, value);
}

// Returns false if the test case should be ignored
bool read_next_test_case(DecoderTestCase& tc, std::uint32_t bitness, std::string_view line, std::uint32_t line_number) {
	auto parts = split(line, ',');
	if (parts.size() != 5)
		throw_error("Invalid number of commas (" + std::to_string(parts.size() - 1) + " commas)");

	tc = DecoderTestCase();
	tc.line_number = line_number;
	tc.test_options = DecoderTestOptions::NONE;
	tc.bitness = bitness;
	tc.ip = get_default_ip(bitness);
	tc.hex_bytes = std::string(trim(parts[0]));
	(void)to_vec_u8(tc.hex_bytes);
	tc.encoded_hex_bytes = tc.hex_bytes;
	if (is_ignored_code(parts[1]))
		return false;
	tc.code = to_code(parts[1]);
	tc.mnemonic = to_mnemonic(parts[2]);
	tc.op_count = to_u32(parts[3]);
	tc.decoder_error = tc.code == Code::INVALID ? DecoderError::InvalidInstruction : DecoderError::None;

	bool found_code = false;
	for (auto key : split_whitespace(parts[4])) {
		if (key.empty())
			continue;
		std::string_view value;
		auto eq_index = key.find('=');
		if (eq_index != std::string_view::npos) {
			value = key.substr(eq_index + 1);
			key = key.substr(0, eq_index);
		}

		switch (to_key(key)) {
		case Key::DecoderError:
			tc.decoder_error = to_decoder_error(value);
			break;
		case Key::Broadcast:
			tc.is_broadcast = true;
			break;
		case Key::Xacquire:
			tc.has_xacquire_prefix = true;
			break;
		case Key::Xrelease:
			tc.has_xrelease_prefix = true;
			break;
		case Key::Rep:
		case Key::Repe:
			tc.has_repe_prefix = true;
			break;
		case Key::Repne:
			tc.has_repne_prefix = true;
			break;
		case Key::Lock:
			tc.has_lock_prefix = true;
			break;
		case Key::ZeroingMasking:
			tc.zeroing_masking = true;
			break;
		case Key::SuppressAllExceptions:
			tc.suppress_all_exceptions = true;
			break;
		case Key::Vsib32:
			tc.vsib_bitness = 32;
			break;
		case Key::Vsib64:
			tc.vsib_bitness = 64;
			break;
		case Key::RoundToNearest:
			tc.rounding_control = RoundingControl::RoundToNearest;
			break;
		case Key::RoundDown:
			tc.rounding_control = RoundingControl::RoundDown;
			break;
		case Key::RoundUp:
			tc.rounding_control = RoundingControl::RoundUp;
			break;
		case Key::RoundTowardZero:
			tc.rounding_control = RoundingControl::RoundTowardZero;
			break;
		case Key::Op0Kind:
			read_op_kind_checked(tc, 0, value);
			break;
		case Key::Op1Kind:
			read_op_kind_checked(tc, 1, value);
			break;
		case Key::Op2Kind:
			read_op_kind_checked(tc, 2, value);
			break;
		case Key::Op3Kind:
			read_op_kind_checked(tc, 3, value);
			break;
		case Key::Op4Kind:
			read_op_kind_checked(tc, 4, value);
			break;

		case Key::EncodedHexBytes:
			if (value.empty())
				throw_error("Invalid encoded hex bytes: '" + std::string(value) + "'");
			tc.encoded_hex_bytes = std::string(value);
			(void)to_vec_u8(tc.encoded_hex_bytes);
			break;

		case Key::Code_:
			if (value.empty())
				throw_error("Invalid Code value: '" + std::string(value) + "'");
			if (is_ignored_code(value))
				return false;
			found_code = true;
			break;

		case Key::Ip:
			if (value.empty())
				throw_error("Invalid encoded IP: '" + std::string(value) + "'");
			tc.ip = to_u64(value);
			break;

		case Key::EvictionHint:
			tc.mvex.eviction_hint = true;
			break;
		case Key::MvexRegSwizzleNone:
			tc.mvex.reg_mem_conv = MvexRegMemConv::RegSwizzleNone;
			break;
		case Key::MvexRegSwizzleCdab:
			tc.mvex.reg_mem_conv = MvexRegMemConv::RegSwizzleCdab;
			break;
		case Key::MvexRegSwizzleBadc:
			tc.mvex.reg_mem_conv = MvexRegMemConv::RegSwizzleBadc;
			break;
		case Key::MvexRegSwizzleDacb:
			tc.mvex.reg_mem_conv = MvexRegMemConv::RegSwizzleDacb;
			break;
		case Key::MvexRegSwizzleAaaa:
			tc.mvex.reg_mem_conv = MvexRegMemConv::RegSwizzleAaaa;
			break;
		case Key::MvexRegSwizzleBbbb:
			tc.mvex.reg_mem_conv = MvexRegMemConv::RegSwizzleBbbb;
			break;
		case Key::MvexRegSwizzleCccc:
			tc.mvex.reg_mem_conv = MvexRegMemConv::RegSwizzleCccc;
			break;
		case Key::MvexRegSwizzleDddd:
			tc.mvex.reg_mem_conv = MvexRegMemConv::RegSwizzleDddd;
			break;
		case Key::MvexMemConvNone:
			tc.mvex.reg_mem_conv = MvexRegMemConv::MemConvNone;
			break;
		case Key::MvexMemConvBroadcast1:
			tc.mvex.reg_mem_conv = MvexRegMemConv::MemConvBroadcast1;
			break;
		case Key::MvexMemConvBroadcast4:
			tc.mvex.reg_mem_conv = MvexRegMemConv::MemConvBroadcast4;
			break;
		case Key::MvexMemConvFloat16:
			tc.mvex.reg_mem_conv = MvexRegMemConv::MemConvFloat16;
			break;
		case Key::MvexMemConvUint8:
			tc.mvex.reg_mem_conv = MvexRegMemConv::MemConvUint8;
			break;
		case Key::MvexMemConvSint8:
			tc.mvex.reg_mem_conv = MvexRegMemConv::MemConvSint8;
			break;
		case Key::MvexMemConvUint16:
			tc.mvex.reg_mem_conv = MvexRegMemConv::MemConvUint16;
			break;
		case Key::MvexMemConvSint16:
			tc.mvex.reg_mem_conv = MvexRegMemConv::MemConvSint16;
			break;

		case Key::DecoderOptions_:
			tc.decoder_options |= parse_decoder_options(value);
			break;
		case Key::SegmentPrefixES:
			tc.segment_prefix = Register::ES;
			break;
		case Key::SegmentPrefixCS:
			tc.segment_prefix = Register::CS;
			break;
		case Key::SegmentPrefixSS:
			tc.segment_prefix = Register::SS;
			break;
		case Key::SegmentPrefixDS:
			tc.segment_prefix = Register::DS;
			break;
		case Key::SegmentPrefixFS:
			tc.segment_prefix = Register::FS;
			break;
		case Key::SegmentPrefixGS:
			tc.segment_prefix = Register::GS;
			break;
		case Key::OpMaskK1:
			tc.op_mask = Register::K1;
			break;
		case Key::OpMaskK2:
			tc.op_mask = Register::K2;
			break;
		case Key::OpMaskK3:
			tc.op_mask = Register::K3;
			break;
		case Key::OpMaskK4:
			tc.op_mask = Register::K4;
			break;
		case Key::OpMaskK5:
			tc.op_mask = Register::K5;
			break;
		case Key::OpMaskK6:
			tc.op_mask = Register::K6;
			break;
		case Key::OpMaskK7:
			tc.op_mask = Register::K7;
			break;
		case Key::ConstantOffsets_:
			tc.constant_offsets = parse_constant_offsets(value);
			break;
		case Key::DecoderTestOptionsNoEncode:
			tc.test_options |= DecoderTestOptions::NO_ENCODE;
			break;
		case Key::DecoderTestOptionsNoOptDisableTest:
			tc.test_options |= DecoderTestOptions::NO_OPT_DISABLE_TEST;
			break;
		default:
			throw_error("Invalid key '" + std::string(key) + "'");
		}
	}

	if (tc.code == Code::INVALID && !found_code)
		throw_error("Test case decodes to INVALID but there's no code=xxx showing the original Code value so it can be filtered out if needed");

	return true;
}

// Returns false if the test case should be ignored
bool read_next_mem_test_case(DecoderMemoryTestCase& tc, std::uint32_t bitness, std::string_view line, std::uint32_t line_number) {
	auto parts = split(line, ',');
	if (parts.size() != 11 && parts.size() != 12)
		throw_error("Invalid number of commas (" + std::to_string(parts.size() - 1) + " commas)");

	tc = DecoderMemoryTestCase();
	auto hex_bytes = trim(parts[0]);
	tc.ip = get_default_ip(bitness);
	(void)to_vec_u8(hex_bytes);
	if (is_ignored_code(trim(parts[1])))
		return false;
	tc.code = to_code(trim(parts[1]));
	tc.register_ = to_register(trim(parts[2]));
	tc.prefix_segment = to_register(trim(parts[3]));
	tc.segment = to_register(trim(parts[4]));
	tc.base_register = to_register(trim(parts[5]));
	tc.index_register = to_register(trim(parts[6]));
	tc.scale = to_u32(trim(parts[7]));
	tc.displacement = to_u64(trim(parts[8]));
	tc.displ_size = to_u32(trim(parts[9]));
	tc.constant_offsets = parse_constant_offsets(trim(parts[10]));
	auto encoded_hex_bytes = parts.size() == 11 ? hex_bytes : trim(parts[11]);
	(void)to_vec_u8(encoded_hex_bytes);

	tc.bitness = bitness;
	tc.hex_bytes = std::string(hex_bytes);
	tc.encoded_hex_bytes = std::string(encoded_hex_bytes);
	tc.decoder_options = DecoderOptions::NONE;
	tc.line_number = line_number;
	tc.test_options = DecoderTestOptions::NONE;
	return true;
}

std::unordered_set<Code> read_code_values(const char* name) {
	std::string filename = get_decoder_unit_tests_dir() + "/" + name;
	std::unordered_set<Code> h;
	std::uint32_t line_number = 0;
	for (const auto& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || starts_with(line, "#") || is_ignored_code(line))
			continue;
		try {
			h.insert(to_code(line));
		}
		catch (const std::exception& ex) {
			throw_error("Error parsing Code file '" + filename + "', line " + std::to_string(line_number) + ": " + ex.what());
		}
	}
	return h;
}

std::vector<DecoderTestCase> read_decoder_test_cases(std::uint32_t bitness) {
	return read_decoder_test_cases_file(bitness, get_decoder_unit_tests_dir() + "/DecoderTest" + std::to_string(bitness) + ".txt");
}

std::vector<DecoderTestCase> read_decoder_misc_test_cases(std::uint32_t bitness) {
	return read_decoder_test_cases_file(bitness, get_decoder_unit_tests_dir() + "/DecoderTestMisc" + std::to_string(bitness) + ".txt");
}

std::vector<DecoderMemoryTestCase> read_decoder_mem_test_cases(std::uint32_t bitness) {
	return read_decoder_mem_test_cases_file(bitness, get_decoder_unit_tests_dir() + "/MemoryTest" + std::to_string(bitness) + ".txt");
}

std::size_t bitness_index(std::uint32_t bitness) {
	switch (bitness) {
	case 16:
		return 0;
	case 32:
		return 1;
	case 64:
		return 2;
	default:
		throw_error("Invalid bitness: " + std::to_string(bitness));
	}
}

// Rust: `Option<bool>`: -1 = None, 0 = Some(false), 1 = Some(true)
void add_tests(std::vector<DecoderTestInfo>& v, const std::vector<DecoderTestCase>& tests, bool include_invalid, int can_encode) {
	for (const auto& tc : tests) {
		if (!include_invalid && tc.code == Code::INVALID)
			continue;
		if (can_encode >= 0) {
			bool tc_can_encode = (tc.test_options & DecoderTestOptions::NO_ENCODE) == 0;
			if (tc_can_encode != (can_encode != 0))
				continue;
		}
		v.emplace_back(tc.bitness, tc.code, tc.hex_bytes, tc.ip, tc.encoded_hex_bytes, tc.decoder_options, tc.test_options);
	}
}

void add_tests_mem(std::vector<DecoderTestInfo>& v, const std::vector<DecoderMemoryTestCase>& tests, bool include_invalid, int can_encode) {
	for (const auto& tc : tests) {
		if (!include_invalid && tc.code == Code::INVALID)
			continue;
		if (can_encode >= 0) {
			bool tc_can_encode = (tc.test_options & DecoderTestOptions::NO_ENCODE) == 0;
			if (tc_can_encode != (can_encode != 0))
				continue;
		}
		v.emplace_back(tc.bitness, tc.code, tc.hex_bytes, tc.ip, tc.encoded_hex_bytes, tc.decoder_options, tc.test_options);
	}
}

std::vector<DecoderTestInfo> get_tests(bool include_other_tests, bool include_invalid, int can_encode) {
	std::vector<DecoderTestInfo> v;
	static constexpr std::uint32_t BITNESS_ARRAY[] = {16, 32, 64};
	for (auto bitness : BITNESS_ARRAY)
		add_tests(v, get_test_cases(bitness), include_invalid, can_encode);
	if (include_other_tests) {
		for (auto bitness : BITNESS_ARRAY)
			add_tests(v, get_misc_test_cases(bitness), include_invalid, can_encode);
		for (auto bitness : BITNESS_ARRAY)
			add_tests_mem(v, get_mem_test_cases(bitness), include_invalid, can_encode);
	}
	return v;
}

} // namespace

std::vector<DecoderTestCase> read_decoder_test_cases_file(std::uint32_t bitness, const std::string& filename) {
	std::vector<DecoderTestCase> result;
	std::uint32_t line_number = 0;
	DecoderTestCase tc;
	for (const auto& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || starts_with(line, "#"))
			continue;
		try {
			if (read_next_test_case(tc, bitness, line, line_number))
				result.push_back(tc);
		}
		catch (const std::exception& ex) {
			throw_error("Error parsing decoder test case file '" + filename + "', line " + std::to_string(line_number) + ": " + ex.what());
		}
	}
	return result;
}

std::vector<DecoderMemoryTestCase> read_decoder_mem_test_cases_file(std::uint32_t bitness, const std::string& filename) {
	std::vector<DecoderMemoryTestCase> result;
	std::uint32_t line_number = 0;
	DecoderMemoryTestCase tc;
	for (const auto& line : read_lines(filename)) {
		line_number++;
		if (line.empty() || starts_with(line, "#"))
			continue;
		try {
			if (read_next_mem_test_case(tc, bitness, line, line_number))
				result.push_back(tc);
		}
		catch (const std::exception& ex) {
			throw_error("Error parsing decoder memory test case file '" + filename + "', line " + std::to_string(line_number) + ": " +
						ex.what());
		}
	}
	return result;
}

ConstantOffsets parse_constant_offsets(std::string_view value) {
	auto parts = split(value, ';');
	if (parts.size() != 6)
		throw_error("Invalid ConstantOffsets: '" + std::string(value) + "'");
	auto immediate_offset = to_u8(parts[0]);
	auto immediate_size = to_u8(parts[1]);
	auto immediate_offset2 = to_u8(parts[2]);
	auto immediate_size2 = to_u8(parts[3]);
	auto displacement_offset = to_u8(parts[4]);
	auto displacement_size = to_u8(parts[5]);
	return ConstantOffsets(displacement_offset, displacement_size, immediate_offset, immediate_size, immediate_offset2, immediate_size2);
}

const std::vector<DecoderTestCase>& get_test_cases(std::uint32_t bitness) {
	static const std::vector<DecoderTestCase> test_cases[3] = {
		read_decoder_test_cases(16),
		read_decoder_test_cases(32),
		read_decoder_test_cases(64),
	};
	return test_cases[bitness_index(bitness)];
}

const std::vector<DecoderTestCase>& get_misc_test_cases(std::uint32_t bitness) {
	static const std::vector<DecoderTestCase> test_cases[3] = {
		read_decoder_misc_test_cases(16),
		read_decoder_misc_test_cases(32),
		read_decoder_misc_test_cases(64),
	};
	return test_cases[bitness_index(bitness)];
}

const std::vector<DecoderMemoryTestCase>& get_mem_test_cases(std::uint32_t bitness) {
	static const std::vector<DecoderMemoryTestCase> test_cases[3] = {
		read_decoder_mem_test_cases(16),
		read_decoder_mem_test_cases(32),
		read_decoder_mem_test_cases(64),
	};
	return test_cases[bitness_index(bitness)];
}

std::vector<DecoderTestInfo> decoder_tests(bool include_other_tests, bool include_invalid) { return get_tests(include_other_tests, include_invalid, -1); }

std::vector<DecoderTestInfo> encoder_tests(bool include_other_tests, bool include_invalid) { return get_tests(include_other_tests, include_invalid, 1); }

const std::unordered_set<Code>& not_decoded() {
	static const std::unordered_set<Code> values = read_code_values("Code.NotDecoded.txt");
	return values;
}

const std::unordered_set<Code>& not_decoded32_only() {
	static const std::unordered_set<Code> values = read_code_values("Code.NotDecoded32Only.txt");
	return values;
}

const std::unordered_set<Code>& not_decoded64_only() {
	static const std::unordered_set<Code> values = read_code_values("Code.NotDecoded64Only.txt");
	return values;
}

const std::unordered_set<Code>& code32_only() {
	static const std::unordered_set<Code> values = read_code_values("Code.32Only.txt");
	return values;
}

const std::unordered_set<Code>& code64_only() {
	static const std::unordered_set<Code> values = read_code_values("Code.64Only.txt");
	return values;
}

CreatedDecoder create_decoder(std::uint32_t bitness, const std::vector<std::uint8_t>& bytes, std::uint64_t ip, std::uint32_t options) {
	auto decoder = Decoder::with_ip(bitness, bytes.data(), bytes.size(), ip, options);
	std::size_t len = std::min<std::size_t>(IcedConstants::MAX_INSTRUCTION_LENGTH, bytes.size());
	return CreatedDecoder{decoder, len, len < bytes.size()};
}

} // namespace iced_x86::tests
