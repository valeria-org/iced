// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "info/info_test_cases.hpp"
#include "generated/instruction_info_keys.hpp"
#include "generated/misc_instr_info_test_constants.hpp"
#include "generated/rflags_bits_constants.hpp"
#include "iced_x86/code_size.hpp"
#include "iced_x86/iced_constants.hpp"
#include "iced_x86/register_ext.hpp"
#include "iced_x86/rflags_bits.hpp"
#include "test_utils.hpp"
#include "generated/test_dicts.hpp"
#include "test_utils/from_str_conv.hpp"
#include "test_utils/str_utils.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace iced_x86::tests::instr_info {

std::vector<std::string_view> splitn(std::string_view s, std::size_t max_parts, char sep) {
	std::vector<std::string_view> result;
	std::size_t start = 0;
	for (;;) {
		if (result.size() + 1 == max_parts) {
			result.push_back(s.substr(start));
			break;
		}
		const std::size_t index = s.find(sep, start);
		if (index == std::string_view::npos) {
			result.push_back(s.substr(start));
			break;
		}
		result.push_back(s.substr(start, index - start));
		start = index + 1;
	}
	return result;
}

namespace {

struct ParseError : std::runtime_error {
	explicit ParseError(const std::string& message) : std::runtime_error(message) {}
};

class InstrInfoTestParser {
public:
	explicit InstrInfoTestParser(std::uint32_t bitness)
		: bitness_(bitness), to_register_(clone_register_hashmap()), to_access_(create_dict(OP_ACCESS_DICT)) {
		switch (bitness) {
		case 16:
			to_register_[MiscInstrInfoTestConstants::XSP] = Register::SP;
			to_register_[MiscInstrInfoTestConstants::XBP] = Register::BP;
			break;
		case 32:
			to_register_[MiscInstrInfoTestConstants::XSP] = Register::ESP;
			to_register_[MiscInstrInfoTestConstants::XBP] = Register::EBP;
			break;
		case 64:
			to_register_[MiscInstrInfoTestConstants::XSP] = Register::RSP;
			to_register_[MiscInstrInfoTestConstants::XBP] = Register::RBP;
			break;
		default:
			throw ParseError("Invalid bitness");
		}

		for (std::uint32_t i = 0; i < IcedConstants::VMM_COUNT; i++) {
			const Register reg = static_cast<Register>(static_cast<std::uint32_t>(IcedConstants::VMM_FIRST) + i);
			to_register_[std::string(MiscInstrInfoTestConstants::VMM_PREFIX) + std::to_string(i)] = reg;
		}
	}

	std::vector<InstrInfoTestCase> read(const std::string& filename) {
		std::vector<InstrInfoTestCase> result;
		std::uint32_t line_number = 0;
		for (const std::string& line : read_lines(filename)) {
			line_number++;
			if (line.empty() || line[0] == '#')
				continue;
			try {
				std::optional<InstrInfoTestCase> tc = read_next_test_case(line, line_number);
				if (tc)
					result.push_back(std::move(*tc));
			}
			catch (const std::exception& ex) {
				throw ParseError("Error parsing instruction info test case file '" + filename + "', line " + std::to_string(line_number) + ": " +
								 ex.what());
			}
		}
		return result;
	}

private:
	std::optional<InstrInfoTestCase> read_next_test_case(const std::string& line, std::uint32_t line_number) {
		static_assert(MiscInstrInfoTestConstants::INSTR_INFO_ELEMS_PER_LINE == 5, "");
		const std::vector<std::string_view> elems = splitn(line, MiscInstrInfoTestConstants::INSTR_INFO_ELEMS_PER_LINE, ',');
		if (elems.size() != MiscInstrInfoTestConstants::INSTR_INFO_ELEMS_PER_LINE)
			throw ParseError("Invalid number of commas: " + std::to_string(elems.size() - 1));

		InstrInfoTestCase tc;
		tc.line_number = line_number;
		tc.bitness = bitness_;

		tc.hex_bytes = std::string(trim(elems[0]));
		tc.ip = get_default_ip(tc.bitness);
		(void)to_vec_u8(tc.hex_bytes);
		if (is_ignored_code(elems[1]))
			return std::nullopt;
		tc.code = to_code(elems[1]);
		tc.encoding = to_encoding_kind(elems[2]);
		const std::vector<std::string_view> cpuid_feature_coll = split(trim(elems[3]), ';');
		tc.cpuid_features.reserve(cpuid_feature_coll.size());
		for (std::string_view s : cpuid_feature_coll)
			tc.cpuid_features.push_back(to_cpuid_features(s));

		for (std::string_view kv : split_whitespace(elems[4])) {
			if (trim(kv).empty())
				continue;
			std::string_view key;
			std::string_view value;
			const std::size_t index = kv.find('=');
			if (index != std::string_view::npos) {
				key = kv.substr(0, index);
				value = kv.substr(index + 1);
			} else {
				key = kv;
				value = std::string_view();
			}

			if (key == InstructionInfoKeys::IS_PRIVILEGED) {
				if (!value.empty())
					throw ParseError("Invalid key-value value, '" + std::string(kv) + "'");
				tc.is_privileged = true;
			} else if (key == InstructionInfoKeys::IS_SAVE_RESTORE_INSTRUCTION) {
				if (!value.empty())
					throw ParseError("Invalid key-value value, '" + std::string(kv) + "'");
				tc.is_save_restore_instruction = true;
			} else if (key == InstructionInfoKeys::IS_STACK_INSTRUCTION) {
				tc.is_stack_instruction = true;
				tc.stack_pointer_increment = to_i32(value);
			} else if (key == InstructionInfoKeys::IS_SPECIAL) {
				if (!value.empty())
					throw ParseError("Invalid key-value value, '" + std::string(kv) + "'");
				tc.is_special = true;
			} else if (key == InstructionInfoKeys::RFLAGS_READ)
				tc.rflags_read |= parse_rflags(value);
			else if (key == InstructionInfoKeys::RFLAGS_UNDEFINED)
				tc.rflags_undefined |= parse_rflags(value);
			else if (key == InstructionInfoKeys::RFLAGS_WRITTEN)
				tc.rflags_written |= parse_rflags(value);
			else if (key == InstructionInfoKeys::RFLAGS_CLEARED)
				tc.rflags_cleared |= parse_rflags(value);
			else if (key == InstructionInfoKeys::RFLAGS_SET)
				tc.rflags_set |= parse_rflags(value);
			else if (key == InstructionInfoKeys::FLOW_CONTROL)
				tc.flow_control = to_flow_control(value);
			else if (key == InstructionInfoKeys::OP0_ACCESS)
				tc.op0_access = to_op_access(value);
			else if (key == InstructionInfoKeys::OP1_ACCESS)
				tc.op1_access = to_op_access(value);
			else if (key == InstructionInfoKeys::OP2_ACCESS)
				tc.op2_access = to_op_access(value);
			else if (key == InstructionInfoKeys::OP3_ACCESS)
				tc.op3_access = to_op_access(value);
			else if (key == InstructionInfoKeys::OP4_ACCESS)
				tc.op4_access = to_op_access(value);
			else if (key == InstructionInfoKeys::READ_REGISTER)
				add_registers(value, OpAccess::Read, tc);
			else if (key == InstructionInfoKeys::COND_READ_REGISTER)
				add_registers(value, OpAccess::CondRead, tc);
			else if (key == InstructionInfoKeys::WRITE_REGISTER)
				add_registers(value, OpAccess::Write, tc);
			else if (key == InstructionInfoKeys::COND_WRITE_REGISTER)
				add_registers(value, OpAccess::CondWrite, tc);
			else if (key == InstructionInfoKeys::READ_WRITE_REGISTER)
				add_registers(value, OpAccess::ReadWrite, tc);
			else if (key == InstructionInfoKeys::READ_COND_WRITE_REGISTER)
				add_registers(value, OpAccess::ReadCondWrite, tc);
			else if (key == InstructionInfoKeys::READ_MEMORY)
				add_memory(value, OpAccess::Read, tc);
			else if (key == InstructionInfoKeys::COND_READ_MEMORY)
				add_memory(value, OpAccess::CondRead, tc);
			else if (key == InstructionInfoKeys::READ_WRITE_MEMORY)
				add_memory(value, OpAccess::ReadWrite, tc);
			else if (key == InstructionInfoKeys::READ_COND_WRITE_MEMORY)
				add_memory(value, OpAccess::ReadCondWrite, tc);
			else if (key == InstructionInfoKeys::WRITE_MEMORY)
				add_memory(value, OpAccess::Write, tc);
			else if (key == InstructionInfoKeys::COND_WRITE_MEMORY)
				add_memory(value, OpAccess::CondWrite, tc);
			else if (key == InstructionInfoKeys::DECODER_OPTIONS)
				tc.decoder_options |= parse_decoder_options(value);
			else if (key == InstructionInfoKeys::FPU_TOP_INCREMENT) {
				tc.fpu_top_increment = to_i32(value);
				tc.fpu_writes_top = true;
			} else if (key == InstructionInfoKeys::FPU_CONDITIONAL_TOP)
				tc.fpu_conditional_top = true;
			else if (key == InstructionInfoKeys::FPU_WRITES_TOP)
				tc.fpu_writes_top = true;
			else
				throw ParseError("Invalid key " + std::string(key));
		}

		return tc;
	}

	static std::uint32_t parse_rflags(std::string_view value) {
		std::uint32_t rflags = 0;
		for (char c : value) {
			switch (c) {
			case RflagsBitsConstants::AF:
				rflags |= RflagsBits::AF;
				break;
			case RflagsBitsConstants::CF:
				rflags |= RflagsBits::CF;
				break;
			case RflagsBitsConstants::OF:
				rflags |= RflagsBits::OF;
				break;
			case RflagsBitsConstants::PF:
				rflags |= RflagsBits::PF;
				break;
			case RflagsBitsConstants::SF:
				rflags |= RflagsBits::SF;
				break;
			case RflagsBitsConstants::ZF:
				rflags |= RflagsBits::ZF;
				break;
			case RflagsBitsConstants::IF:
				rflags |= RflagsBits::IF;
				break;
			case RflagsBitsConstants::DF:
				rflags |= RflagsBits::DF;
				break;
			case RflagsBitsConstants::AC:
				rflags |= RflagsBits::AC;
				break;
			case RflagsBitsConstants::UIF:
				rflags |= RflagsBits::UIF;
				break;
			case RflagsBitsConstants::C0:
				rflags |= RflagsBits::C0;
				break;
			case RflagsBitsConstants::C1:
				rflags |= RflagsBits::C1;
				break;
			case RflagsBitsConstants::C2:
				rflags |= RflagsBits::C2;
				break;
			case RflagsBitsConstants::C3:
				rflags |= RflagsBits::C3;
				break;
			default:
				throw ParseError("Invalid flags string: " + std::string(value) + ", char: " + std::string(1, c));
			}
		}
		return rflags;
	}

	OpAccess to_op_access(std::string_view value) const {
		auto it = to_access_.find(value);
		if (it == to_access_.end())
			throw ParseError("Invalid op access: " + std::string(value));
		return it->second;
	}

	Register get_register(std::string_view reg_str, EncodingKind encoding, OpAccess access) const {
		auto it = to_register_.find(std::string(reg_str));
		if (it == to_register_.end())
			throw ParseError("Invalid register: " + std::string(reg_str));
		const Register reg = it->second;
		if (encoding != EncodingKind::Legacy && encoding != EncodingKind::D3NOW) {
			switch (access) {
			case OpAccess::None:
			case OpAccess::Read:
			case OpAccess::NoMemAccess:
			case OpAccess::CondRead:
				break;

			case OpAccess::Write:
			case OpAccess::CondWrite:
			case OpAccess::ReadWrite:
			case OpAccess::ReadCondWrite:
				if (Register::XMM0 <= reg && reg <= IcedConstants::VMM_LAST && !starts_with(reg_str, MiscInstrInfoTestConstants::VMM_PREFIX)) {
					throw ParseError("Register " + std::string(reg_str) + " is written (" + to_string(access) + ") but " +
									 MiscInstrInfoTestConstants::VMM_PREFIX + " pseudo register should be used instead");
				}
				break;
			}
		}
		return reg;
	}

	void add_registers(std::string_view value, OpAccess access, InstrInfoTestCase& tc) const {
		for (std::string_view reg_str : split(value, ';')) {
			reg_str = trim(reg_str);
			if (reg_str.find('-') != std::string_view::npos) {
				const std::vector<std::string_view> reg_parts = splitn(reg_str, 2, '-');
				const Register first_reg = get_register(trim(reg_parts[0]), tc.encoding, access);
				const Register last_reg = get_register(trim(reg_parts[1]), tc.encoding, access);
				if (last_reg < first_reg)
					throw ParseError("Invalid register range: " + std::string(reg_str));
				for (std::uint32_t r = static_cast<std::uint32_t>(first_reg); r <= static_cast<std::uint32_t>(last_reg); r++)
					tc.used_registers.push_back(UsedRegister(static_cast<Register>(r), access));
			} else {
				const Register reg = get_register(reg_str, tc.encoding, access);
				tc.used_registers.push_back(UsedRegister(reg, access));
			}
		}
	}

	void add_memory(std::string_view value, OpAccess access, InstrInfoTestCase& tc) const {
		if (!add_memory_core(value, access, tc))
			throw ParseError("Invalid memory value: '" + std::string(value) + "'");
	}

	bool add_memory_core(std::string_view value, OpAccess access, InstrInfoTestCase& tc) const {
		const std::vector<std::string_view> elems = split(value, ';');
		if (elems.size() != 2)
			return false;
		const std::string_view expr = elems[0];
		const MemorySize memory_size = to_memory_size(elems[1]);
		MemExpr mem;
		if (!try_parse_mem_expr(expr, mem))
			return false;
		std::uint64_t displ = mem.displ;
		const std::int64_t sdispl = static_cast<std::int64_t>(displ);
		switch (mem.address_size) {
		case CodeSize::Code16:
			if (!(std::numeric_limits<std::int16_t>::min() <= sdispl && sdispl <= std::numeric_limits<std::int16_t>::max()) && displ > UINT16_MAX)
				return false;
			displ = static_cast<std::uint16_t>(displ);
			break;
		case CodeSize::Code32:
			if (!(std::numeric_limits<std::int32_t>::min() <= sdispl && sdispl <= std::numeric_limits<std::int32_t>::max()) && displ > UINT32_MAX)
				return false;
			displ = static_cast<std::uint32_t>(displ);
			break;
		case CodeSize::Code64:
			break;
		default:
			throw ParseError("unreachable");
		}
		if (access != OpAccess::NoMemAccess)
			tc.used_memory.push_back(UsedMemory(mem.segment, mem.base, mem.index, mem.scale, displ, memory_size, access, mem.address_size, mem.vsib_size));
		return true;
	}

	struct MemExpr {
		Register segment = Register::None;
		Register base = Register::None;
		Register index = Register::None;
		std::uint32_t scale = 1;
		std::uint64_t displ = 0;
		CodeSize address_size = CodeSize::Unknown;
		std::uint32_t vsib_size = 0;
	};

	std::optional<Register> try_get_register(std::string_view s) const {
		auto it = to_register_.find(std::string(s));
		if (it == to_register_.end())
			return std::nullopt;
		return it->second;
	}

	bool try_parse_mem_expr(std::string_view expr, MemExpr& mem) const {
		mem = MemExpr();

		const std::vector<std::string_view> mem_args = split(expr, '|');
		for (std::size_t i = 1; i < mem_args.size(); i++) {
			const std::string_view option = mem_args[i];
			if (option == MiscInstrInfoTestConstants::MEM_SIZE_OPTION_ADDR16)
				mem.address_size = CodeSize::Code16;
			else if (option == MiscInstrInfoTestConstants::MEM_SIZE_OPTION_ADDR32)
				mem.address_size = CodeSize::Code32;
			else if (option == MiscInstrInfoTestConstants::MEM_SIZE_OPTION_ADDR64)
				mem.address_size = CodeSize::Code64;
			else if (option == MiscInstrInfoTestConstants::MEM_SIZE_OPTION_VSIB32)
				mem.vsib_size = 4;
			else if (option == MiscInstrInfoTestConstants::MEM_SIZE_OPTION_VSIB64)
				mem.vsib_size = 8;
			else
				return false;
		}

		bool has_base = false;
		for (std::string_view s : split(mem_args[0], '+')) {
			bool is_index = has_base;

			const std::size_t colon_index = s.find(':');
			if (colon_index != std::string_view::npos) {
				const std::optional<Register> seg = try_get_register(s.substr(0, colon_index));
				if (!seg)
					return false;
				mem.segment = *seg;
				s = s.substr(colon_index + 1);
				if (!(Register::ES <= mem.segment && mem.segment <= Register::GS))
					return false;
			}
			if (s.find('*') != std::string_view::npos) {
				if (ends_with(s, "*1"))
					mem.scale = 1;
				else if (ends_with(s, "*2"))
					mem.scale = 2;
				else if (ends_with(s, "*4"))
					mem.scale = 4;
				else if (ends_with(s, "*8"))
					mem.scale = 8;
				else
					return false;
				s = s.substr(0, s.size() - 2);
				is_index = true;
			}
			if (const std::optional<Register> reg = try_get_register(s)) {
				if (is_index)
					mem.index = *reg;
				else {
					mem.base = *reg;
					has_base = true;
				}
			} else {
				try {
					mem.displ = to_u64(s);
				}
				catch (const std::exception&) {
					return false;
				}
			}
		}

		if (mem.address_size == CodeSize::Unknown) {
			const Register reg = mem.base != Register::None ? mem.base : mem.index;
			if (register_ext::is_gpr16(reg))
				mem.address_size = CodeSize::Code16;
			else if (register_ext::is_gpr32(reg))
				mem.address_size = CodeSize::Code32;
			else if (register_ext::is_gpr64(reg))
				mem.address_size = CodeSize::Code64;
		}
		if (mem.address_size == CodeSize::Unknown) {
			switch (bitness_) {
			case 16:
				mem.address_size = CodeSize::Code16;
				break;
			case 32:
				mem.address_size = CodeSize::Code32;
				break;
			case 64:
				mem.address_size = CodeSize::Code64;
				break;
			default:
				throw ParseError("unreachable");
			}
		}
		if (mem.vsib_size == 0 && register_ext::is_vector_register(mem.base))
			return false;
		if (mem.vsib_size != 0 && !register_ext::is_vector_register(mem.index))
			return false;

		return mem.segment != Register::None;
	}

	static std::uint32_t parse_decoder_options(std::string_view value) {
		std::uint32_t decoder_options = 0;
		for (std::string_view option_str : split(value, ';')) {
			try {
				decoder_options |= to_decoder_options(option_str);
			}
			catch (const std::exception&) {
				throw ParseError("Invalid decoder options: " + std::string(option_str));
			}
		}
		return decoder_options;
	}

	std::uint32_t bitness_;
	std::unordered_map<std::string, Register> to_register_;
	std::unordered_map<std::string_view, OpAccess> to_access_;
};

} // namespace

std::vector<InstrInfoTestCase> read_instr_info_test_cases(std::uint32_t bitness) {
	const std::string filename = get_instr_info_unit_tests_dir() + "/InstructionInfoTest_" + std::to_string(bitness) + ".txt";
	return InstrInfoTestParser(bitness).read(filename);
}

const std::vector<InstrInfoTestCase>& get_instr_info_test_cases(std::uint32_t bitness) {
	switch (bitness) {
	case 16: {
		static const std::vector<InstrInfoTestCase> test_cases = read_instr_info_test_cases(16);
		return test_cases;
	}
	case 32: {
		static const std::vector<InstrInfoTestCase> test_cases = read_instr_info_test_cases(32);
		return test_cases;
	}
	case 64: {
		static const std::vector<InstrInfoTestCase> test_cases = read_instr_info_test_cases(64);
		return test_cases;
	}
	default:
		throw std::runtime_error("Invalid bitness");
	}
}

} // namespace iced_x86::tests::instr_info
