// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_framework.hpp"
#include "test_utils/section_file_reader.hpp"
#include "test_utils/str_utils.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace iced_x86::tests;

namespace {

// Creates a temp file that is deleted when this object is destroyed
class TempFile {
public:
	explicit TempFile(const std::string& data) {
		static std::atomic<std::uint32_t> counter{0};
		std::random_device rd;
		const auto name = "iced_x86_tests_" + std::to_string(rd()) + "_" + std::to_string(counter++) + ".txt";
		path_ = (std::filesystem::temp_directory_path() / name).string();
		std::ofstream file(path_, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file)
			throw std::runtime_error("Couldn't create " + path_);
		file.write(data.data(), static_cast<std::streamsize>(data.size()));
	}
	~TempFile() {
		std::error_code ec;
		std::filesystem::remove(path_, ec);
	}
	TempFile(const TempFile&) = delete;
	TempFile& operator=(const TempFile&) = delete;
	const std::string& path() const noexcept { return path_; }

private:
	std::string path_;
};

std::vector<std::pair<std::uint32_t, std::string>> read_file(const std::string& data) {
	TempFile file(data);
	SectionFileReader reader({{"sec-a", 1}, {"sec-b", 2}});
	std::vector<std::pair<std::uint32_t, std::string>> result;
	reader.read(file.path(), [&](std::uint32_t id, std::string_view line) {
		if (line == "bad")
			throw std::runtime_error("Bad line");
		result.emplace_back(id, std::string(line));
	});
	return result;
}

std::optional<std::string> get_read_error(const std::string& data, std::string* filename = nullptr) {
	try {
		(void)read_file(data);
	}
	catch (const std::runtime_error& ex) {
		return std::string(ex.what());
	}
	(void)filename;
	return std::nullopt;
}

bool error_ends_with(const std::optional<std::string>& err, const std::string& suffix) {
	return err.has_value() && starts_with(*err, "Error parsing file '") && ends_with(*err, suffix);
}

} // namespace

TEST_CASE("test_utils/section_file_reader") {
	using Lines = std::vector<std::pair<std::uint32_t, std::string>>;
	CHECK(read_file("").empty());
	CHECK(read_file("# comment\n\n").empty());
	CHECK_EQ(read_file("[sec-a]\nline 1\n# comment\n\nline 2\n[sec-b]\n line 3 \n[sec-a]\nline 4"),
		(Lines{{1, "line 1"}, {1, "line 2"}, {2, " line 3 "}, {1, "line 4"}}));
	// \r\n line endings
	CHECK_EQ(read_file("[sec-b]\r\nline 1\r\n\r\nline 2\r\n"), (Lines{{2, "line 1"}, {2, "line 2"}}));
	// A '\r' at the end of the file (no '\n') is kept (same as Rust)
	CHECK_EQ(read_file("[sec-b]\r\nline 1\r"), (Lines{{2, "line 1\r"}}));

	CHECK(error_ends_with(get_read_error("line 1\n"), "', line 1: Missing section"));
	CHECK(error_ends_with(get_read_error("\n# x\n[sec-a\n"), "', line 3: Missing ']'"));
	CHECK(error_ends_with(get_read_error("[sec-a]\n[sec-c]\n"), "', line 2: Unknown section name: sec-c"));
	CHECK(error_ends_with(get_read_error("[sec-a]\nok\nbad\n"), "', line 3: Bad line"));
	// "[sec-a]\r" isn't a section name
	CHECK(error_ends_with(get_read_error("[sec-a]\r\r\n"), "', line 1: Missing ']'"));

	// Full error message
	{
		TempFile file("[x]\n");
		SectionFileReader reader({{"sec-a", 1}});
		std::optional<std::string> err;
		try {
			reader.read(file.path(), [](std::uint32_t, std::string_view) {});
		}
		catch (const std::runtime_error& ex) {
			err = ex.what();
		}
		CHECK_EQ(err, std::optional<std::string>("Error parsing file '" + file.path() + "', line 1: Unknown section name: x"));
	}

	// Missing file
	{
		SectionFileReader reader({{"sec-a", 1}});
		std::optional<std::string> err;
		try {
			reader.read("/this/file/does/not/exist.txt", [](std::uint32_t, std::string_view) {});
		}
		catch (const std::runtime_error& ex) {
			err = ex.what();
		}
		CHECK_EQ(err, std::optional<std::string>("Couldn't open file /this/file/does/not/exist.txt"));
	}
}

TEST_CASE("test_utils/str_utils") {
	using Parts = std::vector<std::string_view>;
	CHECK_EQ(std::string(trim("  a b \t\r\n")), "a b");
	CHECK_EQ(std::string(trim("\xC2\xA0" "a\xE3\x80\x80")), "a");
	CHECK_EQ(std::string(trim("\xC2\xA1" "a")), "\xC2\xA1" "a");
	CHECK_EQ(std::string(trim_start("  a ")), "a ");
	CHECK_EQ(std::string(trim_end("  a ")), "  a");
	CHECK_EQ(std::string(trim("   ")), "");
	CHECK_EQ(split("", ','), (Parts{""}));
	CHECK_EQ(split("a,,b,", ','), (Parts{"a", "", "b", ""}));
	CHECK_EQ(split("a::b", "::"), (Parts{"a", "b"}));
	CHECK_EQ(split_whitespace("  a  b\tc\n"), (Parts{"a", "b", "c"}));
	CHECK_EQ(split_whitespace("a\xE2\x80\x80" "b"), (Parts{"a", "b"}));
	CHECK(split_whitespace(" \t ").empty());
	CHECK(starts_with("abc", "ab"));
	CHECK(!starts_with("a", "ab"));
	CHECK(ends_with("abc", "bc"));
	CHECK(contains("abc", 'b'));
	CHECK(contains("abc", "bc"));
	CHECK_EQ(to_ascii_lowercase("AbC1"), "abc1");
	CHECK_EQ(to_ascii_uppercase("AbC1"), "ABC1");
}
