// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// C++ side of the differential test (see run.sh). Output format must match rust/src/main.rs.

#include "iced_x86/iced_x86.hpp"
#include "iced_x86/decoder.hpp"
#include "iced_x86/encoder.hpp"
#include "iced_x86/instruction_info.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using namespace iced_x86;
int main(int argc, char** argv) {
	std::ifstream f(argv[1], std::ios::binary);
	std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	std::uint32_t bitness = std::stoul(argv[2]);
	std::uint32_t options = std::stoul(argv[3], nullptr, 16);
	MasmFormatter masm; NasmFormatter nasm; GasFormatter gas; IntelFormatter intel; FastFormatter fast;
	InstructionInfoFactory factory;
	std::uint64_t ip = 0x123456789ABC0000ULL & (bitness == 64 ? ~0ULL : 0xFFFF0000ULL);
	auto decoder = Decoder::with_ip(bitness, data.data(), data.size(), ip, options);
	std::string s, line;
	Instruction instr;
	char buf[64];
	while (decoder.can_decode()) {
		auto pos = decoder.position();
		decoder.decode_out(instr);
		line = std::to_string(pos) + " " + std::to_string(instr.len()) + " " + to_string(instr.code()) + " " + to_string(decoder.last_error());
		Formatter* fs[] = {&masm, &nasm, &gas, &intel};
		for (auto* fm : fs) { s.clear(); fm->format(instr, s); line += "|" + s; }
		s.clear(); fast.format(instr, s); line += "|" + s;
		if (!instr.is_invalid()) {
			const auto& info = factory.info(instr);
			line += "|r" + std::to_string(info.used_registers().size()) + "m" + std::to_string(info.used_memory().size());
			for (auto& r : info.used_registers()) line += std::string(" ") + to_string(r.register_()) + ":" + to_string(r.access());
			for (auto& m : info.used_memory()) line += std::string(" ") + to_string(m.base()) + ":" + to_string(m.memory_size()) + ":" + to_string(m.access());
			line += std::string("|") + to_string(instr.flow_control()) + " [";
			auto cf = instr.cpuid_features();
			for (std::size_t i = 0; i < cf.size(); i++) { if (i) line += ", "; line += to_string(cf[i]); }
			std::snprintf(buf, sizeof buf, "] %d %x %x", (int)instr.stack_pointer_increment(), instr.rflags_read(), instr.rflags_modified());
			line += buf;
			Encoder enc(bitness);
			auto r = enc.encode(instr, instr.ip());
			if (r.is_ok()) { auto b = enc.take_buffer(); line += "|e" + std::to_string(r.value()) + " "; for (auto x : b) { std::snprintf(buf, sizeof buf, "%02X", x); line += buf; } }
			else line += std::string("|E") + r.error().message();
		}
		line += "\n";
		std::fputs(line.c_str(), stdout);
	}
}
