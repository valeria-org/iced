// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/tests/options.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/gas/gas_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/gas/options/test_options_common") { test_format_file_common("Gas", "OptionsResult.Common", gas::create_options); }

TEST_CASE("formatter/gas/options/test_options_all") { test_format_file_all("Gas", "OptionsResult", gas::create_options); }

TEST_CASE("formatter/gas/options/test_options2") { test_format_file("Gas", "OptionsResult2", "Options2", gas::create_options); }
