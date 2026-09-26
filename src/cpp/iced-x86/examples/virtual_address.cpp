// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

// Like Rust's assert!(): also checked in release builds (unlike assert())
static void check(bool condition, const char* message) {
	if (!condition) {
		std::fprintf(stderr, "Check failed: %s\n", message);
		std::abort();
	}
}

static void how_to_get_virtual_address() {
	// add [rdi+r12*8-5AA5EDCCh],esi
	static const std::uint8_t bytes[] = {0x42, 0x01, 0xB4, 0xE7, 0x34, 0x12, 0x5A, 0xA5};
	Decoder decoder(64, bytes, DecoderOptions::NONE);
	Instruction instr = decoder.decode();

	auto va = instr.virtual_address(
		0, 0, [](Register register_, std::size_t /*element_index*/, std::size_t /*element_size*/) -> std::optional<std::uint64_t> {
			switch (register_) {
			// The base address of ES, CS, SS and DS is always 0 in 64-bit mode
			case Register::ES:
			case Register::CS:
			case Register::SS:
			case Register::DS:
				return 0;
			case Register::RDI:
				return 0x0000'0000'1000'0000;
			case Register::R12:
				return 0x0000'0004'0000'0000;
			default:
				return std::nullopt;
			}
		});
	check(va == 0x0000'001F'B55A'1234, "va == 0x0000'001F'B55A'1234");
}

int main() {
	how_to_get_virtual_address();
	return 0;
}
