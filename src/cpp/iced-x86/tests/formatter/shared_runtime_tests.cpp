// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Tests the shared formatter runtime (number formatter, register names, constant string tables, output helpers) with a minimal
// `Formatter` implementation so it's tested even if no syntax formatter is available. The shared test helpers
// (`number_tests()`, `register_tests()`) are also tested this way.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "formatter/formatter_test_utils.hpp"
#include "iced_x86/code.hpp"
#include "iced_x86/formatter.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/register.hpp"
#include "internal/formatter/fmt_common.hpp"
#include "internal/formatter/fmt_consts.hpp"
#include "internal/formatter/fmt_utils.hpp"
#include "internal/formatter/gas/fmt_data.hpp"
#include "internal/formatter/gas/regs.hpp"
#include "internal/formatter/intel/fmt_data.hpp"
#include "internal/formatter/masm/fmt_data.hpp"
#include "internal/formatter/nasm/fmt_data.hpp"
#include "internal/formatter/nasm/regs.hpp"
#include "internal/formatter/num_fmt.hpp"
#include "internal/formatter/pseudo_ops.hpp"
#include "internal/formatter/pseudo_ops_defs.hpp"
#include "internal/formatter/regs_tbl_ls.hpp"
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

namespace {
// Verifies the constant FormatterString data: the uppercase string must be the lowercase string converted to uppercase
void check_formatter_string(internal::FormatterString s, bool can_be_empty = false) {
	const std::string_view lower = s.lower();
	const std::string_view upper = s.upper();
	CHECK_EQ(lower.size(), s.len());
	REQUIRE_EQ(lower.size(), upper.size());
	CHECK_EQ(s.is_default(), lower.empty());
	if (!can_be_empty)
		CHECK(!lower.empty());
	for (std::size_t i = 0; i < lower.size(); i++) {
		const char c = lower[i];
		CHECK(!(c >= 'A' && c <= 'Z'));
		CHECK_EQ(upper[i], c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c);
	}
}

template <typename T>
void check_instr_infos(const T* infos, const char* strings) {
	for (std::size_t i = 0; i < IcedConstants::CODE_ENUM_COUNT; i++)
		check_formatter_string(internal::FormatterString(strings + infos[i].mnemonic));
}
} // namespace

TEST_CASE("formatter/shared/formatter_string_tables") {
	check_formatter_string(internal::FormatterString(), true);
	for (const auto& s : internal::get_regs_tbl())
		check_formatter_string(s, true);
	for (const auto& s : internal::gas::get_all_registers())
		check_formatter_string(s);
	for (std::size_t i = 0; i < IcedConstants::REGISTER_ENUM_COUNT; i++)
		check_formatter_string(internal::nasm::get_all_registers()[i], true);
	CHECK(internal::get_regs_tbl()[static_cast<std::size_t>(Register::ST0)].get(false) == "st(0)");
	CHECK(internal::gas::get_all_registers()[static_cast<std::size_t>(Register::ST0)].get(true) == "%ST(0)");
	CHECK(internal::nasm::get_all_registers()[static_cast<std::size_t>(Register::ST7)].get(false) == "st7");
	CHECK(internal::nasm::get_all_registers()[static_cast<std::size_t>(Register::RAX)].get(true) == "RAX");
	for (const auto& def : internal::pseudo_ops_defs::PSEUDO_OPS_DEFS) {
		const auto pseudo_ops = internal::get_pseudo_ops(def.kind);
		REQUIRE_EQ(pseudo_ops.size(), def.size);
		for (std::size_t i = 0; i < pseudo_ops.size(); i++)
			check_formatter_string(pseudo_ops[i]);
	}
	check_instr_infos(internal::gas::INSTR_INFOS, internal::gas::STRINGS);
	check_instr_infos(internal::intel::INSTR_INFOS, internal::intel::STRINGS);
	check_instr_infos(internal::masm::INSTR_INFOS, internal::masm::STRINGS);
	check_instr_infos(internal::nasm::INSTR_INFOS, internal::nasm::STRINGS);
	CHECK(internal::FormatterString(internal::gas::STRINGS + internal::gas::INSTR_INFOS[static_cast<std::size_t>(Code::Add_rm8_r8)].mnemonic)
			  .get(false) == "add");
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
	REQUIRE_EQ(ac.nasm_branch_infos[6].size(), static_cast<std::size_t>(1));
	CHECK(ac.nasm_branch_infos[6][0] == &c.short_);
	REQUIRE_EQ(ac.intel_branch_infos[1].size(), static_cast<std::size_t>(1));
	CHECK(ac.intel_branch_infos[1][0] == &c.short_);
	CHECK(ac.intel_branch_infos[0].empty());
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
	static constexpr internal::FormatterStringData<sizeof("jb")> JB("jb");
	static constexpr internal::FormatterStringData<sizeof("jc")> JC("jc");
	static constexpr internal::FormatterStringData<sizeof("jnae")> JNAE("jnae");
	const std::array<internal::FormatterString, 3> mnemonics = {internal::FormatterString(JB), internal::FormatterString(JC),
																internal::FormatterString(JNAE)};
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

TEST_CASE("formatter/shared/get_flow_control") {
	const std::pair<Code, internal::FormatterFlowControl> test_cases[] = {
		{Code::Jo_rel8_16, internal::FormatterFlowControl::ShortBranch},
		{Code::Jmp_rel8_64, internal::FormatterFlowControl::ShortBranch},
		{Code::Loop_rel8_64_RCX, internal::FormatterFlowControl::AlwaysShortBranch},
		{Code::Jrcxz_rel8_64, internal::FormatterFlowControl::AlwaysShortBranch},
		{Code::Call_rel32_64, internal::FormatterFlowControl::NearCall},
		{Code::Jne_rel32_64, internal::FormatterFlowControl::NearBranch},
		{Code::Jmpe_disp32, internal::FormatterFlowControl::NearBranch},
		{Code::Call_ptr1632, internal::FormatterFlowControl::FarCall},
		{Code::Jmp_ptr1616, internal::FormatterFlowControl::FarBranch},
		{Code::Xbegin_rel32, internal::FormatterFlowControl::Xbegin},
	};
	for (const auto& [code, flow_control] : test_cases) {
		Instruction instruction;
		instruction.set_code(code);
		CHECK_EQ(internal::get_flow_control(instruction), flow_control);
	}
	CHECK(internal::is_call(internal::FormatterFlowControl::NearCall));
	CHECK(internal::is_call(internal::FormatterFlowControl::FarCall));
	CHECK(!internal::is_call(internal::FormatterFlowControl::NearBranch));
}

TEST_CASE("formatter/shared/register_helpers") {
	CHECK_EQ(internal::r_to_r16(Register::EAX), Register::AX);
	CHECK_EQ(internal::r_to_r16(Register::R15D), Register::R15W);
	CHECK_EQ(internal::r_to_r16(Register::RSP), Register::SP);
	CHECK_EQ(internal::r_to_r16(Register::AL), Register::AL);
	CHECK_EQ(internal::r64_to_r32(Register::RAX), Register::EAX);
	CHECK_EQ(internal::r64_to_r32(Register::R15), Register::R15D);
	CHECK_EQ(internal::r64_to_r32(Register::ECX), Register::ECX);
	CHECK_EQ(internal::get_segment_register_prefix_kind(Register::ES), PrefixKind::ES);
	CHECK_EQ(internal::get_segment_register_prefix_kind(Register::GS), PrefixKind::GS);
}
