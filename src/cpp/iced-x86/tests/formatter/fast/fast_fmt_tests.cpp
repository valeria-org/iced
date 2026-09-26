// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/fast/tests/mod.rs

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>

#include "formatter/fast/fast_test_utils.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/fast_formatter.hpp"
#include "test_framework.hpp"
#include "test_utils/str_utils.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/fast/test_fmt_factory/fmt_default_16") {
	formatter_test_fast<DefaultFastFormatterTraitOptions>(16, "Fast", "Default", false, fast_create_default<DefaultFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_fmt_factory/fmt_inverted_16") {
	formatter_test_fast<DefaultFastFormatterTraitOptions>(16, "Fast", "Inverted", false, fast_create_inverted<DefaultFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_fmt_factory/fmt_misc_16") {
	formatter_test_fast<DefaultFastFormatterTraitOptions>(16, "Fast", "Misc", true, fast_create_default<DefaultFastFormatterTraitOptions>);
}

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_fmt_factory/fmt_nondec_default_16") {
	formatter_test_nondec_fast<DefaultFastFormatterTraitOptions>(16, "Fast", "NonDec_Default", fast_create_default<DefaultFastFormatterTraitOptions>);
}
#endif

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_fmt_factory/fmt_nondec_inverted_16") {
	formatter_test_nondec_fast<DefaultFastFormatterTraitOptions>(16, "Fast", "NonDec_Inverted", fast_create_inverted<DefaultFastFormatterTraitOptions>);
}
#endif

TEST_CASE("formatter/fast/test_fmt_factory/fmt_default_32") {
	formatter_test_fast<DefaultFastFormatterTraitOptions>(32, "Fast", "Default", false, fast_create_default<DefaultFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_fmt_factory/fmt_inverted_32") {
	formatter_test_fast<DefaultFastFormatterTraitOptions>(32, "Fast", "Inverted", false, fast_create_inverted<DefaultFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_fmt_factory/fmt_misc_32") {
	formatter_test_fast<DefaultFastFormatterTraitOptions>(32, "Fast", "Misc", true, fast_create_default<DefaultFastFormatterTraitOptions>);
}

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_fmt_factory/fmt_nondec_default_32") {
	formatter_test_nondec_fast<DefaultFastFormatterTraitOptions>(32, "Fast", "NonDec_Default", fast_create_default<DefaultFastFormatterTraitOptions>);
}
#endif

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_fmt_factory/fmt_nondec_inverted_32") {
	formatter_test_nondec_fast<DefaultFastFormatterTraitOptions>(32, "Fast", "NonDec_Inverted", fast_create_inverted<DefaultFastFormatterTraitOptions>);
}
#endif

TEST_CASE("formatter/fast/test_fmt_factory/fmt_default_64") {
	formatter_test_fast<DefaultFastFormatterTraitOptions>(64, "Fast", "Default", false, fast_create_default<DefaultFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_fmt_factory/fmt_inverted_64") {
	formatter_test_fast<DefaultFastFormatterTraitOptions>(64, "Fast", "Inverted", false, fast_create_inverted<DefaultFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_fmt_factory/fmt_misc_64") {
	formatter_test_fast<DefaultFastFormatterTraitOptions>(64, "Fast", "Misc", true, fast_create_default<DefaultFastFormatterTraitOptions>);
}

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_fmt_factory/fmt_nondec_default_64") {
	formatter_test_nondec_fast<DefaultFastFormatterTraitOptions>(64, "Fast", "NonDec_Default", fast_create_default<DefaultFastFormatterTraitOptions>);
}
#endif

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_fmt_factory/fmt_nondec_inverted_64") {
	formatter_test_nondec_fast<DefaultFastFormatterTraitOptions>(64, "Fast", "NonDec_Inverted", fast_create_inverted<DefaultFastFormatterTraitOptions>);
}
#endif

TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_default_16") {
	formatter_test_fast<NotFastFormatterTraitOptions>(16, "Fast", "Default", false, fast_create_default<NotFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_inverted_16") {
	formatter_test_fast<NotFastFormatterTraitOptions>(16, "Fast", "Inverted", false, fast_create_inverted<NotFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_misc_16") {
	formatter_test_fast<NotFastFormatterTraitOptions>(16, "Fast", "Misc", true, fast_create_default<NotFastFormatterTraitOptions>);
}

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_nondec_default_16") {
	formatter_test_nondec_fast<NotFastFormatterTraitOptions>(16, "Fast", "NonDec_Default", fast_create_default<NotFastFormatterTraitOptions>);
}
#endif

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_nondec_inverted_16") {
	formatter_test_nondec_fast<NotFastFormatterTraitOptions>(16, "Fast", "NonDec_Inverted", fast_create_inverted<NotFastFormatterTraitOptions>);
}
#endif

TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_default_32") {
	formatter_test_fast<NotFastFormatterTraitOptions>(32, "Fast", "Default", false, fast_create_default<NotFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_inverted_32") {
	formatter_test_fast<NotFastFormatterTraitOptions>(32, "Fast", "Inverted", false, fast_create_inverted<NotFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_misc_32") {
	formatter_test_fast<NotFastFormatterTraitOptions>(32, "Fast", "Misc", true, fast_create_default<NotFastFormatterTraitOptions>);
}

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_nondec_default_32") {
	formatter_test_nondec_fast<NotFastFormatterTraitOptions>(32, "Fast", "NonDec_Default", fast_create_default<NotFastFormatterTraitOptions>);
}
#endif

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_nondec_inverted_32") {
	formatter_test_nondec_fast<NotFastFormatterTraitOptions>(32, "Fast", "NonDec_Inverted", fast_create_inverted<NotFastFormatterTraitOptions>);
}
#endif

TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_default_64") {
	formatter_test_fast<NotFastFormatterTraitOptions>(64, "Fast", "Default", false, fast_create_default<NotFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_inverted_64") {
	formatter_test_fast<NotFastFormatterTraitOptions>(64, "Fast", "Inverted", false, fast_create_inverted<NotFastFormatterTraitOptions>);
}

TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_misc_64") {
	formatter_test_fast<NotFastFormatterTraitOptions>(64, "Fast", "Misc", true, fast_create_default<NotFastFormatterTraitOptions>);
}

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_nondec_default_64") {
	formatter_test_nondec_fast<NotFastFormatterTraitOptions>(64, "Fast", "NonDec_Default", fast_create_default<NotFastFormatterTraitOptions>);
}
#endif

#if ICED_X86_TESTS_HAS_ENCODER
TEST_CASE("formatter/fast/test_not_fmt_factory/fmt_nondec_inverted_64") {
	formatter_test_nondec_fast<NotFastFormatterTraitOptions>(64, "Fast", "NonDec_Inverted", fast_create_inverted<NotFastFormatterTraitOptions>);
}
#endif

TEST_CASE("formatter/fast/format_hex2") {
	// mov rax,0000_0000_0000_0000h
	const std::uint8_t bytes[] = {0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	Decoder decoder(64, bytes, sizeof(bytes), DecoderOptions::NONE);
	Instruction instr = decoder.decode();
	REQUIRE_EQ(instr.code(), Code::Mov_r64_imm64);

	// Test formatting every value 00-FF in every nibble position + upper/lower + hex prefix/suffix.
	// This test uses '0' for every 'x' or a random value.
	//			00xxxxxxxxxxxxxx-FFxxxxxxxxxxxxxx
	//			x00xxxxxxxxxxxxx-xFFxxxxxxxxxxxxx
	//			xx00xxxxxxxxxxxx-xxFFxxxxxxxxxxxx
	//			...
	//			xxxxxxxxxxxxxx00-xxxxxxxxxxxxxxFF
	std::string actual_instr;
	const std::uint64_t or_values[] = {0, 0x1234'5678'9ABC'DEF1ULL, 0xFEDC'BA98'7654'321FULL};
	for (const std::uint64_t or_value : or_values) {
		for (const bool uppercase : {false, true}) {
			for (const bool hex_prefix : {false, true}) {
				for (std::uint32_t hex_shift_index = 0; hex_shift_index < 15; hex_shift_index++) {
					const std::uint32_t hex_shift = hex_shift_index * 4;
					for (std::uint64_t hex2_value = 0; hex2_value < 0x100; hex2_value++) {
						FastFormatter formatter;
						formatter.options_mut().set_uppercase_hex(uppercase);
						formatter.options_mut().set_use_hex_prefix(hex_prefix);

						const std::uint64_t mask = 0xFFULL << hex_shift;
						const std::uint64_t imm = (hex2_value << hex_shift) | (or_value & ~mask);
						REQUIRE_EQ((imm >> hex_shift) & 0xFF, hex2_value);
						instr.set_immediate64(imm);

						char buf[32];
						std::snprintf(buf, sizeof(buf), "%llx", static_cast<unsigned long long>(imm));
						std::string expected_imm(buf);
						const char* leading_zero = !hex_prefix && expected_imm[0] >= 'a' ? "0" : "";
						if (uppercase)
							expected_imm = to_ascii_uppercase(expected_imm);
						const char* prefix = hex_prefix ? "0x" : "";
						const char* suffix = hex_prefix ? "" : "h";

						const std::string expected_instr = std::string("mov rax,") + prefix + leading_zero + expected_imm + suffix;

						actual_instr.clear();
						formatter.format(instr, actual_instr);
						REQUIRE_EQ(actual_instr, expected_instr);
					}
				}
			}
		}
	}
}

namespace {
class PanicSymbolResolver final : public SymbolResolver {
public:
	std::optional<SymbolResult> symbol(const Instruction&, std::uint32_t, std::optional<std::uint32_t>, std::uint64_t, std::uint32_t) override {
		FAIL("Unexpected call");
	}
};

struct NoSymResolverTraitOptions : SpecializedFormatterTraitOptions {
	static constexpr bool ENABLE_SYMBOL_RESOLVER = false;
};
struct SymResolverTraitOptions : SpecializedFormatterTraitOptions {
	static constexpr bool ENABLE_SYMBOL_RESOLVER = true;
};
} // namespace

TEST_CASE("formatter/fast/test_no_symresolver_try_with_options_ok1") {
	using MyFormatter = SpecializedFormatter<NoSymResolverTraitOptions>;
	CHECK(MyFormatter::try_with_options(nullptr).is_ok());
}

TEST_CASE("formatter/fast/test_no_symresolver_try_with_options_ok2") {
	using MyFormatter = SpecializedFormatter<SymResolverTraitOptions>;
	CHECK(MyFormatter::try_with_options(nullptr).is_ok());
}

TEST_CASE("formatter/fast/test_symresolver_try_with_options_err") {
	using MyFormatter = SpecializedFormatter<NoSymResolverTraitOptions>;
	CHECK(MyFormatter::try_with_options(std::make_unique<PanicSymbolResolver>()).is_err());
}

TEST_CASE("formatter/fast/test_symresolver_try_with_options_ok") {
	using MyFormatter = SpecializedFormatter<SymResolverTraitOptions>;
	CHECK(MyFormatter::try_with_options(std::make_unique<PanicSymbolResolver>()).is_ok());
}

// Rust: formatter/fast/tests/misc.rs
TEST_CASE("formatter/fast/verify_default_formatter_options") {
	FastFormatter fmt;
	const auto& options = fmt.options();
	CHECK(!options.space_after_operand_separator());
	CHECK(!options.always_show_segment_register());
	CHECK(options.uppercase_hex());
	CHECK(!options.use_hex_prefix());
	CHECK(!options.always_show_memory_size());
	CHECK(!options.rip_relative_addresses());
	CHECK(options.use_pseudo_ops());
	CHECK(!options.show_symbol_address());
}
