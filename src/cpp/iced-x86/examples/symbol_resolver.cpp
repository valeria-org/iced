// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

class MySymbolResolver final : public SymbolResolver {
public:
	explicit MySymbolResolver(std::unordered_map<std::uint64_t, std::string> map) : map_(std::move(map)) {}

	std::optional<SymbolResult> symbol(const Instruction& /*instruction*/, std::uint32_t /*operand*/,
									   std::optional<std::uint32_t> /*instruction_operand*/, std::uint64_t address,
									   std::uint32_t /*address_size*/) override {
		auto it = map_.find(address);
		if (it == map_.end())
			return std::nullopt;
		// The 'address' arg is the address of the symbol and doesn't have to be identical
		// to the 'address' arg passed to symbol(). If it's different from the input
		// address, the formatter will add +N or -N, eg. '[rax+symbol+123]'
		return SymbolResult::with_str(address, it->second);
	}

private:
	std::unordered_map<std::uint64_t, std::string> map_;
};

static void how_to_resolve_symbols() {
	static const std::uint8_t bytes[] = {0x48, 0x8B, 0x8A, 0xA5, 0x5A, 0xA5, 0x5A};
	Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
	Instruction instr = decoder.decode();

	std::unordered_map<std::uint64_t, std::string> sym_map;
	sym_map.emplace(0x5AA5'5AA5, "my_data");

	std::string output;
	auto resolver = std::make_unique<MySymbolResolver>(std::move(sym_map));
	// Create a formatter that uses our symbol resolver
	MasmFormatter formatter(std::move(resolver), nullptr);

	// This will call the symbol resolver for each immediate / displacement
	// it finds in the instruction.
	formatter.format(instr, output);

	// Prints: mov rcx,[rdx+my_data]
	std::printf("%s\n", output.c_str());
}

int main() {
	how_to_resolve_symbols();
	return 0;
}
