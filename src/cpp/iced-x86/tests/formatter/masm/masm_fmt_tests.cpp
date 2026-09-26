// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/masm/tests/mod.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/masm/masm_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/masm/fmt_memalways_16") { formatter_test(16, "Masm", "MemAlways", false, masm::create_memalways); }

TEST_CASE("formatter/masm/fmt_memdefault_16") { formatter_test(16, "Masm", "MemDefault", false, masm::create_memdefault); }

TEST_CASE("formatter/masm/fmt_memminimum_16") { formatter_test(16, "Masm", "MemMinimum", false, masm::create_memminimum); }

TEST_CASE("formatter/masm/fmt_misc_16") { formatter_test(16, "Masm", "Misc", true, masm::create); }

TEST_CASE("formatter/masm/fmt_nondec_memalways_16") { formatter_test_nondec(16, "Masm", "NonDec_MemAlways", masm::create_memalways); }

TEST_CASE("formatter/masm/fmt_nondec_memdefault_16") { formatter_test_nondec(16, "Masm", "NonDec_MemDefault", masm::create_memdefault); }

TEST_CASE("formatter/masm/fmt_nondec_memminimum_16") { formatter_test_nondec(16, "Masm", "NonDec_MemMinimum", masm::create_memminimum); }

TEST_CASE("formatter/masm/fmt_memalways_32") { formatter_test(32, "Masm", "MemAlways", false, masm::create_memalways); }

TEST_CASE("formatter/masm/fmt_memdefault_32") { formatter_test(32, "Masm", "MemDefault", false, masm::create_memdefault); }

TEST_CASE("formatter/masm/fmt_memminimum_32") { formatter_test(32, "Masm", "MemMinimum", false, masm::create_memminimum); }

TEST_CASE("formatter/masm/fmt_misc_32") { formatter_test(32, "Masm", "Misc", true, masm::create); }

TEST_CASE("formatter/masm/fmt_nondec_memalways_32") { formatter_test_nondec(32, "Masm", "NonDec_MemAlways", masm::create_memalways); }

TEST_CASE("formatter/masm/fmt_nondec_memdefault_32") { formatter_test_nondec(32, "Masm", "NonDec_MemDefault", masm::create_memdefault); }

TEST_CASE("formatter/masm/fmt_nondec_memminimum_32") { formatter_test_nondec(32, "Masm", "NonDec_MemMinimum", masm::create_memminimum); }

TEST_CASE("formatter/masm/fmt_memalways_64") { formatter_test(64, "Masm", "MemAlways", false, masm::create_memalways); }

TEST_CASE("formatter/masm/fmt_memdefault_64") { formatter_test(64, "Masm", "MemDefault", false, masm::create_memdefault); }

TEST_CASE("formatter/masm/fmt_memminimum_64") { formatter_test(64, "Masm", "MemMinimum", false, masm::create_memminimum); }

TEST_CASE("formatter/masm/fmt_misc_64") { formatter_test(64, "Masm", "Misc", true, masm::create); }

TEST_CASE("formatter/masm/fmt_nondec_memalways_64") { formatter_test_nondec(64, "Masm", "NonDec_MemAlways", masm::create_memalways); }

TEST_CASE("formatter/masm/fmt_nondec_memdefault_64") { formatter_test_nondec(64, "Masm", "NonDec_MemDefault", masm::create_memdefault); }

TEST_CASE("formatter/masm/fmt_nondec_memminimum_64") { formatter_test_nondec(64, "Masm", "NonDec_MemMinimum", masm::create_memminimum); }
