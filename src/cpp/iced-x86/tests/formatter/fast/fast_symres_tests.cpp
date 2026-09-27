// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/fast/tests/symres.rs

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "formatter/fast/fast_test_utils.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/fast_formatter.hpp"
#include "iced_x86/iced_constants.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/fast/symres/test_fmt_factory/symres") {
	symbol_resolver_test_fast<DefaultFastFormatterTraitOptions>("Fast", "SymbolResolverTests", fast_create_resolver<DefaultFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/symres/test_not_fmt_factory/symres") {
	symbol_resolver_test_fast<NotFastFormatterTraitOptions>("Fast", "SymbolResolverTests", fast_create_resolver<NotFastFormatterTraitOptions>);
}

namespace {
class LongSymbolResolver final : public SymbolResolver {
public:
	LongSymbolResolver(std::string symbol_result, std::uint32_t instruction_operand)
		: symbol_result_(std::move(symbol_result)), instruction_operand_(instruction_operand) {}

	std::optional<SymbolResult> symbol(const Instruction& instruction, std::uint32_t operand, std::optional<std::uint32_t> instruction_operand,
									   std::uint64_t address, std::uint32_t address_size) override {
		static_cast<void>(instruction);
		static_cast<void>(operand);
		static_cast<void>(address_size);
		if (instruction_operand == instruction_operand_)
			return SymbolResult::with_str(address, symbol_result_);
		return std::nullopt;
	}

private:
	std::string symbol_result_;
	std::uint32_t instruction_operand_;
};
} // namespace

TEST_CASE("formatter/fast/symres/test_long_symbols") {
	constexpr std::size_t MAX_SYMBOL_LEN = 1024;
	static_assert(MAX_SYMBOL_LEN > internal::fast::MAX_FMT_INSTR_LEN, "");

	const std::pair<std::uint32_t, std::vector<std::uint8_t>> test_cases[IcedConstants::MAX_OP_COUNT] = {
		// mov [rax+12345678h],rcx
		{0, {0x48, 0x89, 0x88, 0x78, 0x56, 0x34, 0x12}},
		// op1: mov rcx,[rax+12345678h]
		{1, {0x48, 0x8B, 0x88, 0x78, 0x56, 0x34, 0x12}},
		// op2: imul ecx,[rsi+12345678h],9ABCDEF1h
		{2, {0x69, 0x8E, 0x78, 0x56, 0x34, 0x12, 0xF1, 0xDE, 0xBC, 0x9A}},
		// op3: vpermil2ps xmm2,xmm6,xmm4,[rax+12345678h],1
		{3, {0xC4, 0xE3, 0xC9, 0x48, 0x90, 0x78, 0x56, 0x34, 0x12, 0x41}},
		// op4: vpermil2ps xmm2,xmm6,[rax],xmm4,1
		{4, {0xC4, 0xE3, 0x49, 0x48, 0x10, 0x41}},
	};
	for (const auto& [op, bytes] : test_cases) {
		Decoder decoder(64, bytes.data(), bytes.size(), DecoderOptions::NONE);
		const auto instr = decoder.decode();
		std::vector<char> big(MAX_SYMBOL_LEN + FastFormatter::MAX_FMT_INSTR_LEN + 1);
		for (std::size_t symbol_len = 0; symbol_len <= MAX_SYMBOL_LEN; symbol_len++) {
			const std::string symbol(symbol_len, 'a');
			// Don't re-use it, always create a new one
			auto resolver = std::make_unique<LongSymbolResolver>(symbol, op);
			auto formatter = FastFormatter::try_with_options(std::move(resolver)).value();

			// Big buffer
			const std::size_t len = formatter.format(instr, big.data(), big.size());
			REQUIRE(len >= symbol_len);
			REQUIRE(len < big.size());
			CHECK(std::strlen(big.data()) == len);
			const std::string output(big.data(), len);
			CHECK(output.find(symbol) != std::string::npos);

			// Small buffers (truncated output) and a MAX_FMT_INSTR_LEN + 1 byte buffer (truncated if it's a long symbol)
			char small[16];
			CHECK(formatter.format(instr, small) == len);
			CHECK(std::strlen(small) == std::min(len, sizeof(small) - 1));
			CHECK(output.compare(0, std::strlen(small), small) == 0);
			char buffer[FastFormatter::MAX_FMT_INSTR_LEN + 1];
			CHECK(formatter.format(instr, buffer) == len);
			CHECK(std::strlen(buffer) == std::min(len, sizeof(buffer) - 1));
			CHECK(output.compare(0, std::strlen(buffer), buffer) == 0);

			// std::string wrapper (formats it twice if it's longer than MAX_FMT_INSTR_LEN)
			std::string str("x");
			formatter.format(instr, str);
			CHECK(str == "x" + output);
		}
	}
}
