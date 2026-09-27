// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Disassembling on an embedded device: no heap allocations, small stack, fixed size buffers.
//
// - The decoder and all tables are constant data: nothing is allocated or initialized at runtime
// - `FastFormatter` formats to a caller provided char buffer and never allocates memory
// - The other formatters (gas/intel/masm/nasm) can write to your own `FormatterOutput`, eg. a fixed size buffer.
//   They allocate ~100 bytes when they're created (their number formatter) but nothing when formatting.
//
// This example counts all calls to `operator new` to show that nothing is allocated while decoding and formatting.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string_view>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

static std::size_t heap_allocations = 0;
void* operator new(std::size_t size) {
	heap_allocations++;
	if (void* p = std::malloc(size != 0 ? size : 1))
		return p;
	std::abort();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

// A `FormatterOutput` that writes to a fixed size char buffer (the text is truncated if it doesn't fit)
class FixedBufferOutput final : public FormatterOutput {
public:
	void clear() noexcept {
		len_ = 0;
		buffer_[0] = '\0';
	}
	const char* c_str() const noexcept { return buffer_; }

	void write(std::string_view text, FormatterTextKind kind) override {
		static_cast<void>(kind);
		const std::size_t n = text.size() < sizeof(buffer_) - 1 - len_ ? text.size() : sizeof(buffer_) - 1 - len_;
		std::memcpy(buffer_ + len_, text.data(), n);
		len_ += n;
		buffer_[len_] = '\0';
	}

private:
	char buffer_[128] = {};
	std::size_t len_ = 0;
};

// The code to disassemble (could also be read from flash, a debug interface, etc.)
static const std::uint8_t CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x48, 0x8D, 0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81,
	0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05, 0x18, 0x57, 0x0A, 0x00, 0xC5, 0xF8, 0x10, 0x44,
	0x24, 0x20, 0xE8, 0x10, 0x00, 0x00, 0x00, 0x74, 0xF0,
};
static constexpr std::uint64_t CODE_RIP = 0x7FF7'1FF3'2800;

int main() {
	// Everything can be created on the stack or statically: FastFormatter is 32 bytes, Decoder ~300 bytes,
	// MasmFormatter ~400 bytes, and the output buffers are as big as you want them to be. They're static here so
	// they don't use any stack.
	static FastFormatter fast_formatter;
	static MasmFormatter masm_formatter;
	static FixedBufferOutput masm_output;
	static char fast_output[FastFormatter::MAX_FMT_INSTR_LEN + 1];

	// Change some options (they're stored in the formatter, no allocation)
	masm_formatter.options_mut().set_first_operand_char_index(8);
	fast_formatter.options_mut().set_space_after_operand_separator(true);

	const std::size_t allocations_before = heap_allocations;

	Decoder decoder = Decoder::with_ip(64, CODE, CODE_RIP, DecoderOptions::NONE);
	Instruction instruction;
	while (decoder.can_decode()) {
		decoder.decode_out(instruction);

		fast_formatter.format(instruction, fast_output);

		masm_output.clear();
		masm_formatter.format(instruction, masm_output);

		std::printf("%016llX %-40s %s\n", static_cast<unsigned long long>(instruction.ip()), fast_output, masm_output.c_str());
	}

	std::printf("Heap allocations while decoding and formatting: %zu\n", heap_allocations - allocations_before);
	return heap_allocations == allocations_before ? 0 : 1;
}
