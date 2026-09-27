// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

// Hide the members of SpecializedFormatterTraitOptions that you want to change
struct MyTraitOptions : SpecializedFormatterTraitOptions {
	// If you never create a db/dw/dd/dq 'instruction', we don't need this feature.
	static constexpr bool ENABLE_DB_DW_DD_DQ = false;
	// For a few percent faster code, you can also hide `verify_output_has_enough_bytes_left()` and return `false`
	// static constexpr bool verify_output_has_enough_bytes_left() noexcept { return false; }
};
using MyFormatter = SpecializedFormatter<MyTraitOptions>;

static void how_to_disassemble_really_fast() {
	// Assume this is a big array and not just one instruction
	static const std::uint8_t bytes[] = {0x62, 0xF2, 0x4F, 0xDD, 0x72, 0x50, 0x01};
	Decoder decoder(64, bytes, DecoderOptions::NONE);

	// The formatter writes to a caller provided buffer (it never allocates memory). If there's no symbol
	// resolver, the formatted instruction is never longer than MAX_FMT_INSTR_LEN chars (+ a NUL char).
	char output[MyFormatter::MAX_FMT_INSTR_LEN + 1];
	Instruction instruction;
	MyFormatter formatter;
	while (decoder.can_decode()) {
		decoder.decode_out(instruction);
		// Returns the length of the formatted instruction, `output` is NUL terminated
		const std::size_t len = formatter.format(instruction, output);
		// do something with 'output' here, eg.:
		std::printf("%s (%zu chars)\n", output, len);
	}

	// If the buffer is too small, the output is truncated (and NUL terminated) and it returns the length of the
	// whole formatted instruction (same as snprintf())
	char small[16];
	const std::size_t len = formatter.format(instruction, small);
	std::printf("%s (%zu chars)\n", small, len);
}

int main() {
	how_to_disassemble_really_fast();
	return 0;
}
