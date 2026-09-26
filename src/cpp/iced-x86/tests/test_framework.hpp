// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Minimal self-registering test framework (no external dependencies).
//
//	TEST_CASE("decoder/decode_16") { CHECK_EQ(1, 1); REQUIRE(x != nullptr); }
//
// - CHECK*()   records a failure and continues
// - REQUIRE*() records a failure and aborts the current test case (throws, tests are compiled with exceptions)
// - Test names should be "<component>/<name>" so they can be filtered, eg. `iced_x86_tests decoder/`

#pragma once

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace iced_x86::tests {

struct TestCaseInfo {
	const char* name;
	const char* file;
	int line;
	void (*fn)();
};

std::vector<TestCaseInfo>& get_test_cases();

struct TestRegistrar {
	TestRegistrar(const char* name, const char* file, int line, void (*fn)()) { get_test_cases().push_back(TestCaseInfo{name, file, line, fn}); }
};

struct RequireFailedException {};

void report_failure(const char* file, int line, const std::string& message);

// Converts a value to a string for failure messages
template <typename T>
std::string to_test_string(const T& value) {
	if constexpr (std::is_same_v<T, bool>)
		return value ? "true" : "false";
	else if constexpr (std::is_enum_v<T>) {
		using U = std::underlying_type_t<T>;
		std::ostringstream os;
		os << static_cast<std::uint64_t>(static_cast<U>(value));
		return os.str();
	}
	else if constexpr (std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::int8_t> || std::is_same_v<T, char>)
		return std::to_string(static_cast<int>(value));
	else if constexpr (std::is_arithmetic_v<T>) {
		std::ostringstream os;
		if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
			os << value << " (0x" << std::hex << static_cast<std::uint64_t>(value) << ")";
		else
			os << value;
		return os.str();
	}
	else if constexpr (std::is_convertible_v<T, std::string>)
		return "\"" + std::string(value) + "\"";
	else
		return "<?>";
}

template <typename A, typename B>
bool check_eq(const A& a, const B& b, const char* sa, const char* sb, const char* file, int line) {
	if (a == b)
		return true;
	report_failure(file, line, std::string("CHECK_EQ(") + sa + ", " + sb + ") failed: " + to_test_string(a) + " != " + to_test_string(b));
	return false;
}

} // namespace iced_x86::tests

#define ICED_TEST_CAT2(a, b) a##b
#define ICED_TEST_CAT(a, b) ICED_TEST_CAT2(a, b)

#define TEST_CASE(name) \
	static void ICED_TEST_CAT(iced_test_fn_, __LINE__)(); \
	static ::iced_x86::tests::TestRegistrar ICED_TEST_CAT(iced_test_reg_, __LINE__)(name, __FILE__, __LINE__, &ICED_TEST_CAT(iced_test_fn_, __LINE__)); \
	static void ICED_TEST_CAT(iced_test_fn_, __LINE__)()

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			::iced_x86::tests::report_failure(__FILE__, __LINE__, "CHECK(" #cond ") failed"); \
	} while (0)

#define CHECK_MSG(cond, msg) \
	do { \
		if (!(cond)) \
			::iced_x86::tests::report_failure(__FILE__, __LINE__, std::string("CHECK(" #cond ") failed: ") + (msg)); \
	} while (0)

#define CHECK_EQ(a, b) ((void)::iced_x86::tests::check_eq((a), (b), #a, #b, __FILE__, __LINE__))

#define REQUIRE(cond) \
	do { \
		if (!(cond)) { \
			::iced_x86::tests::report_failure(__FILE__, __LINE__, "REQUIRE(" #cond ") failed"); \
			throw ::iced_x86::tests::RequireFailedException(); \
		} \
	} while (0)

#define REQUIRE_MSG(cond, msg) \
	do { \
		if (!(cond)) { \
			::iced_x86::tests::report_failure(__FILE__, __LINE__, std::string("REQUIRE(" #cond ") failed: ") + (msg)); \
			throw ::iced_x86::tests::RequireFailedException(); \
		} \
	} while (0)

#define REQUIRE_EQ(a, b) \
	do { \
		if (!::iced_x86::tests::check_eq((a), (b), #a, #b, __FILE__, __LINE__)) \
			throw ::iced_x86::tests::RequireFailedException(); \
	} while (0)

#define FAIL(msg) \
	do { \
		::iced_x86::tests::report_failure(__FILE__, __LINE__, std::string("FAIL: ") + (msg)); \
		throw ::iced_x86::tests::RequireFailedException(); \
	} while (0)
