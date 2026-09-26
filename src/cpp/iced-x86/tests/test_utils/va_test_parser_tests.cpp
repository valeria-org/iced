// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_framework.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/va_test_parser.hpp"
#include "iced_x86/decoder_options.hpp"
#include "iced_x86/register.hpp"

#include <cstddef>
#include <cstdint>

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("test_utils/va_test_cases") {
	const auto& test_cases = get_va_test_cases();
	REQUIRE(!test_cases.empty());
	// Same object every time
	CHECK(&test_cases == &get_va_test_cases());

	// 16, Movsw_m16_m16, A5, 1, 1, 0, 0x7654321001242B38, , si=0x123456789ABCE5D1 ds=0x7654321001234567
	const auto& tc = test_cases[0];
	CHECK_EQ(tc.bitness, 16U);
	CHECK_EQ(tc.hex_bytes, "A5");
	CHECK_EQ(tc.decoder_options, DecoderOptions::NONE);
	CHECK_EQ(tc.operand, 1);
	CHECK_EQ(tc.used_mem_index, 1);
	CHECK_EQ(tc.element_index, 0U);
	CHECK_EQ(tc.expected_value, 0x7654321001242B38ULL);
	REQUIRE_EQ(tc.register_values.size(), 2U);
	CHECK_EQ(tc.register_values[0].register_, Register::SI);
	CHECK_EQ(tc.register_values[0].element_index, 0U);
	CHECK_EQ(tc.register_values[0].element_size, 0U);
	CHECK_EQ(tc.register_values[0].value, 0x123456789ABCE5D1ULL);
	CHECK_EQ(tc.register_values[1].register_, Register::DS);
	CHECK_EQ(tc.register_values[1].value, 0x7654321001234567ULL);

	bool found_elem_reg = false;
	bool found_mpx = false;
	for (const auto& t : test_cases) {
		if (t.decoder_options == DecoderOptions::MPX)
			found_mpx = true;
		CHECK(t.bitness == 16 || t.bitness == 32 || t.bitness == 64);
		CHECK(!to_vec_u8(t.hex_bytes).empty());
		for (const auto& rv : t.register_values) {
			CHECK(rv.register_ != Register::None);
			if (rv.element_size != 0)
				found_elem_reg = true;
		}
	}
	// There are VSIB test cases with "reg;idx;size=value" keys
	CHECK(found_elem_reg);
	CHECK(found_mpx);
}
