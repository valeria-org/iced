// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's encoder/tests/op_code_test_case.rs

#pragma once

#include "iced_x86/code.hpp"
#include "iced_x86/encoding_kind.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/mandatory_prefix.hpp"
#include "iced_x86/memory_size.hpp"
#include "iced_x86/mnemonic.hpp"
#include "iced_x86/mvex_conv_fn.hpp"
#include "iced_x86/mvex_eh_bit.hpp"
#include "iced_x86/mvex_tuple_type_lut_kind.hpp"
#include "iced_x86/op_code_operand_kind.hpp"
#include "iced_x86/op_code_table_kind.hpp"
#include "iced_x86/tuple_type.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace iced_x86::tests {

struct MvexTestCase {
	MvexEHBit eh_bit = MvexEHBit::None;
	bool can_use_eviction_hint = false;
	bool can_use_imm_rounding_control = false;
	bool ignores_op_mask_register = false;
	bool no_sae_rc = false;
	MvexTupleTypeLutKind tuple_type_lut_kind = static_cast<MvexTupleTypeLutKind>(0);
	MvexConvFn conversion_func = MvexConvFn::None;
	std::uint8_t valid_conversion_funcs_mask = 0;
	std::uint8_t valid_swizzle_funcs_mask = 0;
};

struct OpCodeInfoTestCase {
	std::uint32_t line_number = 0;
	Code code = Code::INVALID;
	Mnemonic mnemonic = Mnemonic::INVALID;
	std::string op_code_string;
	std::string instruction_string;
	EncodingKind encoding = EncodingKind::Legacy;
	bool is_instruction = false;
	bool mode16 = false;
	bool mode32 = false;
	bool mode64 = false;
	bool fwait = false;
	std::uint32_t operand_size = 0;
	std::uint32_t address_size = 0;
	std::uint32_t l = 0;
	std::uint32_t w = 0;
	bool is_lig = false;
	bool is_wig = false;
	bool is_wig32 = false;
	TupleType tuple_type = TupleType::N1;
	MemorySize memory_size = MemorySize::Unknown;
	MemorySize broadcast_memory_size = MemorySize::Unknown;
	std::uint32_t decoder_option = 0;
	bool can_broadcast = false;
	bool can_use_rounding_control = false;
	bool can_suppress_all_exceptions = false;
	bool can_use_op_mask_register = false;
	bool require_op_mask_register = false;
	bool can_use_zeroing_masking = false;
	bool can_use_lock_prefix = false;
	bool can_use_xacquire_prefix = false;
	bool can_use_xrelease_prefix = false;
	bool can_use_rep_prefix = false;
	bool can_use_repne_prefix = false;
	bool can_use_bnd_prefix = false;
	bool can_use_hint_taken_prefix = false;
	bool can_use_notrack_prefix = false;
	bool ignores_rounding_control = false;
	bool amd_lock_reg_bit = false;
	bool default_op_size64 = false;
	bool force_op_size64 = false;
	bool intel_force_op_size64 = false;
	bool cpl0 = false;
	bool cpl1 = false;
	bool cpl2 = false;
	bool cpl3 = false;
	bool is_input_output = false;
	bool is_nop = false;
	bool is_reserved_nop = false;
	bool is_serializing_intel = false;
	bool is_serializing_amd = false;
	bool may_require_cpl0 = false;
	bool is_cet_tracked = false;
	bool is_non_temporal = false;
	bool is_fpu_no_wait = false;
	bool ignores_mod_bits = false;
	bool no66 = false;
	bool nfx = false;
	bool requires_unique_reg_nums = false;
	bool requires_unique_dest_reg_num = false;
	bool is_privileged = false;
	bool is_save_restore = false;
	bool is_stack_instruction = false;
	bool ignores_segment = false;
	bool is_op_mask_read_write = false;
	bool real_mode = false;
	bool protected_mode = false;
	bool virtual8086_mode = false;
	bool compatibility_mode = false;
	bool long_mode = false;
	bool use_outside_smm = false;
	bool use_in_smm = false;
	bool use_outside_enclave_sgx = false;
	bool use_in_enclave_sgx1 = false;
	bool use_in_enclave_sgx2 = false;
	bool use_outside_vmx_op = false;
	bool use_in_vmx_root_op = false;
	bool use_in_vmx_non_root_op = false;
	bool use_outside_seam = false;
	bool use_in_seam = false;
	bool tdx_non_root_gen_ud = false;
	bool tdx_non_root_gen_ve = false;
	bool tdx_non_root_may_gen_ex = false;
	bool intel_vm_exit = false;
	bool intel_may_vm_exit = false;
	bool intel_smm_vm_exit = false;
	bool amd_vm_exit = false;
	bool amd_may_vm_exit = false;
	bool tsx_abort = false;
	bool tsx_impl_abort = false;
	bool tsx_may_abort = false;
	bool intel_decoder16 = false;
	bool intel_decoder32 = false;
	bool intel_decoder64 = false;
	bool amd_decoder16 = false;
	bool amd_decoder32 = false;
	bool amd_decoder64 = false;
	OpCodeTableKind table = OpCodeTableKind::Normal;
	MandatoryPrefix mandatory_prefix = MandatoryPrefix::None;
	std::uint32_t op_code = 0;
	std::uint32_t op_code_len = 0;
	bool is_group = false;
	std::int32_t group_index = 0;
	bool is_rm_group = false;
	std::int32_t rm_group_index = 0;
	std::uint32_t op_count = 0;
	OpCodeOperandKind op_kinds[IcedConstants::MAX_OP_COUNT] = {};
	MvexTestCase mvex;
};

/// Reads all test cases in `OpCodeInfos.txt` (Rust: `OpCodeInfoTestParser`)
std::vector<OpCodeInfoTestCase> read_op_code_info_test_cases(const std::string& filename);

} // namespace iced_x86::tests
