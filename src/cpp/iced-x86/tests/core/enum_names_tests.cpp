// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_framework.hpp"
#include "iced_x86/code.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/register.hpp"
#include "iced_x86/iced_error.hpp"

#include <string>

using namespace iced_x86;

TEST_CASE("core/enum_to_string") {
	CHECK_EQ(std::string(to_string(Code::INVALID)), "INVALID");
	CHECK_EQ(std::string(to_string(Code::Add_rm8_r8)), "Add_rm8_r8");
	CHECK_EQ(std::string(to_string(Register::EAX)), "EAX");
	CHECK_EQ(std::string(to_string(static_cast<Code>(IcedConstants::CODE_ENUM_COUNT))), "");
	CHECK(IcedConstants::is_mvex(static_cast<Code>(IcedConstants::MVEX_START)));
}

TEST_CASE("core/result") {
	Result<int> ok(5);
	CHECK(ok.is_ok());
	CHECK_EQ(ok.value(), 5);
	Result<int> err(IcedError("bad"));
	CHECK(err.is_err());
	CHECK_EQ(std::string(err.error().message()), "bad");
	Result<std::string> s(std::string("hello"));
	Result<std::string> s2 = s;
	CHECK_EQ(s2.value(), "hello");
	Result<void> v;
	CHECK(v.is_ok());
	Result<void> v2(IcedError(std::string("dyn")));
	CHECK_EQ(std::string(v2.error().message()), "dyn");
}
