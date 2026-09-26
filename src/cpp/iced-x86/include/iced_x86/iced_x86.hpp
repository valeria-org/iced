// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Umbrella header: includes the whole public API

#pragma once

// Core
#include "iced_x86/code.hpp"
#include "iced_x86/code_ext.hpp"
#include "iced_x86/code_size.hpp"
#include "iced_x86/condition_code.hpp"
#include "iced_x86/constant_offsets.hpp"
#include "iced_x86/cpuid_feature.hpp"
#include "iced_x86/encoding_kind.hpp"
#include "iced_x86/flow_control.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/iced_error.hpp"
#include "iced_x86/iced_features.hpp"
#include "iced_x86/instruction.hpp"
#include "iced_x86/memory_size.hpp"
#include "iced_x86/memory_size_ext.hpp"
#include "iced_x86/mnemonic.hpp"
#include "iced_x86/mvex_conv_fn.hpp"
#include "iced_x86/mvex_eh_bit.hpp"
#include "iced_x86/mvex_reg_mem_conv.hpp"
#include "iced_x86/mvex_tuple_type_lut_kind.hpp"
#include "iced_x86/op_kind.hpp"
#include "iced_x86/register.hpp"
#include "iced_x86/register_ext.hpp"
#include "iced_x86/rep_prefix_kind.hpp"
#include "iced_x86/rflags_bits.hpp"
#include "iced_x86/rounding_control.hpp"
#include "iced_x86/slice.hpp"
#include "iced_x86/tuple_type.hpp"
