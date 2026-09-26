// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/intel/tests/number.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/intel/intel_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/intel/number/test_numbers") { number_tests(intel::create_numbers); }
