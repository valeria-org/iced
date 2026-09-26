// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: formatter/intel/tests/mod.rs

#include "formatter/formatter_test_utils.hpp"
#include "formatter/intel/intel_fmt_factory.hpp"
#include "test_framework.hpp"

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("formatter/intel/fmt_memalways_16") { formatter_test(16, "Intel", "MemAlways", false, intel::create_memalways); }

TEST_CASE("formatter/intel/fmt_memdefault_16") { formatter_test(16, "Intel", "MemDefault", false, intel::create_memdefault); }

TEST_CASE("formatter/intel/fmt_memminimum_16") { formatter_test(16, "Intel", "MemMinimum", false, intel::create_memminimum); }

TEST_CASE("formatter/intel/fmt_misc_16") { formatter_test(16, "Intel", "Misc", true, intel::create); }

TEST_CASE("formatter/intel/fmt_nondec_memalways_16") { formatter_test_nondec(16, "Intel", "NonDec_MemAlways", intel::create_memalways); }

TEST_CASE("formatter/intel/fmt_nondec_memdefault_16") { formatter_test_nondec(16, "Intel", "NonDec_MemDefault", intel::create_memdefault); }

TEST_CASE("formatter/intel/fmt_nondec_memminimum_16") { formatter_test_nondec(16, "Intel", "NonDec_MemMinimum", intel::create_memminimum); }

TEST_CASE("formatter/intel/fmt_memalways_32") { formatter_test(32, "Intel", "MemAlways", false, intel::create_memalways); }

TEST_CASE("formatter/intel/fmt_memdefault_32") { formatter_test(32, "Intel", "MemDefault", false, intel::create_memdefault); }

TEST_CASE("formatter/intel/fmt_memminimum_32") { formatter_test(32, "Intel", "MemMinimum", false, intel::create_memminimum); }

TEST_CASE("formatter/intel/fmt_misc_32") { formatter_test(32, "Intel", "Misc", true, intel::create); }

TEST_CASE("formatter/intel/fmt_nondec_memalways_32") { formatter_test_nondec(32, "Intel", "NonDec_MemAlways", intel::create_memalways); }

TEST_CASE("formatter/intel/fmt_nondec_memdefault_32") { formatter_test_nondec(32, "Intel", "NonDec_MemDefault", intel::create_memdefault); }

TEST_CASE("formatter/intel/fmt_nondec_memminimum_32") { formatter_test_nondec(32, "Intel", "NonDec_MemMinimum", intel::create_memminimum); }

TEST_CASE("formatter/intel/fmt_memalways_64") { formatter_test(64, "Intel", "MemAlways", false, intel::create_memalways); }

TEST_CASE("formatter/intel/fmt_memdefault_64") { formatter_test(64, "Intel", "MemDefault", false, intel::create_memdefault); }

TEST_CASE("formatter/intel/fmt_memminimum_64") { formatter_test(64, "Intel", "MemMinimum", false, intel::create_memminimum); }

TEST_CASE("formatter/intel/fmt_misc_64") { formatter_test(64, "Intel", "Misc", true, intel::create); }

TEST_CASE("formatter/intel/fmt_nondec_memalways_64") { formatter_test_nondec(64, "Intel", "NonDec_MemAlways", intel::create_memalways); }

TEST_CASE("formatter/intel/fmt_nondec_memdefault_64") { formatter_test_nondec(64, "Intel", "NonDec_MemDefault", intel::create_memdefault); }

TEST_CASE("formatter/intel/fmt_nondec_memminimum_64") { formatter_test_nondec(64, "Intel", "NonDec_MemMinimum", intel::create_memminimum); }

