// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#pragma once

namespace iced_x86::tests {

/// A (name, value) pair. The generated test tables (`tests/generated/from_str_conv_tables.hpp`,
/// `tests/generated/test_dicts.hpp`) are `std::array<NameValue<T>, N>` constants.
template <typename T>
struct NameValue {
	const char* name;
	T value;
};

} // namespace iced_x86::tests
