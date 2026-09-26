// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/nasm/tests/mod.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/nasm/nasm_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/nasm/fmt_memalways_16") { formatter_test(16, "Nasm", "MemAlways", false, nasm::create_memalways); }

TEST_CASE("formatter/nasm/fmt_memdefault_16") { formatter_test(16, "Nasm", "MemDefault", false, nasm::create_memdefault); }

TEST_CASE("formatter/nasm/fmt_memminimum_16") { formatter_test(16, "Nasm", "MemMinimum", false, nasm::create_memminimum); }

TEST_CASE("formatter/nasm/fmt_misc_16") { formatter_test(16, "Nasm", "Misc", true, nasm::create); }

TEST_CASE("formatter/nasm/fmt_nondec_memalways_16") { formatter_test_nondec(16, "Nasm", "NonDec_MemAlways", nasm::create_memalways); }

TEST_CASE("formatter/nasm/fmt_nondec_memdefault_16") { formatter_test_nondec(16, "Nasm", "NonDec_MemDefault", nasm::create_memdefault); }

TEST_CASE("formatter/nasm/fmt_nondec_memminimum_16") { formatter_test_nondec(16, "Nasm", "NonDec_MemMinimum", nasm::create_memminimum); }

TEST_CASE("formatter/nasm/fmt_memalways_32") { formatter_test(32, "Nasm", "MemAlways", false, nasm::create_memalways); }

TEST_CASE("formatter/nasm/fmt_memdefault_32") { formatter_test(32, "Nasm", "MemDefault", false, nasm::create_memdefault); }

TEST_CASE("formatter/nasm/fmt_memminimum_32") { formatter_test(32, "Nasm", "MemMinimum", false, nasm::create_memminimum); }

TEST_CASE("formatter/nasm/fmt_misc_32") { formatter_test(32, "Nasm", "Misc", true, nasm::create); }

TEST_CASE("formatter/nasm/fmt_nondec_memalways_32") { formatter_test_nondec(32, "Nasm", "NonDec_MemAlways", nasm::create_memalways); }

TEST_CASE("formatter/nasm/fmt_nondec_memdefault_32") { formatter_test_nondec(32, "Nasm", "NonDec_MemDefault", nasm::create_memdefault); }

TEST_CASE("formatter/nasm/fmt_nondec_memminimum_32") { formatter_test_nondec(32, "Nasm", "NonDec_MemMinimum", nasm::create_memminimum); }

TEST_CASE("formatter/nasm/fmt_memalways_64") { formatter_test(64, "Nasm", "MemAlways", false, nasm::create_memalways); }

TEST_CASE("formatter/nasm/fmt_memdefault_64") { formatter_test(64, "Nasm", "MemDefault", false, nasm::create_memdefault); }

TEST_CASE("formatter/nasm/fmt_memminimum_64") { formatter_test(64, "Nasm", "MemMinimum", false, nasm::create_memminimum); }

TEST_CASE("formatter/nasm/fmt_misc_64") { formatter_test(64, "Nasm", "Misc", true, nasm::create); }

TEST_CASE("formatter/nasm/fmt_nondec_memalways_64") { formatter_test_nondec(64, "Nasm", "NonDec_MemAlways", nasm::create_memalways); }

TEST_CASE("formatter/nasm/fmt_nondec_memdefault_64") { formatter_test_nondec(64, "Nasm", "NonDec_MemDefault", nasm::create_memdefault); }

TEST_CASE("formatter/nasm/fmt_nondec_memminimum_64") { formatter_test_nondec(64, "Nasm", "NonDec_MemMinimum", nasm::create_memminimum); }
