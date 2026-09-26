// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/intel/tests/fmt_factory.rs

#pragma once

#include <memory>

#include "iced_x86/formatter_options.hpp"
#include "iced_x86/intel_formatter.hpp"
#include "iced_x86/memory_size_options.hpp"
#include "iced_x86/symbol_resolver.hpp"

namespace iced_x86::tests::intel {

inline std::unique_ptr<IntelFormatter> create_fmt2(std::unique_ptr<SymbolResolver> symbol_resolver,
												   std::unique_ptr<FormatterOptionsProvider> options_provider) {
	auto fmt = std::make_unique<IntelFormatter>(std::move(symbol_resolver), std::move(options_provider));
	fmt->options_mut().set_uppercase_hex(false);
	fmt->options_mut().set_hex_prefix("0x");
	fmt->options_mut().set_hex_suffix("");
	fmt->options_mut().set_octal_prefix("0o");
	fmt->options_mut().set_octal_suffix("");
	fmt->options_mut().set_binary_prefix("0b");
	fmt->options_mut().set_binary_suffix("");
	fmt->options_mut().set_space_after_operand_separator(true);
	return fmt;
}

inline std::unique_ptr<IntelFormatter> create_fmt() { return create_fmt2(nullptr, nullptr); }

inline std::unique_ptr<Formatter> create_memdefault() {
	auto fmt = create_fmt();
	fmt->options_mut().set_memory_size_options(MemorySizeOptions::Default);
	fmt->options_mut().set_show_branch_size(false);
	fmt->options_mut().set_rip_relative_addresses(true);
	fmt->options_mut().set_signed_immediate_operands(false);
	fmt->options_mut().set_space_after_operand_separator(false);
	return fmt;
}

inline std::unique_ptr<Formatter> create_memalways() {
	auto fmt = create_fmt();
	fmt->options_mut().set_memory_size_options(MemorySizeOptions::Always);
	fmt->options_mut().set_show_branch_size(true);
	fmt->options_mut().set_rip_relative_addresses(false);
	fmt->options_mut().set_signed_immediate_operands(true);
	fmt->options_mut().set_space_after_operand_separator(true);
	return fmt;
}

inline std::unique_ptr<Formatter> create_memminimum() {
	auto fmt = create_fmt();
	fmt->options_mut().set_memory_size_options(MemorySizeOptions::Minimal);
	fmt->options_mut().set_show_branch_size(true);
	fmt->options_mut().set_rip_relative_addresses(false);
	fmt->options_mut().set_signed_immediate_operands(true);
	fmt->options_mut().set_space_after_operand_separator(true);
	return fmt;
}

inline std::unique_ptr<Formatter> create() {
	auto fmt = create_fmt();
	fmt->options_mut().set_memory_size_options(MemorySizeOptions::Always);
	fmt->options_mut().set_show_branch_size(true);
	fmt->options_mut().set_rip_relative_addresses(false);
	return fmt;
}

inline std::unique_ptr<Formatter> create_options() {
	auto fmt = create_fmt();
	fmt->options_mut().set_memory_size_options(MemorySizeOptions::Default);
	fmt->options_mut().set_show_branch_size(false);
	fmt->options_mut().set_rip_relative_addresses(true);
	fmt->options_mut().set_space_after_operand_separator(false);
	return fmt;
}

inline std::unique_ptr<Formatter> create_registers() { return create_fmt(); }

inline std::unique_ptr<Formatter> create_numbers() {
	auto fmt = create_fmt();
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

inline std::unique_ptr<Formatter> create_resolver(std::unique_ptr<SymbolResolver> symbol_resolver) {
	auto fmt = create_fmt2(std::move(symbol_resolver), nullptr);
	fmt->options_mut().set_memory_size_options(MemorySizeOptions::Default);
	fmt->options_mut().set_show_branch_size(false);
	fmt->options_mut().set_rip_relative_addresses(true);
	fmt->options_mut().set_space_after_operand_separator(false);
	return fmt;
}

} // namespace iced_x86::tests::intel
