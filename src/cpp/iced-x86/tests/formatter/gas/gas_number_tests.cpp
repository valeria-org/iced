// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/tests/number.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/gas/gas_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/gas/number/test_numbers") { number_tests(gas::create_numbers); }
