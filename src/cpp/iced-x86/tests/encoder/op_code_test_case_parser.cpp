// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Port of Rust's encoder/tests/op_code_test_case_parser.rs

#include "encoder/op_code_test_case.hpp"
#include "generated/op_code_info_flags.hpp"
#include "generated/op_code_info_keys.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/str_utils.hpp"
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace iced_x86::tests {

namespace {

enum class Key {
	GROUP_INDEX,
	RM_GROUP_INDEX,
	OP_CODE_OPERAND_KIND,
	TUPLE_TYPE,
	DECODER_OPTION,
	MVEX,
};

enum class Flag {
	NO_INSTRUCTION,
	BIT16,
	BIT32,
	BIT64,
	FWAIT,
	OPERAND_SIZE16,
	OPERAND_SIZE32,
	OPERAND_SIZE64,
	ADDRESS_SIZE16,
	ADDRESS_SIZE32,
	ADDRESS_SIZE64,
	LIG,
	L0,
	L1,
	L128,
	L256,
	L512,
	WIG,
	WIG32,
	W0,
	W1,
	BROADCAST,
	ROUNDING_CONTROL,
	SUPPRESS_ALL_EXCEPTIONS,
	OP_MASK_REGISTER,
	REQUIRE_OP_MASK_REGISTER,
	ZEROING_MASKING,
	LOCK,
	XACQUIRE,
	XRELEASE,
	REP,
	REPE,
	REPNE,
	BND,
	HINT_TAKEN,
	NOTRACK,
	IGNORES_ROUNDING_CONTROL,
	AMD_LOCK_REG_BIT,
	DEFAULT_OP_SIZE64,
	FORCE_OP_SIZE64,
	INTEL_FORCE_OP_SIZE64,
	CPL0,
	CPL1,
	CPL2,
	CPL3,
	INPUT_OUTPUT,
	NOP,
	RESERVED_NOP,
	SERIALIZING_INTEL,
	SERIALIZING_AMD,
	MAY_REQUIRE_CPL0,
	CET_TRACKED,
	NON_TEMPORAL,
	FPU_NO_WAIT,
	IGNORES_MOD_BITS,
	NO66,
	NFX,
	REQUIRES_UNIQUE_REG_NUMS,
	PRIVILEGED,
	SAVE_RESTORE,
	STACK_INSTRUCTION,
	IGNORES_SEGMENT,
	OP_MASK_READ_WRITE,
	REAL_MODE,
	PROTECTED_MODE,
	VIRTUAL8086_MODE,
	COMPATIBILITY_MODE,
	LONG_MODE,
	USE_OUTSIDE_SMM,
	USE_IN_SMM,
	USE_OUTSIDE_ENCLAVE_SGX,
	USE_IN_ENCLAVE_SGX1,
	USE_IN_ENCLAVE_SGX2,
	USE_OUTSIDE_VMX_OP,
	USE_IN_VMX_ROOT_OP,
	USE_IN_VMX_NON_ROOT_OP,
	USE_OUTSIDE_SEAM,
	USE_IN_SEAM,
	TDX_NON_ROOT_GEN_UD,
	TDX_NON_ROOT_GEN_VE,
	TDX_NON_ROOT_MAY_GEN_EX,
	INTEL_VM_EXIT,
	INTEL_MAY_VM_EXIT,
	INTEL_SMM_VM_EXIT,
	AMD_VM_EXIT,
	AMD_MAY_VM_EXIT,
	TSX_ABORT,
	TSX_IMPL_ABORT,
	TSX_MAY_ABORT,
	INTEL_DECODER16,
	INTEL_DECODER32,
	INTEL_DECODER64,
	AMD_DECODER16,
	AMD_DECODER32,
	AMD_DECODER64,
	REQUIRES_UNIQUE_DEST_REG_NUM,
	EH0,
	EH1,
	EVICTION_HINT,
	IMM_ROUNDING_CONTROL,
	IGNORES_OP_MASK_REGISTER,
	NO_SAE_ROUNDING_CONTROL,
};

const std::unordered_map<std::string_view, Key>& get_keys() {
	static const std::unordered_map<std::string_view, Key> keys = {
		{OpCodeInfoKeys::GROUP_INDEX, Key::GROUP_INDEX},
		{OpCodeInfoKeys::RM_GROUP_INDEX, Key::RM_GROUP_INDEX},
		{OpCodeInfoKeys::OP_CODE_OPERAND_KIND, Key::OP_CODE_OPERAND_KIND},
		{OpCodeInfoKeys::TUPLE_TYPE, Key::TUPLE_TYPE},
		{OpCodeInfoKeys::DECODER_OPTION, Key::DECODER_OPTION},
		{OpCodeInfoKeys::MVEX, Key::MVEX},
	};
	return keys;
}

const std::unordered_map<std::string_view, Flag>& get_flags() {
	static const std::unordered_map<std::string_view, Flag> flags = {
		{OpCodeInfoFlags::NO_INSTRUCTION, Flag::NO_INSTRUCTION},
		{OpCodeInfoFlags::BIT16, Flag::BIT16},
		{OpCodeInfoFlags::BIT32, Flag::BIT32},
		{OpCodeInfoFlags::BIT64, Flag::BIT64},
		{OpCodeInfoFlags::FWAIT, Flag::FWAIT},
		{OpCodeInfoFlags::OPERAND_SIZE16, Flag::OPERAND_SIZE16},
		{OpCodeInfoFlags::OPERAND_SIZE32, Flag::OPERAND_SIZE32},
		{OpCodeInfoFlags::OPERAND_SIZE64, Flag::OPERAND_SIZE64},
		{OpCodeInfoFlags::ADDRESS_SIZE16, Flag::ADDRESS_SIZE16},
		{OpCodeInfoFlags::ADDRESS_SIZE32, Flag::ADDRESS_SIZE32},
		{OpCodeInfoFlags::ADDRESS_SIZE64, Flag::ADDRESS_SIZE64},
		{OpCodeInfoFlags::LIG, Flag::LIG},
		{OpCodeInfoFlags::L0, Flag::L0},
		{OpCodeInfoFlags::L1, Flag::L1},
		{OpCodeInfoFlags::L128, Flag::L128},
		{OpCodeInfoFlags::L256, Flag::L256},
		{OpCodeInfoFlags::L512, Flag::L512},
		{OpCodeInfoFlags::WIG, Flag::WIG},
		{OpCodeInfoFlags::WIG32, Flag::WIG32},
		{OpCodeInfoFlags::W0, Flag::W0},
		{OpCodeInfoFlags::W1, Flag::W1},
		{OpCodeInfoFlags::BROADCAST, Flag::BROADCAST},
		{OpCodeInfoFlags::ROUNDING_CONTROL, Flag::ROUNDING_CONTROL},
		{OpCodeInfoFlags::SUPPRESS_ALL_EXCEPTIONS, Flag::SUPPRESS_ALL_EXCEPTIONS},
		{OpCodeInfoFlags::OP_MASK_REGISTER, Flag::OP_MASK_REGISTER},
		{OpCodeInfoFlags::REQUIRE_OP_MASK_REGISTER, Flag::REQUIRE_OP_MASK_REGISTER},
		{OpCodeInfoFlags::ZEROING_MASKING, Flag::ZEROING_MASKING},
		{OpCodeInfoFlags::LOCK, Flag::LOCK},
		{OpCodeInfoFlags::XACQUIRE, Flag::XACQUIRE},
		{OpCodeInfoFlags::XRELEASE, Flag::XRELEASE},
		{OpCodeInfoFlags::REP, Flag::REP},
		{OpCodeInfoFlags::REPE, Flag::REPE},
		{OpCodeInfoFlags::REPNE, Flag::REPNE},
		{OpCodeInfoFlags::BND, Flag::BND},
		{OpCodeInfoFlags::HINT_TAKEN, Flag::HINT_TAKEN},
		{OpCodeInfoFlags::NOTRACK, Flag::NOTRACK},
		{OpCodeInfoFlags::IGNORES_ROUNDING_CONTROL, Flag::IGNORES_ROUNDING_CONTROL},
		{OpCodeInfoFlags::AMD_LOCK_REG_BIT, Flag::AMD_LOCK_REG_BIT},
		{OpCodeInfoFlags::DEFAULT_OP_SIZE64, Flag::DEFAULT_OP_SIZE64},
		{OpCodeInfoFlags::FORCE_OP_SIZE64, Flag::FORCE_OP_SIZE64},
		{OpCodeInfoFlags::INTEL_FORCE_OP_SIZE64, Flag::INTEL_FORCE_OP_SIZE64},
		{OpCodeInfoFlags::CPL0, Flag::CPL0},
		{OpCodeInfoFlags::CPL1, Flag::CPL1},
		{OpCodeInfoFlags::CPL2, Flag::CPL2},
		{OpCodeInfoFlags::CPL3, Flag::CPL3},
		{OpCodeInfoFlags::INPUT_OUTPUT, Flag::INPUT_OUTPUT},
		{OpCodeInfoFlags::NOP, Flag::NOP},
		{OpCodeInfoFlags::RESERVED_NOP, Flag::RESERVED_NOP},
		{OpCodeInfoFlags::SERIALIZING_INTEL, Flag::SERIALIZING_INTEL},
		{OpCodeInfoFlags::SERIALIZING_AMD, Flag::SERIALIZING_AMD},
		{OpCodeInfoFlags::MAY_REQUIRE_CPL0, Flag::MAY_REQUIRE_CPL0},
		{OpCodeInfoFlags::CET_TRACKED, Flag::CET_TRACKED},
		{OpCodeInfoFlags::NON_TEMPORAL, Flag::NON_TEMPORAL},
		{OpCodeInfoFlags::FPU_NO_WAIT, Flag::FPU_NO_WAIT},
		{OpCodeInfoFlags::IGNORES_MOD_BITS, Flag::IGNORES_MOD_BITS},
		{OpCodeInfoFlags::NO66, Flag::NO66},
		{OpCodeInfoFlags::NFX, Flag::NFX},
		{OpCodeInfoFlags::REQUIRES_UNIQUE_REG_NUMS, Flag::REQUIRES_UNIQUE_REG_NUMS},
		{OpCodeInfoFlags::PRIVILEGED, Flag::PRIVILEGED},
		{OpCodeInfoFlags::SAVE_RESTORE, Flag::SAVE_RESTORE},
		{OpCodeInfoFlags::STACK_INSTRUCTION, Flag::STACK_INSTRUCTION},
		{OpCodeInfoFlags::IGNORES_SEGMENT, Flag::IGNORES_SEGMENT},
		{OpCodeInfoFlags::OP_MASK_READ_WRITE, Flag::OP_MASK_READ_WRITE},
		{OpCodeInfoFlags::REAL_MODE, Flag::REAL_MODE},
		{OpCodeInfoFlags::PROTECTED_MODE, Flag::PROTECTED_MODE},
		{OpCodeInfoFlags::VIRTUAL8086_MODE, Flag::VIRTUAL8086_MODE},
		{OpCodeInfoFlags::COMPATIBILITY_MODE, Flag::COMPATIBILITY_MODE},
		{OpCodeInfoFlags::LONG_MODE, Flag::LONG_MODE},
		{OpCodeInfoFlags::USE_OUTSIDE_SMM, Flag::USE_OUTSIDE_SMM},
		{OpCodeInfoFlags::USE_IN_SMM, Flag::USE_IN_SMM},
		{OpCodeInfoFlags::USE_OUTSIDE_ENCLAVE_SGX, Flag::USE_OUTSIDE_ENCLAVE_SGX},
		{OpCodeInfoFlags::USE_IN_ENCLAVE_SGX1, Flag::USE_IN_ENCLAVE_SGX1},
		{OpCodeInfoFlags::USE_IN_ENCLAVE_SGX2, Flag::USE_IN_ENCLAVE_SGX2},
		{OpCodeInfoFlags::USE_OUTSIDE_VMX_OP, Flag::USE_OUTSIDE_VMX_OP},
		{OpCodeInfoFlags::USE_IN_VMX_ROOT_OP, Flag::USE_IN_VMX_ROOT_OP},
		{OpCodeInfoFlags::USE_IN_VMX_NON_ROOT_OP, Flag::USE_IN_VMX_NON_ROOT_OP},
		{OpCodeInfoFlags::USE_OUTSIDE_SEAM, Flag::USE_OUTSIDE_SEAM},
		{OpCodeInfoFlags::USE_IN_SEAM, Flag::USE_IN_SEAM},
		{OpCodeInfoFlags::TDX_NON_ROOT_GEN_UD, Flag::TDX_NON_ROOT_GEN_UD},
		{OpCodeInfoFlags::TDX_NON_ROOT_GEN_VE, Flag::TDX_NON_ROOT_GEN_VE},
		{OpCodeInfoFlags::TDX_NON_ROOT_MAY_GEN_EX, Flag::TDX_NON_ROOT_MAY_GEN_EX},
		{OpCodeInfoFlags::INTEL_VM_EXIT, Flag::INTEL_VM_EXIT},
		{OpCodeInfoFlags::INTEL_MAY_VM_EXIT, Flag::INTEL_MAY_VM_EXIT},
		{OpCodeInfoFlags::INTEL_SMM_VM_EXIT, Flag::INTEL_SMM_VM_EXIT},
		{OpCodeInfoFlags::AMD_VM_EXIT, Flag::AMD_VM_EXIT},
		{OpCodeInfoFlags::AMD_MAY_VM_EXIT, Flag::AMD_MAY_VM_EXIT},
		{OpCodeInfoFlags::TSX_ABORT, Flag::TSX_ABORT},
		{OpCodeInfoFlags::TSX_IMPL_ABORT, Flag::TSX_IMPL_ABORT},
		{OpCodeInfoFlags::TSX_MAY_ABORT, Flag::TSX_MAY_ABORT},
		{OpCodeInfoFlags::INTEL_DECODER16, Flag::INTEL_DECODER16},
		{OpCodeInfoFlags::INTEL_DECODER32, Flag::INTEL_DECODER32},
		{OpCodeInfoFlags::INTEL_DECODER64, Flag::INTEL_DECODER64},
		{OpCodeInfoFlags::AMD_DECODER16, Flag::AMD_DECODER16},
		{OpCodeInfoFlags::AMD_DECODER32, Flag::AMD_DECODER32},
		{OpCodeInfoFlags::AMD_DECODER64, Flag::AMD_DECODER64},
		{OpCodeInfoFlags::REQUIRES_UNIQUE_DEST_REG_NUM, Flag::REQUIRES_UNIQUE_DEST_REG_NUM},
		{OpCodeInfoFlags::EH0, Flag::EH0},
		{OpCodeInfoFlags::EH1, Flag::EH1},
		{OpCodeInfoFlags::EVICTION_HINT, Flag::EVICTION_HINT},
		{OpCodeInfoFlags::IMM_ROUNDING_CONTROL, Flag::IMM_ROUNDING_CONTROL},
		{OpCodeInfoFlags::IGNORES_OP_MASK_REGISTER, Flag::IGNORES_OP_MASK_REGISTER},
		{OpCodeInfoFlags::NO_SAE_ROUNDING_CONTROL, Flag::NO_SAE_ROUNDING_CONTROL},
	};
	return flags;
}

class OpCodeInfoTestParser {
public:
	OpCodeInfoTestParser()
		: to_encoding_kind(create_dict(ENCODING_KIND_DICT)), to_mandatory_prefix(create_dict(MANDATORY_PREFIX_DICT)),
		  to_op_code_table_kind(create_dict(OP_CODE_TABLE_KIND_DICT)) {}

	std::optional<OpCodeInfoTestCase> read_next_test_case(std::string_view line, std::uint32_t line_number) const {
		const auto elems = split(line, ',');
		if (elems.size() != 11)
			throw std::runtime_error("Invalid number of commas: " + std::to_string(elems.size() - 1));

		OpCodeInfoTestCase tc;
		tc.line_number = line_number;
		tc.is_instruction = true;
		tc.group_index = -1;
		tc.rm_group_index = -1;

		if (is_ignored_code(trim(elems[0])))
			return std::nullopt;
		tc.code = to_code(trim(elems[0]));
		tc.mnemonic = to_mnemonic(trim(elems[1]));
		tc.memory_size = to_memory_size(trim(elems[2]));
		tc.broadcast_memory_size = to_memory_size(trim(elems[3]));
		tc.encoding = to_encoding(trim(elems[4]));
		tc.mandatory_prefix = to_mandatory_prefix_value(trim(elems[5]));
		tc.table = to_table(trim(elems[6]));
		to_op_code(trim(elems[7]), tc.op_code, tc.op_code_len);
		tc.op_code_string = std::string(trim(elems[8]));
		tc.instruction_string = std::string(trim(elems[9]));
		for (auto& c : tc.instruction_string) {
			if (c == '|')
				c = ',';
		}

		bool got_vector_length = false;
		bool got_w = false;
		for (std::string_view part : split_whitespace(elems[10])) {
			std::string_view key = trim(part);
			if (key.empty())
				continue;
			const auto eq_index = key.find('=');
			if (eq_index != std::string_view::npos) {
				const std::string_view value = key.substr(eq_index + 1);
				key = key.substr(0, eq_index);
				const auto& keys = get_keys();
				const auto it = keys.find(key);
				if (it == keys.end())
					throw std::runtime_error("Invalid key: '" + std::string(key) + "'");
				switch (it->second) {
				case Key::GROUP_INDEX:
					tc.group_index = to_i32(value);
					if (tc.group_index > 7)
						throw std::runtime_error("Invalid group index: " + std::string(value));
					tc.is_group = true;
					break;

				case Key::RM_GROUP_INDEX:
					tc.rm_group_index = to_i32(value);
					if (tc.rm_group_index > 7)
						throw std::runtime_error("Invalid rm group index: " + std::string(value));
					tc.is_rm_group = true;
					break;

				case Key::OP_CODE_OPERAND_KIND: {
					const auto op_parts = split(value, ';');
					tc.op_count = static_cast<std::uint32_t>(op_parts.size());
					static_assert(IcedConstants::MAX_OP_COUNT == 5, "");
					if (op_parts.size() >= 6)
						throw std::runtime_error("Invalid number of operands: '" + std::string(value) + "'");
					for (std::size_t i = 0; i < op_parts.size(); i++)
						tc.op_kinds[i] = to_op_code_operand_kind(op_parts[i]);
					break;
				}

				case Key::TUPLE_TYPE:
					tc.tuple_type = to_tuple_type(trim(value));
					break;

				case Key::DECODER_OPTION:
					tc.decoder_option = to_decoder_options(trim(value));
					break;

				case Key::MVEX: {
					const auto parts = split(trim(value), ';');
					if (parts.size() != 4)
						throw std::runtime_error("Invalid number of semicolons. Expected 3, found " + std::to_string(parts.size() - 1));
					tc.mvex.tuple_type_lut_kind = to_mvex_tuple_type_lut_kind(parts[0]);
					tc.mvex.conversion_func = to_mvex_conv_fn(parts[1]);
					tc.mvex.valid_conversion_funcs_mask = to_u8(parts[2]);
					tc.mvex.valid_swizzle_funcs_mask = to_u8(parts[3]);
					break;
				}
				}
			}
			else {
				const auto& flags = get_flags();
				const auto it = flags.find(key);
				if (it == flags.end())
					throw std::runtime_error("Invalid key: '" + std::string(key) + "'");
				switch (it->second) {
				case Flag::NO_INSTRUCTION:
					tc.is_instruction = false;
					break;
				case Flag::BIT16:
					tc.mode16 = true;
					break;
				case Flag::BIT32:
					tc.mode32 = true;
					break;
				case Flag::BIT64:
					tc.mode64 = true;
					break;
				case Flag::FWAIT:
					tc.fwait = true;
					break;
				case Flag::OPERAND_SIZE16:
					tc.operand_size = 16;
					break;
				case Flag::OPERAND_SIZE32:
					tc.operand_size = 32;
					break;
				case Flag::OPERAND_SIZE64:
					tc.operand_size = 64;
					break;
				case Flag::ADDRESS_SIZE16:
					tc.address_size = 16;
					break;
				case Flag::ADDRESS_SIZE32:
					tc.address_size = 32;
					break;
				case Flag::ADDRESS_SIZE64:
					tc.address_size = 64;
					break;
				case Flag::LIG:
					tc.is_lig = true;
					got_vector_length = true;
					break;
				case Flag::L0:
					tc.l = 0;
					got_vector_length = true;
					break;
				case Flag::L1:
					tc.l = 1;
					got_vector_length = true;
					break;
				case Flag::L128:
					tc.l = 0;
					got_vector_length = true;
					break;
				case Flag::L256:
					tc.l = 1;
					got_vector_length = true;
					break;
				case Flag::L512:
					tc.l = 2;
					got_vector_length = true;
					break;
				case Flag::WIG:
					tc.is_wig = true;
					got_w = true;
					break;
				case Flag::WIG32:
					tc.w = 0;
					tc.is_wig32 = true;
					got_w = true;
					break;
				case Flag::W0:
					tc.w = 0;
					got_w = true;
					break;
				case Flag::W1:
					tc.w = 1;
					got_w = true;
					break;
				case Flag::BROADCAST:
					tc.can_broadcast = true;
					break;
				case Flag::ROUNDING_CONTROL:
					tc.can_use_rounding_control = true;
					break;
				case Flag::SUPPRESS_ALL_EXCEPTIONS:
					tc.can_suppress_all_exceptions = true;
					break;
				case Flag::OP_MASK_REGISTER:
					tc.can_use_op_mask_register = true;
					break;
				case Flag::REQUIRE_OP_MASK_REGISTER:
					tc.can_use_op_mask_register = true;
					tc.require_op_mask_register = true;
					break;
				case Flag::ZEROING_MASKING:
					tc.can_use_zeroing_masking = true;
					break;
				case Flag::LOCK:
					tc.can_use_lock_prefix = true;
					break;
				case Flag::XACQUIRE:
					tc.can_use_xacquire_prefix = true;
					break;
				case Flag::XRELEASE:
					tc.can_use_xrelease_prefix = true;
					break;
				case Flag::REP:
				case Flag::REPE:
					tc.can_use_rep_prefix = true;
					break;
				case Flag::REPNE:
					tc.can_use_repne_prefix = true;
					break;
				case Flag::BND:
					tc.can_use_bnd_prefix = true;
					break;
				case Flag::HINT_TAKEN:
					tc.can_use_hint_taken_prefix = true;
					break;
				case Flag::NOTRACK:
					tc.can_use_notrack_prefix = true;
					break;
				case Flag::IGNORES_ROUNDING_CONTROL:
					tc.ignores_rounding_control = true;
					break;
				case Flag::AMD_LOCK_REG_BIT:
					tc.amd_lock_reg_bit = true;
					break;
				case Flag::DEFAULT_OP_SIZE64:
					tc.default_op_size64 = true;
					break;
				case Flag::FORCE_OP_SIZE64:
					tc.force_op_size64 = true;
					break;
				case Flag::INTEL_FORCE_OP_SIZE64:
					tc.intel_force_op_size64 = true;
					break;
				case Flag::CPL0:
					tc.cpl0 = true;
					break;
				case Flag::CPL1:
					tc.cpl1 = true;
					break;
				case Flag::CPL2:
					tc.cpl2 = true;
					break;
				case Flag::CPL3:
					tc.cpl3 = true;
					break;
				case Flag::INPUT_OUTPUT:
					tc.is_input_output = true;
					break;
				case Flag::NOP:
					tc.is_nop = true;
					break;
				case Flag::RESERVED_NOP:
					tc.is_reserved_nop = true;
					break;
				case Flag::SERIALIZING_INTEL:
					tc.is_serializing_intel = true;
					break;
				case Flag::SERIALIZING_AMD:
					tc.is_serializing_amd = true;
					break;
				case Flag::MAY_REQUIRE_CPL0:
					tc.may_require_cpl0 = true;
					break;
				case Flag::CET_TRACKED:
					tc.is_cet_tracked = true;
					break;
				case Flag::NON_TEMPORAL:
					tc.is_non_temporal = true;
					break;
				case Flag::FPU_NO_WAIT:
					tc.is_fpu_no_wait = true;
					break;
				case Flag::IGNORES_MOD_BITS:
					tc.ignores_mod_bits = true;
					break;
				case Flag::NO66:
					tc.no66 = true;
					break;
				case Flag::NFX:
					tc.nfx = true;
					break;
				case Flag::REQUIRES_UNIQUE_REG_NUMS:
					tc.requires_unique_reg_nums = true;
					break;
				case Flag::PRIVILEGED:
					tc.is_privileged = true;
					break;
				case Flag::SAVE_RESTORE:
					tc.is_save_restore = true;
					break;
				case Flag::STACK_INSTRUCTION:
					tc.is_stack_instruction = true;
					break;
				case Flag::IGNORES_SEGMENT:
					tc.ignores_segment = true;
					break;
				case Flag::OP_MASK_READ_WRITE:
					tc.is_op_mask_read_write = true;
					break;
				case Flag::REAL_MODE:
					tc.real_mode = true;
					break;
				case Flag::PROTECTED_MODE:
					tc.protected_mode = true;
					break;
				case Flag::VIRTUAL8086_MODE:
					tc.virtual8086_mode = true;
					break;
				case Flag::COMPATIBILITY_MODE:
					tc.compatibility_mode = true;
					break;
				case Flag::LONG_MODE:
					tc.long_mode = true;
					break;
				case Flag::USE_OUTSIDE_SMM:
					tc.use_outside_smm = true;
					break;
				case Flag::USE_IN_SMM:
					tc.use_in_smm = true;
					break;
				case Flag::USE_OUTSIDE_ENCLAVE_SGX:
					tc.use_outside_enclave_sgx = true;
					break;
				case Flag::USE_IN_ENCLAVE_SGX1:
					tc.use_in_enclave_sgx1 = true;
					break;
				case Flag::USE_IN_ENCLAVE_SGX2:
					tc.use_in_enclave_sgx2 = true;
					break;
				case Flag::USE_OUTSIDE_VMX_OP:
					tc.use_outside_vmx_op = true;
					break;
				case Flag::USE_IN_VMX_ROOT_OP:
					tc.use_in_vmx_root_op = true;
					break;
				case Flag::USE_IN_VMX_NON_ROOT_OP:
					tc.use_in_vmx_non_root_op = true;
					break;
				case Flag::USE_OUTSIDE_SEAM:
					tc.use_outside_seam = true;
					break;
				case Flag::USE_IN_SEAM:
					tc.use_in_seam = true;
					break;
				case Flag::TDX_NON_ROOT_GEN_UD:
					tc.tdx_non_root_gen_ud = true;
					break;
				case Flag::TDX_NON_ROOT_GEN_VE:
					tc.tdx_non_root_gen_ve = true;
					break;
				case Flag::TDX_NON_ROOT_MAY_GEN_EX:
					tc.tdx_non_root_may_gen_ex = true;
					break;
				case Flag::INTEL_VM_EXIT:
					tc.intel_vm_exit = true;
					break;
				case Flag::INTEL_MAY_VM_EXIT:
					tc.intel_may_vm_exit = true;
					break;
				case Flag::INTEL_SMM_VM_EXIT:
					tc.intel_smm_vm_exit = true;
					break;
				case Flag::AMD_VM_EXIT:
					tc.amd_vm_exit = true;
					break;
				case Flag::AMD_MAY_VM_EXIT:
					tc.amd_may_vm_exit = true;
					break;
				case Flag::TSX_ABORT:
					tc.tsx_abort = true;
					break;
				case Flag::TSX_IMPL_ABORT:
					tc.tsx_impl_abort = true;
					break;
				case Flag::TSX_MAY_ABORT:
					tc.tsx_may_abort = true;
					break;
				case Flag::INTEL_DECODER16:
					tc.intel_decoder16 = true;
					break;
				case Flag::INTEL_DECODER32:
					tc.intel_decoder32 = true;
					break;
				case Flag::INTEL_DECODER64:
					tc.intel_decoder64 = true;
					break;
				case Flag::AMD_DECODER16:
					tc.amd_decoder16 = true;
					break;
				case Flag::AMD_DECODER32:
					tc.amd_decoder32 = true;
					break;
				case Flag::AMD_DECODER64:
					tc.amd_decoder64 = true;
					break;
				case Flag::REQUIRES_UNIQUE_DEST_REG_NUM:
					tc.requires_unique_dest_reg_num = true;
					break;
				case Flag::EH0:
					tc.mvex.eh_bit = MvexEHBit::EH0;
					break;
				case Flag::EH1:
					tc.mvex.eh_bit = MvexEHBit::EH1;
					break;
				case Flag::EVICTION_HINT:
					tc.mvex.can_use_eviction_hint = true;
					break;
				case Flag::IMM_ROUNDING_CONTROL:
					tc.mvex.can_use_imm_rounding_control = true;
					break;
				case Flag::IGNORES_OP_MASK_REGISTER:
					tc.mvex.ignores_op_mask_register = true;
					break;
				case Flag::NO_SAE_ROUNDING_CONTROL:
					tc.mvex.no_sae_rc = true;
					break;
				}
			}
		}
		switch (tc.encoding) {
		case EncodingKind::Legacy:
		case EncodingKind::D3NOW:
			break;
		case EncodingKind::VEX:
		case EncodingKind::EVEX:
		case EncodingKind::XOP:
		case EncodingKind::MVEX:
			if (!got_vector_length)
				throw std::runtime_error("Missing vector length: L0/L1/L128/L256/L512/LIG");
			if (!got_w)
				throw std::runtime_error("Missing W bit: W0/W1/WIG/WIG32");
			break;
		}

		return tc;
	}

private:
	EncodingKind to_encoding(std::string_view value) const {
		const auto it = to_encoding_kind.find(value);
		if (it == to_encoding_kind.end())
			throw std::runtime_error("Invalid encoding value: '" + std::string(value) + "'");
		return it->second;
	}

	MandatoryPrefix to_mandatory_prefix_value(std::string_view value) const {
		const auto it = to_mandatory_prefix.find(value);
		if (it == to_mandatory_prefix.end())
			throw std::runtime_error("Invalid mandatory prefix value: '" + std::string(value) + "'");
		return it->second;
	}

	OpCodeTableKind to_table(std::string_view value) const {
		const auto it = to_op_code_table_kind.find(value);
		if (it == to_op_code_table_kind.end())
			throw std::runtime_error("Invalid opcode table value: '" + std::string(value) + "'");
		return it->second;
	}

	static void to_op_code(std::string_view value, std::uint32_t& op_code, std::uint32_t& op_code_len) {
		std::uint32_t result = 0;
		if (value.empty() || value.size() > 8)
			throw std::runtime_error("Invalid opcode: '" + std::string(value) + "'");
		for (const char c : value) {
			std::uint32_t digit;
			if (c >= '0' && c <= '9')
				digit = static_cast<std::uint32_t>(c - '0');
			else if (c >= 'A' && c <= 'F')
				digit = static_cast<std::uint32_t>(c - 'A' + 10);
			else if (c >= 'a' && c <= 'f')
				digit = static_cast<std::uint32_t>(c - 'a' + 10);
			else
				throw std::runtime_error("Invalid opcode: '" + std::string(value) + "'");
			result = (result << 4) | digit;
		}
		op_code = result;
		op_code_len = static_cast<std::uint32_t>(value.size()) / 2;
	}

	std::unordered_map<std::string_view, EncodingKind> to_encoding_kind;
	std::unordered_map<std::string_view, MandatoryPrefix> to_mandatory_prefix;
	std::unordered_map<std::string_view, OpCodeTableKind> to_op_code_table_kind;
};

} // namespace

std::vector<OpCodeInfoTestCase> read_op_code_info_test_cases(const std::string& filename) {
	std::vector<OpCodeInfoTestCase> result;
	const OpCodeInfoTestParser parser;
	const auto lines = read_lines(filename);
	std::uint32_t line_number = 0;
	for (const auto& line : lines) {
		line_number++;
		if (line.empty() || line[0] == '#')
			continue;
		try {
			if (auto tc = parser.read_next_test_case(line, line_number))
				result.push_back(std::move(*tc));
		}
		catch (const std::exception& ex) {
			throw std::runtime_error("Error parsing OpCodeInfo test case file '" + filename + "', line " + std::to_string(line_number) + ": " + ex.what());
		}
	}
	return result;
}

} // namespace iced_x86::tests
