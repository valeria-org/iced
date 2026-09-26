// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/intel/tests/symres.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/intel/intel_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/intel/symres/symres") { symbol_resolver_test("Intel", "SymbolResolverTests", intel::create_resolver); }
