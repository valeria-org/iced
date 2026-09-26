// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "iced_x86/fast_formatter.hpp"

#include "iced_x86/mvex_conv_fn.hpp"
#include "internal/mvex/mvex.hpp"

namespace iced_x86 {

namespace internal::fast {

// clang-format off
static constexpr std::array<std::uint8_t, 1 + 12> MVEX_REG_MEM_CONSTS_32[IcedConstants::MVEX_REG_MEM_CONV_ENUM_COUNT] = {
	mk_fast_str_data<12>(""),
	mk_fast_str_data<12>(""),
	mk_fast_str_data<12>("{cdab}"),
	mk_fast_str_data<12>("{badc}"),
	mk_fast_str_data<12>("{dacb}"),
	mk_fast_str_data<12>("{aaaa}"),
	mk_fast_str_data<12>("{bbbb}"),
	mk_fast_str_data<12>("{cccc}"),
	mk_fast_str_data<12>("{dddd}"),
	mk_fast_str_data<12>(""),
	mk_fast_str_data<12>("{1to16}"),
	mk_fast_str_data<12>("{4to16}"),
	mk_fast_str_data<12>("{float16}"),
	mk_fast_str_data<12>("{uint8}"),
	mk_fast_str_data<12>("{sint8}"),
	mk_fast_str_data<12>("{uint16}"),
	mk_fast_str_data<12>("{sint16}"),
};
static constexpr std::array<std::uint8_t, 1 + 12> MVEX_REG_MEM_CONSTS_64[IcedConstants::MVEX_REG_MEM_CONV_ENUM_COUNT] = {
	mk_fast_str_data<12>(""),
	mk_fast_str_data<12>(""),
	mk_fast_str_data<12>("{cdab}"),
	mk_fast_str_data<12>("{badc}"),
	mk_fast_str_data<12>("{dacb}"),
	mk_fast_str_data<12>("{aaaa}"),
	mk_fast_str_data<12>("{bbbb}"),
	mk_fast_str_data<12>("{cccc}"),
	mk_fast_str_data<12>("{dddd}"),
	mk_fast_str_data<12>(""),
	mk_fast_str_data<12>("{1to8}"),
	mk_fast_str_data<12>("{4to8}"),
	mk_fast_str_data<12>("{float16}"),
	mk_fast_str_data<12>("{uint8}"),
	mk_fast_str_data<12>("{sint8}"),
	mk_fast_str_data<12>("{uint16}"),
	mk_fast_str_data<12>("{sint16}"),
};
// clang-format on

const std::uint8_t* get_mvex_reg_mem_conv_string(Code code, MvexRegMemConv conv) {
	ICED_ASSERT(IcedConstants::is_mvex(code));
	const auto& mvex = get_mvex_info(code);
	if (mvex.conv_fn == MvexConvFn::None)
		return nullptr;
	const auto index = static_cast<std::size_t>(conv);
	ICED_ASSERT(index < IcedConstants::MVEX_REG_MEM_CONV_ENUM_COUNT);
	return mvex.is_conv_fn_32() ? MVEX_REG_MEM_CONSTS_32[index].data() : MVEX_REG_MEM_CONSTS_64[index].data();
}

} // namespace internal::fast

// The library contains these instantiations
template class SpecializedFormatter<DefaultFastFormatterTraitOptions>;
template class SpecializedFormatter<DefaultSpecializedFormatterTraitOptions>;

} // namespace iced_x86
