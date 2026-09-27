// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// C++ only: tests of the buffer API: `format(const Instruction&, char* output, std::size_t output_size)`

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "formatter/fast/fast_test_utils.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/fast_formatter.hpp"
#include "iced_x86/iced_constants.hpp"
#include "test_framework.hpp"
#include "test_utils/decoder_test_utils.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

namespace {

constexpr char GUARD = 'G';
constexpr std::size_t GUARD_SIZE = 64;

Instruction decode(std::uint32_t bitness, const std::vector<std::uint8_t>& bytes, std::uint64_t ip = 0) {
	return Decoder::with_ip(bitness, bytes.data(), bytes.size(), ip, DecoderOptions::NONE).decode();
}

// Formats to a buffer of `size` bytes followed by guard bytes and verifies the snprintf-like semantics.
// `expected` is the whole formatted string.
template <typename TraitOptions>
void check_format_to_size(SpecializedFormatter<TraitOptions>& formatter, const Instruction& instruction, const std::string& expected,
						  std::size_t size) {
	std::vector<char> buffer(size + GUARD_SIZE, GUARD);
	const std::size_t len = formatter.format(instruction, size == 0 ? nullptr : buffer.data(), size);
	CHECK_EQ(len, expected.size());
	if (size != 0) {
		const std::size_t written = expected.size() < size ? expected.size() : size - 1;
		CHECK(std::memcmp(buffer.data(), expected.data(), written) == 0);
		CHECK(buffer[written] == '\0');
	}
	// Nothing is written after the end of the buffer
	for (std::size_t i = size; i < buffer.size(); i++)
		REQUIRE(buffer[i] == GUARD);
}

// Checks all buffer sizes 0..=len+1, MAX_FMT_INSTR_LEN (+1) and a big buffer
template <typename TraitOptions>
void check_all_sizes(SpecializedFormatter<TraitOptions>& formatter, const Instruction& instruction, const std::string& expected) {
	for (std::size_t size = 0; size <= expected.size() + 1; size++)
		check_format_to_size(formatter, instruction, expected, size);
	check_format_to_size(formatter, instruction, expected, FastFormatter::MAX_FMT_INSTR_LEN);
	check_format_to_size(formatter, instruction, expected, FastFormatter::MAX_FMT_INSTR_LEN + 1);
	check_format_to_size(formatter, instruction, expected, FastFormatter::MAX_FMT_INSTR_LEN + 2);
	check_format_to_size(formatter, instruction, expected, expected.size() + 1000);
}

// vcvtne2ps2bf16 zmm2{k5}{z},zmm6,dword bcst [rax+4h]
const std::vector<std::uint8_t> VCVTNE2PS2BF16 = {0x62, 0xF2, 0x4F, 0xDD, 0x72, 0x50, 0x01};
// mov rax,[rax+rcx*4+12345678h]
const std::vector<std::uint8_t> MOV_MEM = {0x48, 0x8B, 0x84, 0x88, 0x78, 0x56, 0x34, 0x12};

} // namespace

TEST_CASE("formatter/fast/buffer/max_fmt_instr_len") {
	static_assert(FastFormatter::MAX_FMT_INSTR_LEN == internal::fast::MAX_FMT_INSTR_LEN, "");
	static_assert(SpecializedFormatter<DefaultSpecializedFormatterTraitOptions>::MAX_FMT_INSTR_LEN == internal::fast::MAX_FMT_INSTR_LEN, "");
	CHECK_EQ(FastFormatter::MAX_FMT_INSTR_LEN, std::size_t{315});
}

TEST_CASE("formatter/fast/buffer/exact_fit") {
	const auto instr = decode(64, VCVTNE2PS2BF16);
	FastFormatter formatter;
	const std::string expected = "vcvtne2ps2bf16 zmm2{k5}{z},zmm6,dword bcst [rax+4h]";

	char big[FastFormatter::MAX_FMT_INSTR_LEN + 1];
	CHECK_EQ(formatter.format(instr, big), expected.size());
	CHECK_EQ(std::string(big), expected);

	std::vector<char> exact(expected.size() + 1, 'X');
	CHECK_EQ(formatter.format(instr, exact.data(), exact.size()), expected.size());
	CHECK_EQ(std::string(exact.data()), expected);

	char exact_array[sizeof("vcvtne2ps2bf16 zmm2{k5}{z},zmm6,dword bcst [rax+4h]")];
	CHECK_EQ(formatter.format(instr, exact_array), expected.size());
	CHECK_EQ(std::string(exact_array), expected);
}

TEST_CASE("formatter/fast/buffer/truncated") {
	const auto instr = decode(64, VCVTNE2PS2BF16);
	FastFormatter formatter;
	const std::string expected = "vcvtne2ps2bf16 zmm2{k5}{z},zmm6,dword bcst [rax+4h]";

	char small[10];
	CHECK_EQ(formatter.format(instr, small), expected.size());
	CHECK_EQ(std::string(small), "vcvtne2ps");

	char one[1] = {'X'};
	CHECK_EQ(formatter.format(instr, one), expected.size());
	CHECK(one[0] == '\0');

	CHECK_EQ(formatter.format(instr, nullptr, 0), expected.size());

	check_all_sizes(formatter, instr, expected);
}

TEST_CASE("formatter/fast/buffer/all_sizes") {
	struct TestCase {
		std::uint32_t bitness;
		std::vector<std::uint8_t> bytes;
		const char* expected;
	};
	const TestCase test_cases[] = {
		{64, VCVTNE2PS2BF16, "vcvtne2ps2bf16 zmm2{k5}{z},zmm6,dword bcst [rax+4h]"},
		{64, MOV_MEM, "mov rax,[rax+rcx*4+12345678h]"},
		// nop
		{64, {0x90}, "nop"},
		// lock xacquire add [rax],ecx
		{64, {0xF0, 0xF2, 0x01, 0x08}, "xacquire lock add [rax],ecx"},
		// mov rax,123456789ABCDEF0h
		{64, {0x48, 0xB8, 0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12}, "mov rax,123456789ABCDEF0h"},
	};
	FastFormatter formatter;
	for (const auto& tc : test_cases) {
		const auto instr = decode(tc.bitness, tc.bytes);
		check_all_sizes(formatter, instr, tc.expected);
	}
}

namespace {
// Hard coded options: space after the operand separator, hex prefix, lowercase hex, always show the memory size
struct CustomTraitOptions : SpecializedFormatterTraitOptions {
	static constexpr bool ENABLE_DB_DW_DD_DQ = false;
	static constexpr bool verify_output_has_enough_bytes_left() noexcept { return false; }
	static constexpr bool space_after_operand_separator(const FastFormatterOptions&) noexcept { return true; }
	static constexpr bool always_show_memory_size(const FastFormatterOptions&) noexcept { return true; }
	static constexpr bool uppercase_hex(const FastFormatterOptions&) noexcept { return false; }
	static constexpr bool use_hex_prefix(const FastFormatterOptions&) noexcept { return true; }
};

// Same but the options can be changed at runtime + a symbol resolver
struct CustomSymResTraitOptions : SpecializedFormatterTraitOptions {
	static constexpr bool ENABLE_SYMBOL_RESOLVER = true;
	static constexpr bool space_after_operand_separator(const FastFormatterOptions& options) noexcept {
		return options.space_after_operand_separator();
	}
	static constexpr bool show_symbol_address(const FastFormatterOptions& options) noexcept { return options.show_symbol_address(); }
};

class LongSymbolResolver final : public SymbolResolver {
public:
	explicit LongSymbolResolver(std::size_t len) : len_(len) {}

	std::optional<SymbolResult> symbol(const Instruction&, std::uint32_t operand, std::optional<std::uint32_t>, std::uint64_t address,
									   std::uint32_t) override {
		// A new string each time: the previous result's borrowed string is invalid after this call
		buffer_.assign(len_, static_cast<char>('a' + operand));
		if (len_ != 0)
			buffer_.back() = 'Z';
		return SymbolResult::with_str(address, buffer_);
	}

private:
	std::size_t len_;
	std::string buffer_;
};
} // namespace

TEST_CASE("formatter/fast/buffer/specialized_custom_options") {
	using MyFormatter = SpecializedFormatter<CustomTraitOptions>;
	MyFormatter formatter;
	// Hard coded options can't be changed at runtime
	formatter.options_mut().set_space_after_operand_separator(false);
	formatter.options_mut().set_use_hex_prefix(false);

	const auto instr1 = decode(64, VCVTNE2PS2BF16);
	const std::string expected1 = "vcvtne2ps2bf16 zmm2{k5}{z}, zmm6, dword bcst [rax+0x4]";
	char buffer[MyFormatter::MAX_FMT_INSTR_LEN + 1];
	CHECK_EQ(formatter.format(instr1, buffer), expected1.size());
	CHECK_EQ(std::string(buffer), expected1);
	check_all_sizes(formatter, instr1, expected1);

	const auto instr2 = decode(64, MOV_MEM);
	const std::string expected2 = "mov rax, qword ptr [rax+rcx*4+0x12345678]";
	CHECK_EQ(formatter.format(instr2, buffer), expected2.size());
	CHECK_EQ(std::string(buffer), expected2);
	check_all_sizes(formatter, instr2, expected2);

	std::string str("x");
	formatter.format(instr2, str);
	CHECK_EQ(str, "x" + expected2);
}

TEST_CASE("formatter/fast/buffer/specialized_custom_options_symres") {
	using MyFormatter = SpecializedFormatter<CustomSymResTraitOptions>;
	const auto instr = decode(64, MOV_MEM);

	MyFormatter formatter1;
	CHECK_EQ(fast_format(formatter1, instr), "mov rax,[rax+rcx*4+0x12345678]");
	formatter1.options_mut().set_space_after_operand_separator(true);
	CHECK_EQ(fast_format(formatter1, instr), "mov rax, [rax+rcx*4+0x12345678]");

	auto formatter2 = MyFormatter::try_with_options(std::make_unique<LongSymbolResolver>(5)).value();
	CHECK_EQ(fast_format(formatter2, instr), "mov rax,[rax+rcx*4+bbbbZ]");
	formatter2.options_mut().set_show_symbol_address(true);
	check_all_sizes(formatter2, instr, "mov rax,[rax+rcx*4+bbbbZ (0x12345678)]");
}

TEST_CASE("formatter/fast/buffer/long_symbols") {
	struct TestCase {
		std::uint32_t bitness;
		std::vector<std::uint8_t> bytes;
		// Expected string, `@0`/`@1` is replaced with the symbol of operand 0/1
		const char* expected;
	};
	const TestCase test_cases[] = {
		// mov [rax+12345678h],rcx
		{64, {0x48, 0x89, 0x88, 0x78, 0x56, 0x34, 0x12}, "mov [rax+@0],rcx"},
		// mov ecx,12345678h
		{64, {0xB9, 0x78, 0x56, 0x34, 0x12}, "mov ecx,offset @1"},
		// jmp near (ip = 0)
		{64, {0xE9, 0x78, 0x56, 0x34, 0x12}, "jmp @0"},
		// call far 1234h:5678h (the resolver is called for the offset, the selector and the offset again)
		{16, {0x9A, 0x78, 0x56, 0x34, 0x12}, "call @1:@0"},
		// call far 12345678h:9ABCh
		{32, {0x9A, 0x78, 0x56, 0x34, 0x12, 0xBC, 0x9A}, "call @1:@0"},
	};
	const std::size_t symbol_lens[] = {0, 1, 2, 20, 100, FastFormatter::MAX_FMT_INSTR_LEN - 1, FastFormatter::MAX_FMT_INSTR_LEN,
									   FastFormatter::MAX_FMT_INSTR_LEN + 1, 1000};
	for (const auto& tc : test_cases) {
		const auto instr = decode(tc.bitness, tc.bytes);
		for (const std::size_t symbol_len : symbol_lens) {
			std::string symbols[2];
			for (std::size_t i = 0; i < 2; i++) {
				symbols[i].assign(symbol_len, static_cast<char>('a' + i));
				if (symbol_len != 0)
					symbols[i].back() = 'Z';
			}
			std::string expected;
			for (const char* p = tc.expected; *p != '\0'; p++) {
				if (*p == '@') {
					p++;
					expected += symbols[*p - '0'];
				}
				else
					expected += *p;
			}
			auto formatter = FastFormatter::try_with_options(std::make_unique<LongSymbolResolver>(symbol_len)).value();
			CHECK_EQ(fast_format(formatter, instr), expected);
			check_all_sizes(formatter, instr, expected);
		}
	}
}

namespace {
struct UncheckedTraitOptions : SpecializedFormatterTraitOptions {
	static constexpr bool ENABLE_DB_DW_DD_DQ = true;
	static constexpr bool verify_output_has_enough_bytes_left() noexcept { return false; }
	static constexpr bool use_pseudo_ops(const FastFormatterOptions&) noexcept { return true; }
	static constexpr bool show_symbol_address(const FastFormatterOptions&) noexcept { return true; }
	static constexpr bool always_show_segment_register(const FastFormatterOptions&) noexcept { return true; }
	static constexpr bool always_show_memory_size(const FastFormatterOptions&) noexcept { return true; }
};
} // namespace

TEST_CASE("formatter/fast/buffer/fast_path_doesnt_write_past_the_end") {
	// All test instructions are formatted with verify_output_has_enough_bytes_left() == true and a
	// MAX_FMT_INSTR_LEN + 1 byte buffer (fast_format()), this test also uses guard bytes and the unchecked formatter.
	SpecializedFormatter<UncheckedTraitOptions> unchecked;
	std::vector<char> big(10000);
	for (const auto& tc : decoder_tests(true, true)) {
		const auto bytes = to_vec_u8(tc.hex_bytes());
		const auto instr = Decoder::with_ip(tc.bitness(), bytes.data(), bytes.size(), tc.ip(), tc.decoder_options()).decode();
		const std::size_t len = unchecked.format(instr, big.data(), big.size());
		REQUIRE(len + 20 <= FastFormatter::MAX_FMT_INSTR_LEN);
		check_format_to_size(unchecked, instr, std::string(big.data(), len), FastFormatter::MAX_FMT_INSTR_LEN + 1);
	}
}
