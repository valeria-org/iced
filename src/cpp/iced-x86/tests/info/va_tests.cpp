// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_framework.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/instruction_info.hpp"
#include "iced_x86/register.hpp"
#include "test_utils.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/va_test_parser.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using namespace iced_x86;
using namespace iced_x86::tests;

TEST_CASE("info/va_tests") {
	for (const VirtualAddressTestCase& tc : get_va_test_cases()) {
		if (tc.used_mem_index < 0)
			continue;
		const std::vector<std::uint8_t> bytes = to_vec_u8(tc.hex_bytes);
		Decoder decoder = Decoder::with_ip(tc.bitness, bytes, get_default_ip(tc.bitness), tc.decoder_options);
		const Instruction instruction = decoder.decode();

		InstructionInfoFactory factory;
		const InstructionInfo& info = factory.info(instruction);
		REQUIRE(static_cast<std::size_t>(tc.used_mem_index) < info.used_memory().size());
		const UsedMemory& used_mem = info.used_memory()[static_cast<std::size_t>(tc.used_mem_index)];

		auto get_register_value = [&tc](Register register_, std::size_t element_index, std::size_t element_size) -> std::optional<std::uint64_t> {
			for (const VARegisterValue& reg_value : tc.register_values) {
				if (reg_value.register_ == register_ && reg_value.element_index == element_index && reg_value.element_size == element_size)
					return reg_value.value;
			}
			return std::nullopt;
		};
		auto fail_register_value = [](Register, std::size_t, std::size_t) -> std::optional<std::uint64_t> { return std::nullopt; };

		const std::optional<std::uint64_t> value1 = used_mem.virtual_address(tc.element_index, get_register_value);
		CHECK(value1.has_value());
		CHECK_EQ(value1.value_or(0), tc.expected_value);

		const std::optional<std::uint64_t> value2 = used_mem.try_virtual_address(tc.element_index, get_register_value);
		CHECK(value2.has_value());
		CHECK_EQ(value2.value_or(0), tc.expected_value);

		const std::optional<std::uint64_t> value3 = used_mem.virtual_address(tc.element_index, fail_register_value);
		CHECK(!value3.has_value());

		const std::optional<std::uint64_t> value4 = used_mem.try_virtual_address(tc.element_index, fail_register_value);
		CHECK(!value4.has_value());
	}
}
