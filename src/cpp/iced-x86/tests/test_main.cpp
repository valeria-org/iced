// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_framework.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

namespace iced_x86::tests {

std::vector<TestCaseInfo>& get_test_cases() {
	static std::vector<TestCaseInfo> cases;
	return cases;
}

static std::size_t g_failures = 0;
static std::size_t g_failures_in_current_test = 0;
static const char* g_current_test = nullptr;
static constexpr std::size_t MAX_REPORTED_FAILURES_PER_TEST = 50;

void report_failure(const char* file, int line, const std::string& message) {
	g_failures++;
	g_failures_in_current_test++;
	if (g_failures_in_current_test <= MAX_REPORTED_FAILURES_PER_TEST)
		std::fprintf(stderr, "  [%s] %s:%d: %s\n", g_current_test != nullptr ? g_current_test : "?", file, line, message.c_str());
	else if (g_failures_in_current_test == MAX_REPORTED_FAILURES_PER_TEST + 1)
		std::fprintf(stderr, "  [%s] (more failures not shown)\n", g_current_test != nullptr ? g_current_test : "?");
}

} // namespace iced_x86::tests

// Usage: iced_x86_tests [-l] [filter...]
//	-l		List all tests
//	filter	Only run tests whose name contains one of the filters
int main(int argc, char** argv) {
	using namespace iced_x86::tests;
	auto cases = get_test_cases();
	std::sort(cases.begin(), cases.end(), [](const TestCaseInfo& a, const TestCaseInfo& b) { return std::strcmp(a.name, b.name) < 0; });

	std::vector<std::string> filters;
	bool list = false;
	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "-l") == 0)
			list = true;
		else
			filters.emplace_back(argv[i]);
	}

	std::size_t ran = 0, failed_tests = 0;
	auto start_all = std::chrono::steady_clock::now();
	for (const auto& tc : cases) {
		if (!filters.empty()) {
			bool match = false;
			for (const auto& f : filters) {
				if (std::string(tc.name).find(f) != std::string::npos) {
					match = true;
					break;
				}
			}
			if (!match)
				continue;
		}
		if (list) {
			std::printf("%s\n", tc.name);
			continue;
		}
		ran++;
		g_current_test = tc.name;
		g_failures_in_current_test = 0;
		auto start = std::chrono::steady_clock::now();
		try {
			tc.fn();
		}
		catch (const RequireFailedException&) {
		}
		catch (const std::exception& ex) {
			report_failure(tc.file, tc.line, std::string("Unexpected exception: ") + ex.what());
		}
		catch (...) {
			report_failure(tc.file, tc.line, "Unexpected exception");
		}
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		if (g_failures_in_current_test != 0) {
			failed_tests++;
			std::printf("FAIL %s (%zu failures, %lld ms)\n", tc.name, g_failures_in_current_test, static_cast<long long>(ms));
		}
		else if (ms >= 1000)
			std::printf("ok   %s (%lld ms)\n", tc.name, static_cast<long long>(ms));
	}
	if (list)
		return 0;
	auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_all).count();
	std::printf("\n%zu test cases, %zu failed, %zu failed checks (%lld ms)\n", ran, failed_tests, g_failures, static_cast<long long>(total_ms));
	return failed_tests == 0 && ran != 0 ? 0 : 1;
}
