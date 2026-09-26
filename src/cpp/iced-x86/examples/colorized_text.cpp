// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

static constexpr std::uint32_t EXAMPLE_CODE_BITNESS = 64;
static constexpr std::uint64_t EXAMPLE_CODE_RIP = 0x0000'7FFA'C46A'CDA4;
static const std::uint8_t EXAMPLE_CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D,
	0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05,
	0x18, 0x57, 0x0A, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x85, 0xF0, 0x00, 0x00, 0x00, 0x4C, 0x8B,
	0x05, 0x2F, 0x24, 0x0A, 0x00, 0x48, 0x8D, 0x05, 0x78, 0x7C, 0x04, 0x00, 0x33, 0xFF,
};

// Custom formatter output that stores the output in a vector.
class MyFormatterOutput final : public FormatterOutput {
public:
	std::vector<std::pair<std::string, FormatterTextKind>> vec;

	void write(std::string_view text, FormatterTextKind kind) override {
		// This allocates a string. If that's a problem, just call printf() here
		// instead of storing the result in a vector.
		vec.emplace_back(std::string(text), kind);
	}
};

// Returns the ANSI escape sequence of the color to use (same colors as the Rust example
// which uses the `colored` crate)
static const char* get_color(FormatterTextKind kind) {
	switch (kind) {
	case FormatterTextKind::Directive:
	case FormatterTextKind::Keyword:
		return "\x1B[93m"; // bright yellow
	case FormatterTextKind::Prefix:
	case FormatterTextKind::Mnemonic:
		return "\x1B[91m"; // bright red
	case FormatterTextKind::Register:
		return "\x1B[94m"; // bright blue
	case FormatterTextKind::Number:
		return "\x1B[96m"; // bright cyan
	default:
		return "\x1B[37m"; // white
	}
}

static constexpr const char* RESET_COLOR = "\x1B[0m";

static void how_to_colorize_text() {
	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, EXAMPLE_CODE, sizeof(EXAMPLE_CODE), EXAMPLE_CODE_RIP, DecoderOptions::NONE);

	IntelFormatter formatter;
	formatter.options_mut().set_first_operand_char_index(8);
	MyFormatterOutput output;
	for (const Instruction& instruction : decoder) {
		output.vec.clear();
		// The formatter calls output.write() which will update vec with text/colors
		formatter.format(instruction, output);
		for (const auto& [text, kind] : output.vec)
			std::printf("%s%s%s", get_color(kind), text.c_str(), RESET_COLOR);
		std::printf("\n");
	}
}

int main() {
	how_to_colorize_text();
	return 0;
}
