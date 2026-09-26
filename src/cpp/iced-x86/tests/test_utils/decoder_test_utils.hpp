// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Decoder test cases (src/UnitTests/Intel/Decoder/*.txt), used by the decoder, encoder, formatter and instr info tests.
// Port of Rust's decoder/tests/{test_utils,test_cases,test_parser,mem_test_parser,decoder_test_case,decoder_mem_test_case}.rs
//
// Usage:
//	for (const DecoderTestInfo& tc : decoder_tests(true, false)) {
//		auto bytes = to_vec_u8(tc.hex_bytes());
//		auto decoder = Decoder::with_ip(tc.bitness(), bytes, tc.ip(), tc.decoder_options());
//		...
//	}

#pragma once

#include "iced_x86/code.hpp"
#include "iced_x86/constant_offsets.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/decoder_error.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/memory_size.hpp"
#include "iced_x86/mnemonic.hpp"
#include "iced_x86/mvex_reg_mem_conv.hpp"
#include "iced_x86/op_kind.hpp"
#include "iced_x86/register.hpp"
#include "iced_x86/rounding_control.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace iced_x86::tests {

struct MvexDecoderInfo {
	bool eviction_hint = false;
	MvexRegMemConv reg_mem_conv = MvexRegMemConv::None;
};

/// A test case from `DecoderTest{16,32,64}.txt` / `DecoderTestMisc{16,32,64}.txt`
struct DecoderTestCase {
	std::uint32_t line_number = 0;
	std::uint32_t test_options = 0; // DecoderTestOptions
	DecoderError decoder_error = DecoderError::None;
	std::uint32_t decoder_options = 0;
	std::uint32_t bitness = 0;
	std::string hex_bytes;
	std::uint64_t ip = 0;
	std::string encoded_hex_bytes;
	Code code = Code::INVALID;
	Mnemonic mnemonic = Mnemonic::INVALID;
	std::uint32_t op_count = 0;
	bool zeroing_masking = false;
	bool suppress_all_exceptions = false;
	bool is_broadcast = false;
	bool has_xacquire_prefix = false;
	bool has_xrelease_prefix = false;
	bool has_repe_prefix = false;
	bool has_repne_prefix = false;
	bool has_lock_prefix = false;
	std::uint32_t vsib_bitness = 0;
	Register op_mask = Register::None;
	RoundingControl rounding_control = RoundingControl::None;
	OpKind op_kinds[IcedConstants::MAX_OP_COUNT] = {};
	Register segment_prefix = Register::None;
	Register memory_segment = Register::None;
	Register memory_base = Register::None;
	Register memory_index = Register::None;
	std::uint32_t memory_displ_size = 0;
	MemorySize memory_size = MemorySize::Unknown;
	std::uint32_t memory_index_scale = 0;
	std::uint64_t memory_displacement = 0;
	std::uint64_t immediate = 0;
	std::uint8_t immediate_2nd = 0;
	std::uint64_t near_branch = 0;
	std::uint32_t far_branch = 0;
	std::uint16_t far_branch_selector = 0;
	Register op_registers[IcedConstants::MAX_OP_COUNT] = {};
	ConstantOffsets constant_offsets;
	MvexDecoderInfo mvex;
};

/// A test case from `MemoryTest{16,32,64}.txt`
struct DecoderMemoryTestCase {
	std::uint32_t bitness = 0;
	std::string hex_bytes;
	std::uint64_t ip = 0;
	Code code = Code::INVALID;
	Register register_ = Register::None;
	Register prefix_segment = Register::None;
	Register segment = Register::None;
	Register base_register = Register::None;
	Register index_register = Register::None;
	std::uint32_t scale = 0;
	std::uint64_t displacement = 0;
	std::uint32_t displ_size = 0;
	ConstantOffsets constant_offsets;
	std::string encoded_hex_bytes;
	std::uint32_t decoder_options = 0;
	std::uint32_t line_number = 0;
	std::uint32_t test_options = 0; // DecoderTestOptions
};

/// Info about a decoder test case, returned by `decoder_tests()` and `encoder_tests()`
class DecoderTestInfo {
public:
	DecoderTestInfo(std::uint32_t bitness, Code code, std::string hex_bytes, std::uint64_t ip, std::string encoded_hex_bytes,
					std::uint32_t decoder_options, std::uint32_t decoder_test_options)
		: bitness_(bitness), code_(code), hex_bytes_(std::move(hex_bytes)), ip_(ip), encoded_hex_bytes_(std::move(encoded_hex_bytes)),
		  decoder_options_(decoder_options), decoder_test_options_(decoder_test_options) {}

	std::uint32_t bitness() const noexcept { return bitness_; }
	Code code() const noexcept { return code_; }
	const std::string& hex_bytes() const noexcept { return hex_bytes_; }
	std::uint64_t ip() const noexcept { return ip_; }
	const std::string& encoded_hex_bytes() const noexcept { return encoded_hex_bytes_; }
	std::uint32_t decoder_options() const noexcept { return decoder_options_; }
	/// `DecoderTestOptions` flags
	std::uint32_t decoder_test_options() const noexcept { return decoder_test_options_; }

private:
	std::uint32_t bitness_;
	Code code_;
	std::string hex_bytes_;
	std::uint64_t ip_;
	std::string encoded_hex_bytes_;
	std::uint32_t decoder_options_;
	std::uint32_t decoder_test_options_;
};

/// Parses a `DecoderTest*.txt` file. Throws `std::runtime_error` if the file can't be parsed.
std::vector<DecoderTestCase> read_decoder_test_cases_file(std::uint32_t bitness, const std::string& filename);
/// Parses a `MemoryTest*.txt` file. Throws `std::runtime_error` if the file can't be parsed.
std::vector<DecoderMemoryTestCase> read_decoder_mem_test_cases_file(std::uint32_t bitness, const std::string& filename);
/// Parses a constant offsets value (`co=imm_offs;imm_size;imm_offs2;imm_size2;displ_offs;displ_size`)
ConstantOffsets parse_constant_offsets(std::string_view value);

/// Gets all test cases in `DecoderTest{bitness}.txt` (the file is only parsed once)
const std::vector<DecoderTestCase>& get_test_cases(std::uint32_t bitness);
/// Gets all test cases in `DecoderTestMisc{bitness}.txt` (the file is only parsed once)
const std::vector<DecoderTestCase>& get_misc_test_cases(std::uint32_t bitness);
/// Gets all test cases in `MemoryTest{bitness}.txt` (the file is only parsed once)
const std::vector<DecoderMemoryTestCase>& get_mem_test_cases(std::uint32_t bitness);

/// Gets all decoder tests (16, 32 and 64-bit)
///
/// # Arguments
///
/// * `include_other_tests`: also include the misc and memory tests
/// * `include_invalid`: include the tests that decode to `Code::INVALID`
std::vector<DecoderTestInfo> decoder_tests(bool include_other_tests, bool include_invalid);
/// Same as `decoder_tests()` but only returns the tests that can be encoded (no `noencode` option)
std::vector<DecoderTestInfo> encoder_tests(bool include_other_tests, bool include_invalid);

/// `Code` values in `Code.NotDecoded.txt`
const std::unordered_set<Code>& not_decoded();
/// `Code` values in `Code.NotDecoded32Only.txt`
const std::unordered_set<Code>& not_decoded32_only();
/// `Code` values in `Code.NotDecoded64Only.txt`
const std::unordered_set<Code>& not_decoded64_only();
/// `Code` values in `Code.32Only.txt`
const std::unordered_set<Code>& code32_only();
/// `Code` values in `Code.64Only.txt`
const std::unordered_set<Code>& code64_only();

/// Result of `create_decoder()` (Rust returns a `(Decoder, usize, bool)` tuple)
struct CreatedDecoder {
	Decoder decoder;
	/// Expected instruction length: `min(MAX_INSTRUCTION_LENGTH, bytes.size())`
	std::size_t len;
	/// `true` if there are more bytes after the instruction
	bool can_read;
};

/// Creates a decoder. `bytes` must stay alive while the decoder is used.
CreatedDecoder create_decoder(std::uint32_t bitness, const std::vector<std::uint8_t>& bytes, std::uint64_t ip, std::uint32_t options);

} // namespace iced_x86::tests
