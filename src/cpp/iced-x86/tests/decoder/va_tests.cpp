// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's test/va.rs (virtual address tests, they need the decoder)

#include "test_framework.hpp"
#include "test_utils.hpp"
#include "test_utils/decoder_test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/va_test_parser.hpp"

#include "iced_x86/decoder.hpp"

#include <cstdint>
#include <optional>

namespace iced_x86::tests {

TEST_CASE("decoder/va_tests") {
	for (const auto& tc : get_va_test_cases()) {
		if (tc.operand < 0)
			continue;
		auto operand = static_cast<std::uint32_t>(tc.operand);
		auto bytes = to_vec_u8(tc.hex_bytes);
		auto created = create_decoder(tc.bitness, bytes, get_default_ip(tc.bitness), tc.decoder_options);
		Instruction instruction = created.decoder.decode();

		auto get_register_value = [&tc](Register register_, std::size_t element_index, std::size_t element_size) -> std::optional<std::uint64_t> {
			for (const auto& reg_value : tc.register_values) {
				if (reg_value.register_ == register_ && reg_value.element_index == element_index && reg_value.element_size == element_size)
					return reg_value.value;
			}
			return std::nullopt;
		};
		auto no_value = [](Register, std::size_t, std::size_t) -> std::optional<std::uint64_t> { return std::nullopt; };

		auto value1 = instruction.virtual_address(operand, tc.element_index, get_register_value);
		CHECK_MSG(value1 == std::optional<std::uint64_t>(tc.expected_value), tc.hex_bytes);

		auto value2 = instruction.try_virtual_address(operand, tc.element_index, get_register_value);
		CHECK_MSG(value2 == std::optional<std::uint64_t>(tc.expected_value), tc.hex_bytes);

		auto value3 = instruction.virtual_address(operand, tc.element_index, no_value);
		CHECK(!value3.has_value());

		auto value4 = instruction.try_virtual_address(operand, tc.element_index, no_value);
		CHECK(!value4.has_value());
	}
}

} // namespace iced_x86::tests
