// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/tests/registers.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/gas/gas_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/gas/registers/test_regs1") {
	register_tests("Gas", "RegisterTests_1", [] { return gas::create_registers(false); });
}

TEST_CASE("formatter/gas/registers/test_regs2") {
	register_tests("Gas", "RegisterTests_2", [] { return gas::create_registers(true); });
}
