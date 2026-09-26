// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Ported from src/rust/iced-x86/src/block_enc/tests/mod.rs

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "iced_x86/block_encoder.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/instruction.hpp"
#include "test_framework.hpp"

namespace iced_x86::tests::block_enc {

constexpr std::uint32_t DECODER_OPTIONS = DecoderOptions::NONE;

std::vector<Instruction> decode(std::uint32_t bitness, std::uint64_t rip, const std::uint8_t* data, std::size_t size, std::uint32_t options);
inline std::vector<Instruction> decode(std::uint32_t bitness, std::uint64_t rip, const std::vector<std::uint8_t>& data, std::uint32_t options) {
	return decode(bitness, rip, data.data(), data.size(), options);
}

void encode_test(std::uint32_t bitness, std::uint64_t orig_rip, const std::vector<std::uint8_t>& original_data, std::uint64_t new_rip,
	const std::vector<std::uint8_t>& new_data, std::uint32_t options, std::uint32_t decoder_options,
	const std::vector<std::uint32_t>& expected_instruction_offsets, const std::vector<RelocInfo>& expected_reloc_infos);

} // namespace iced_x86::tests::block_enc
