// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/tests/symres.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/gas/gas_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/gas/symres/symres") { symbol_resolver_test("Gas", "SymbolResolverTests", gas::create_resolver); }
