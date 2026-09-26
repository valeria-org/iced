// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/intel/tests/options.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/intel/intel_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/intel/options/test_options_common") { test_format_file_common("Intel", "OptionsResult.Common", intel::create_options); }

TEST_CASE("formatter/intel/options/test_options_all") { test_format_file_all("Intel", "OptionsResult", intel::create_options); }

TEST_CASE("formatter/intel/options/test_options2") { test_format_file("Intel", "OptionsResult2", "Options2", intel::create_options); }
