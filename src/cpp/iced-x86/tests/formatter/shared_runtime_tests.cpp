// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Tests the shared formatter runtime (number formatter, register names, strings table, output helpers) with a minimal
// `Formatter` implementation so it's tested even if no syntax formatter is available. The shared test helpers
// (`number_tests()`, `register_tests()`) are also tested this way.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "formatter/formatter_test_utils.hpp"
#include "iced_x86/formatter.hpp"
#include "internal/formatter/fmt_common.hpp"
#include "internal/formatter/fmt_consts.hpp"
#include "internal/formatter/fmt_utils.hpp"
#include "internal/formatter/num_fmt.hpp"
#include "internal/formatter/pseudo_ops.hpp"
#include "internal/formatter/regs_tbl_ls.hpp"
#include "internal/formatter/strings_data.hpp"
#include "internal/formatter/strings_tbl.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

namespace {
// Only the number and register methods are implemented (same as the Intel formatter)
class NumberAndRegisterFormatter final : public Formatter {
public:
	NumberAndRegisterFormatter() : options_(FormatterOptions::with_intel()) {}

	void format(const Instruction&, FormatterOutput&) override { FAIL("Not implemented"); }
	const FormatterOptions& options() const noexcept override { return options_; }
	FormatterOptions& options_mut() noexcept override { return options_; }
	void format_mnemonic_options(const Instruction&, FormatterOutput&, std::uint32_t) override { FAIL("Not implemented"); }
	std::uint32_t operand_count(const Instruction&) override { FAIL("Not implemented"); }
	Result<std::optional<OpAccess>> op_access(const Instruction&, std::uint32_t) override { return IcedError("Not implemented"); }
	Result<std::optional<std::uint32_t>> get_instruction_operand(const Instruction&, std::uint32_t) override { return IcedError("Not implemented"); }
	Result<std::optional<std::uint32_t>> get_formatter_operand(const Instruction&, std::uint32_t) override { return IcedError("Not implemented"); }
	Result<void> format_operand(const Instruction&, FormatterOutput&, std::uint32_t) override { return IcedError("Not implemented"); }
	void format_operand_separator(const Instruction&, FormatterOutput&) override { FAIL("Not implemented"); }
	void format_all_operands(const Instruction&, FormatterOutput&) override { FAIL("Not implemented"); }

	std::string_view format_register(Register register_) override {
		if (options_.prefer_st0() && register_ == internal::REGISTER_ST)
			register_ = Register::ST0;
		return internal::get_regs_tbl()[static_cast<std::size_t>(register_)].get(options_.uppercase_registers() || options_.uppercase_all());
	}

	std::string_view format_i8(std::int8_t value) override { return format_i8_options(value, NumberFormattingOptions::with_immediate(options_)); }
	std::string_view format_i16(std::int16_t value) override { return format_i16_options(value, NumberFormattingOptions::with_immediate(options_)); }
	std::string_view format_i32(std::int32_t value) override { return format_i32_options(value, NumberFormattingOptions::with_immediate(options_)); }
	std::string_view format_i64(std::int64_t value) override { return format_i64_options(value, NumberFormattingOptions::with_immediate(options_)); }
	std::string_view format_u8(std::uint8_t value) override { return format_u8_options(value, NumberFormattingOptions::with_immediate(options_)); }
	std::string_view format_u16(std::uint16_t value) override { return format_u16_options(value, NumberFormattingOptions::with_immediate(options_)); }
	std::string_view format_u32(std::uint32_t value) override { return format_u32_options(value, NumberFormattingOptions::with_immediate(options_)); }
	std::string_view format_u64(std::uint64_t value) override { return format_u64_options(value, NumberFormattingOptions::with_immediate(options_)); }
	std::string_view format_i8_options(std::int8_t value, const NumberFormattingOptions& number_options) override {
		return number_formatter_.format_i8(options_, number_options, value);
	}
	std::string_view format_i16_options(std::int16_t value, const NumberFormattingOptions& number_options) override {
		return number_formatter_.format_i16(options_, number_options, value);
	}
	std::string_view format_i32_options(std::int32_t value, const NumberFormattingOptions& number_options) override {
		return number_formatter_.format_i32(options_, number_options, value);
	}
	std::string_view format_i64_options(std::int64_t value, const NumberFormattingOptions& number_options) override {
		return number_formatter_.format_i64(options_, number_options, value);
	}
	std::string_view format_u8_options(std::uint8_t value, const NumberFormattingOptions& number_options) override {
		return number_formatter_.format_u8(options_, number_options, value);
	}
	std::string_view format_u16_options(std::uint16_t value, const NumberFormattingOptions& number_options) override {
		return number_formatter_.format_u16(options_, number_options, value);
	}
	std::string_view format_u32_options(std::uint32_t value, const NumberFormattingOptions& number_options) override {
		return number_formatter_.format_u32(options_, number_options, value);
	}
	std::string_view format_u64_options(std::uint64_t value, const NumberFormattingOptions& number_options) override {
		return number_formatter_.format_u64(options_, number_options, value);
	}

private:
	FormatterOptions options_;
	internal::NumberFormatter number_formatter_;
};

std::unique_ptr<Formatter> create_numbers() {
	auto fmt = std::make_unique<NumberAndRegisterFormatter>();
	fmt->options_mut().set_uppercase_hex(true);
	fmt->options_mut().set_hex_prefix("");
	fmt->options_mut().set_hex_suffix("");
	fmt->options_mut().set_decimal_prefix("");
	fmt->options_mut().set_decimal_suffix("");
	fmt->options_mut().set_octal_prefix("");
	fmt->options_mut().set_octal_suffix("");
	fmt->options_mut().set_binary_prefix("");
	fmt->options_mut().set_binary_suffix("");
	return fmt;
}

class TestOutput final : public FormatterOutput {
public:
	std::string text;
	void write(std::string_view s, FormatterTextKind kind) override {
		text += '<';
		text += to_string(kind);
		text += ':';
		text += s;
		text += '>';
	}
};
} // namespace

TEST_CASE("formatter/shared/number_formatter") { number_tests(create_numbers); }

TEST_CASE("formatter/shared/register_names") {
	register_tests("Intel", "RegisterTests", [] { return std::unique_ptr<Formatter>(std::make_unique<NumberAndRegisterFormatter>()); });
}

TEST_CASE("formatter/shared/strings_table") {
	const auto strings = internal::get_strings_table_ref();
	REQUIRE_EQ(strings.size(), internal::strings_data::STRINGS_COUNT);
	std::size_t max_len = 0;
	for (const auto s : strings) {
		CHECK(!s.empty());
		max_len = std::max(max_len, s.size());
		for (const char c : s)
			CHECK(!(c >= 'A' && c <= 'Z'));
	}
	CHECK_EQ(max_len, internal::strings_data::MAX_STRING_LEN);
}

TEST_CASE("formatter/shared/pseudo_ops") {
	const auto& cmpps = internal::get_pseudo_ops(internal::PseudoOpsKind::cmpps);
	REQUIRE_EQ(cmpps.size(), static_cast<std::size_t>(8));
	CHECK(cmpps[0].get(false) == "cmpeqps");
	CHECK(cmpps[7].get(true) == "CMPORDPS");
	const auto& vcmpsd = internal::get_pseudo_ops(internal::PseudoOpsKind::vcmpsd);
	REQUIRE_EQ(vcmpsd.size(), static_cast<std::size_t>(32));
	CHECK(vcmpsd[31].get(false) == "vcmptrue_ussd");
	const auto& vpcmpud6 = internal::get_pseudo_ops(internal::PseudoOpsKind::vpcmpud6);
	REQUIRE_EQ(vpcmpud6.size(), static_cast<std::size_t>(8));
	CHECK(vpcmpud6[3].get(false) == "vpcmp??ud");
	const auto& pclmulqdq = internal::get_pseudo_ops(internal::PseudoOpsKind::pclmulqdq);
	REQUIRE_EQ(pclmulqdq.size(), static_cast<std::size_t>(4));
	CHECK(pclmulqdq[3].get(false) == "pclmulhqhqdq");
}

TEST_CASE("formatter/shared/formatter_constants") {
	const auto& c = internal::get_formatter_constants();
	CHECK(c.empty.is_default());
	CHECK(c.dword.get(false) == "dword");
	CHECK(c.dword.get(true) == "DWORD");
	CHECK(c.rex_w.get(true) == "REX.W");
	CHECK(c.hint_not_taken.get(true) == "HINT-NOT-TAKEN");
	CHECK(c.repe[1].get(false) == "repz");
	CHECK(c.mvex.mem_float16.get(false) == "float16");
	const auto& ac = internal::get_array_constants();
	REQUIRE_EQ(ac.dword_ptr.size(), static_cast<std::size_t>(2));
	CHECK(ac.dword_ptr[0] == &c.dword);
	CHECK(ac.dword_ptr[1] == &c.ptr);
	CHECK(ac.nasm_branch_infos[2].size() == 2);
	CHECK(ac.nasm_branch_infos[2][0] == &c.near);
	CHECK(ac.mvex_reg_mem_consts_64[10] == &c.mvex.mem_1to8);
}

TEST_CASE("formatter/shared/add_tabs") {
	struct TestCase {
		std::uint32_t column;
		std::uint32_t first_operand_char_index;
		std::uint32_t tab_size;
		const char* expected;
	};
	const TestCase test_cases[] = {
		{3, 0, 0, " "},
		{3, 8, 0, "     "},
		{3, 3, 0, " "},
		{3, 30, 0, "                           "},
		{3, 0, 4, "\t"},
		{3, 8, 4, "\t\t"},
		{3, 10, 4, "\t\t  "},
		{4, 4, 4, " "},
		{5, 6, 8, " "},
	};
	for (const auto& tc : test_cases) {
		std::string output;
		StringFormatterOutput string_output(output);
		internal::add_tabs(string_output, tc.column, tc.first_operand_char_index, tc.tab_size);
		CHECK_EQ(output, std::string(tc.expected));
	}
}

TEST_CASE("formatter/shared/formatter_output_default_methods") {
	TestOutput output;
	const Instruction instruction;
	output.write_prefix(instruction, "lock", PrefixKind::Lock);
	output.write_mnemonic(instruction, "add");
	output.write_number(instruction, 0, 0U, "12h", 0x12, NumberKind::UInt8, FormatterTextKind::LabelAddress);
	output.write_decorator(instruction, 0, std::nullopt, "sae", DecoratorKind::SuppressAllExceptions);
	output.write_register(instruction, 0, 0U, "eax", Register::EAX);
	const SymResTextPart parts[] = {SymResTextPart("a", FormatterTextKind::Data), SymResTextPart("b", FormatterTextKind::Function)};
	output.write_symbol(instruction, 0, 0U, 0x1234, SymbolResult::with_text(0x1234, SymResTextInfo::with_vec(parts, 2)));
	output.write_symbol(instruction, 0, 0U, 0x1234, SymbolResult::with_string(0x1234, "sym"));
	CHECK_EQ(output.text, std::string("<Prefix:lock><Mnemonic:add><LabelAddress:12h><Decorator:sae><Register:eax><Data:a><Function:b><Label:sym>"));
}

TEST_CASE("formatter/shared/formatter_output_methods_write") {
	internal::NumberFormatter number_formatter;
	FormatterOptions options = FormatterOptions::with_masm();
	const auto number_options = NumberFormattingOptions::with_immediate(options);
	const Instruction instruction;
	{
		TestOutput output;
		const auto symbol = SymbolResult::with_str(0x1000, "sym");
		internal::FormatterOutputMethods::write1(output, instruction, 0, 0U, options, number_formatter, number_options, 0x1010, symbol, true);
		CHECK_EQ(output.text, std::string("<Label:sym><Operator:+><Number:10h><Text: ><Punctuation:(><Number:1010h><Punctuation:)>"));
	}
	{
		TestOutput output;
		const auto symbol = SymbolResult::with_str_kind_flags(0x1010, "sym", FormatterTextKind::Data, SymbolFlags::SIGNED);
		internal::FormatterOutputMethods::write2(output, instruction, 0, 0U, options, number_formatter, number_options, 0x1000, symbol, false, true, true);
		CHECK_EQ(output.text, std::string("<Operator:-><Data:sym><Text: ><Operator:+><Text: ><Number:10h>"));
	}
	{
		TestOutput output;
		const auto symbol = SymbolResult::with_str(0x1010, "sym");
		internal::FormatterOutputMethods::write2(output, instruction, 0, 0U, options, number_formatter, number_options, 0x1000, symbol, false, true,
												 false);
		CHECK_EQ(output.text, std::string("<Label:sym><Operator:-><Number:10h>"));
	}
}

TEST_CASE("formatter/shared/get_mnemonic_cc") {
	FormatterOptions options;
	const std::vector<internal::FormatterString> mnemonics = {internal::FormatterString("jb"), internal::FormatterString("jc"),
															   internal::FormatterString("jnae")};
	CHECK(internal::get_mnemonic_cc(options, 2, mnemonics).get(false) == "jb");
	options.set_cc_b(CC_b::c);
	CHECK(internal::get_mnemonic_cc(options, 2, mnemonics).get(false) == "jc");
	options.set_cc_b(CC_b::nae);
	CHECK(internal::get_mnemonic_cc(options, 2, mnemonics).get(false) == "jnae");
}

TEST_CASE("formatter/shared/symbol_result_to_owned") {
	std::string s1 = "abc";
	std::string s2 = "def";
	const SymResTextPart parts[] = {SymResTextPart(std::string_view(s1), FormatterTextKind::Data), SymResTextPart(std::string_view(s2), FormatterTextKind::Text)};
	const auto symbol = SymbolResult::with_text_flags_size(0x1234, SymResTextInfo::with_vec(parts, 2), SymbolFlags::RELATIVE, MemorySize::UInt32);
	std::vector<SymResTextPart> vec;
	const auto owned = symbol.to_owned(vec);
	s1 = "xxx";
	s2 = "yyy";
	CHECK_EQ(owned.address, 0x1234ULL);
	CHECK_EQ(owned.flags, SymbolFlags::RELATIVE);
	CHECK(owned.symbol_size == std::optional<MemorySize>(MemorySize::UInt32));
	REQUIRE(owned.text.is_text_vec());
	REQUIRE_EQ(owned.text.size(), static_cast<std::size_t>(2));
	CHECK(owned.text[0].text.as_str() == "abc");
	CHECK(owned.text[0].text.is_string());
	CHECK_EQ(owned.text[0].color, FormatterTextKind::Data);
	CHECK(owned.text[1].text.as_str() == "def");

	const auto symbol2 = SymbolResult::with_str(0x10, "sym");
	CHECK(!symbol2.text.is_text_vec());
	const auto owned2 = symbol2.to_owned(vec);
	CHECK(owned2.text.text().text.is_string());
	CHECK(owned2.text.text().text.as_str() == "sym");
	CHECK_EQ(owned2.text.text().color, FormatterTextKind::Label);
}
