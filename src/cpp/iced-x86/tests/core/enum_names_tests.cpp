// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "test_framework.hpp"
#include "iced_x86/code.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/register.hpp"
#include "iced_x86/iced_error.hpp"
#include "test_utils/abort_utils.hpp"

#include <string>
#include <utility>

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

TEST_CASE("core/result_operators") {
	Result<std::string> s(std::string("hello"));
	CHECK(s.has_value());
	CHECK_EQ(*s, "hello");
	CHECK_EQ(s->size(), static_cast<std::size_t>(5));
	s->append("!");
	CHECK_EQ(s.value(), "hello!");
	(*s)[0] = 'H';
	const Result<std::string>& cs = s;
	CHECK_EQ(*cs, "Hello!");
	CHECK_EQ(cs->size(), static_cast<std::size_t>(6));
	std::string moved = *std::move(s);
	CHECK_EQ(moved, "Hello!");

	Result<int> err(IcedError("bad"));
	CHECK(!err.has_value());
	Result<void> v;
	CHECK(v.has_value());
	Result<void> v2(IcedError("bad"));
	CHECK(!v2.has_value());

	CHECK(::iced_x86::tests::aborts([] {
		Result<int> e(IcedError("bad"));
		(void)*e;
	}));
	CHECK(::iced_x86::tests::aborts([] {
		Result<std::string> e(IcedError("bad"));
		(void)e->size();
	}));
}
