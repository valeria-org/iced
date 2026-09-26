// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/gas/tests/mod.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/gas/gas_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/gas/fmt_forcesuffix_16") { formatter_test(16, "Gas", "ForceSuffix", false, gas::create_forcesuffix); }

TEST_CASE("formatter/gas/fmt_nosuffix_16") { formatter_test(16, "Gas", "NoSuffix", false, gas::create_nosuffix); }

TEST_CASE("formatter/gas/fmt_misc_16") { formatter_test(16, "Gas", "Misc", true, gas::create); }

TEST_CASE("formatter/gas/fmt_nondec_forcesuffix_16") { formatter_test_nondec(16, "Gas", "NonDec_ForceSuffix", gas::create_forcesuffix); }

TEST_CASE("formatter/gas/fmt_nondec_nosuffix_16") { formatter_test_nondec(16, "Gas", "NonDec_NoSuffix", gas::create_nosuffix); }

TEST_CASE("formatter/gas/fmt_forcesuffix_32") { formatter_test(32, "Gas", "ForceSuffix", false, gas::create_forcesuffix); }

TEST_CASE("formatter/gas/fmt_nosuffix_32") { formatter_test(32, "Gas", "NoSuffix", false, gas::create_nosuffix); }

TEST_CASE("formatter/gas/fmt_misc_32") { formatter_test(32, "Gas", "Misc", true, gas::create); }

TEST_CASE("formatter/gas/fmt_nondec_forcesuffix_32") { formatter_test_nondec(32, "Gas", "NonDec_ForceSuffix", gas::create_forcesuffix); }

TEST_CASE("formatter/gas/fmt_nondec_nosuffix_32") { formatter_test_nondec(32, "Gas", "NonDec_NoSuffix", gas::create_nosuffix); }

TEST_CASE("formatter/gas/fmt_forcesuffix_64") { formatter_test(64, "Gas", "ForceSuffix", false, gas::create_forcesuffix); }

TEST_CASE("formatter/gas/fmt_nosuffix_64") { formatter_test(64, "Gas", "NoSuffix", false, gas::create_nosuffix); }

TEST_CASE("formatter/gas/fmt_misc_64") { formatter_test(64, "Gas", "Misc", true, gas::create); }

TEST_CASE("formatter/gas/fmt_nondec_forcesuffix_64") { formatter_test_nondec(64, "Gas", "NonDec_ForceSuffix", gas::create_forcesuffix); }

TEST_CASE("formatter/gas/fmt_nondec_nosuffix_64") { formatter_test_nondec(64, "Gas", "NonDec_NoSuffix", gas::create_nosuffix); }

