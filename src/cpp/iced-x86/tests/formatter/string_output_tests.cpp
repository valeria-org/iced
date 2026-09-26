// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// format(const Instruction&, std::string&) must produce the same output as format(const Instruction&, FormatterOutput&),
// also when the text doesn't fit in the formatter's initial string buffer (long symbol names).

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "iced_x86/decoder.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/formatter.hpp"
#include "iced_x86/formatter_output.hpp"
#include "iced_x86/gas_formatter.hpp"
#include "iced_x86/intel_formatter.hpp"
#include "iced_x86/masm_formatter.hpp"
#include "iced_x86/nasm_formatter.hpp"
#include "iced_x86/symbol_resolver.hpp"
#include "test_framework.hpp"

using namespace iced_x86;

namespace {
class LongSymbolResolver final : public SymbolResolver {
public:
	explicit LongSymbolResolver(std::size_t length) : name_(length, 'x') {}

	std::optional<SymbolResult> symbol(const Instruction& instruction, std::uint32_t operand, std::optional<std::uint32_t> instruction_operand,
									   std::uint64_t address, std::uint32_t address_size) override {
		static_cast<void>(instruction);
		static_cast<void>(operand);
		static_cast<void>(instruction_operand);
		static_cast<void>(address_size);
		return SymbolResult::with_str(address, name_);
	}

private:
	std::string name_;
};

std::vector<std::unique_ptr<Formatter>> create_formatters(std::size_t symbol_length) {
	std::vector<std::unique_ptr<Formatter>> formatters;
	formatters.push_back(std::make_unique<GasFormatter>(std::make_unique<LongSymbolResolver>(symbol_length), nullptr));
	formatters.push_back(std::make_unique<IntelFormatter>(std::make_unique<LongSymbolResolver>(symbol_length), nullptr));
	formatters.push_back(std::make_unique<MasmFormatter>(std::make_unique<LongSymbolResolver>(symbol_length), nullptr));
	formatters.push_back(std::make_unique<NasmFormatter>(std::make_unique<LongSymbolResolver>(symbol_length), nullptr));
	return formatters;
}
} // namespace

TEST_CASE("formatter/string_output/same_as_formatter_output") {
	// mov rcx,[rdx+5AA55AA5h] ; call far 1234h:5678h (16-bit: 9A) ; add al,5
	const std::uint8_t bytes64[] = {0x48, 0x8B, 0x8A, 0xA5, 0x5A, 0xA5, 0x5A, 0x04, 0x05, 0xE8, 0x00, 0x00, 0x00, 0x00};
	const std::uint8_t bytes16[] = {0x9A, 0x78, 0x56, 0x34, 0x12};
	for (const std::size_t symbol_length : {std::size_t{1}, std::size_t{200}, std::size_t{300}, std::size_t{5000}}) {
		auto formatters = create_formatters(symbol_length);
		for (int bitness : {16, 64}) {
			const std::uint8_t* data = bitness == 64 ? bytes64 : bytes16;
			const std::size_t size = bitness == 64 ? sizeof(bytes64) : sizeof(bytes16);
			Decoder decoder(static_cast<std::uint32_t>(bitness), data, size, DecoderOptions::NONE);
			while (decoder.can_decode()) {
				const Instruction instruction = decoder.decode();
				for (auto& formatter : formatters) {
					std::string expected("prefix:");
					StringFormatterOutput output(expected);
					formatter->format(instruction, output);

					// Twice: the buffer is reused
					for (int i = 0; i < 2; i++) {
						std::string actual("prefix:");
						formatter->format(instruction, actual);
						CHECK_EQ(actual, expected);
					}
					CHECK(expected.size() > symbol_length);
				}
			}
		}
	}
}
