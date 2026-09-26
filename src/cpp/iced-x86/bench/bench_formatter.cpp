// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Formatter benchmark: decodes + formats the .text section of an x86-64 ELF file N times.
//
// Usage: iced_x86_bench_formatter [elf-file] [loops]
//	elf-file	Default: /usr/lib/x86_64-linux-gnu/libstdc++.so.6 (or this executable if it doesn't exist)
//	loops		Default: 0 = auto (decode at least ~50MB)

#include "iced_x86/decoder.hpp"
#include "iced_x86/fast_formatter.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

bool read_file(const char* filename, std::vector<std::uint8_t>& data) {
	std::FILE* f = std::fopen(filename, "rb");
	if (f == nullptr)
		return false;
	std::uint8_t buf[0x1000];
	for (;;) {
		std::size_t n = std::fread(buf, 1, sizeof(buf), f);
		if (n == 0)
			break;
		data.insert(data.end(), buf, buf + n);
	}
	std::fclose(f);
	return true;
}

template <typename T>
T read_at(const std::vector<std::uint8_t>& data, std::size_t offset) {
	T value{};
	if (offset + sizeof(T) <= data.size())
		std::memcpy(&value, data.data() + offset, sizeof(T));
	return value;
}

// Finds the .text section of an ELF64 (little endian) file
bool get_text_section(const std::vector<std::uint8_t>& elf, std::size_t& offset, std::size_t& size, std::uint64_t& address) {
	if (elf.size() < 0x40 || std::memcmp(elf.data(), "\x7F" "ELF", 4) != 0 || elf[4] != 2 || elf[5] != 1)
		return false;
	auto shoff = read_at<std::uint64_t>(elf, 0x28);
	auto shentsize = read_at<std::uint16_t>(elf, 0x3A);
	auto shnum = read_at<std::uint16_t>(elf, 0x3C);
	auto shstrndx = read_at<std::uint16_t>(elf, 0x3E);
	if (shstrndx >= shnum)
		return false;
	auto strtab_offset = read_at<std::uint64_t>(elf, shoff + std::size_t{shstrndx} * shentsize + 0x18);
	for (std::uint32_t i = 0; i < shnum; i++) {
		std::size_t sh = shoff + std::size_t{i} * shentsize;
		auto name_offset = read_at<std::uint32_t>(elf, sh);
		std::size_t name_pos = strtab_offset + name_offset;
		if (name_pos + 6 > elf.size() || std::memcmp(elf.data() + name_pos, ".text", 6) != 0)
			continue;
		address = read_at<std::uint64_t>(elf, sh + 0x10);
		offset = read_at<std::uint64_t>(elf, sh + 0x18);
		size = read_at<std::uint64_t>(elf, sh + 0x20);
		return offset + size <= elf.size();
	}
	return false;
}

// Hard coded options: the fastest possible formatter
struct HardCodedTraitOptions : iced_x86::SpecializedFormatterTraitOptions {
	static constexpr bool ENABLE_DB_DW_DD_DQ = false;
	static constexpr bool verify_output_has_enough_bytes_left() noexcept { return false; }
};

// Decodes + formats all instructions `loops` times. `format` is called with each instruction and the output string
template <typename F>
void bench(const char* name, const std::vector<std::uint8_t>& code, std::uint64_t address, unsigned long loops, F&& format) {
	using namespace iced_x86;
	std::uint64_t checksum = 0;
	std::string output;
	output.reserve(512);
	// Warm up (also creates the tables)
	{
		auto decoder = Decoder::with_ip(64, code.data(), code.size(), address, DecoderOptions::NONE);
		Instruction instruction;
		while (decoder.can_decode()) {
			decoder.decode_out(instruction);
			output.clear();
			format(instruction, output);
		}
	}

	auto start = std::chrono::steady_clock::now();
	std::uint64_t instr_count = 0;
	for (unsigned long i = 0; i < loops; i++) {
		auto decoder = Decoder::with_ip(64, code.data(), code.size(), address, DecoderOptions::NONE);
		Instruction instruction;
		while (decoder.can_decode()) {
			decoder.decode_out(instruction);
			output.clear();
			format(instruction, output);
			checksum += output.size();
			instr_count++;
		}
	}
	const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	const double total_bytes = static_cast<double>(code.size()) * static_cast<double>(loops);
	std::printf("%-22s %.3f s, %.1f MB/s, %.2f M instr/s (checksum %llu)\n", name, secs, total_bytes / secs / 1e6,
				static_cast<double>(instr_count) / secs / 1e6, static_cast<unsigned long long>(checksum));
}

} // namespace

int main(int argc, char** argv) {
	using namespace iced_x86;
	const char* filename = argc >= 2 ? argv[1] : "/usr/lib/x86_64-linux-gnu/libstdc++.so.6";
	std::vector<std::uint8_t> elf;
	if (!read_file(filename, elf)) {
		filename = "/proc/self/exe";
		if (!read_file(filename, elf)) {
			std::fprintf(stderr, "Couldn't read the ELF file\n");
			return 1;
		}
	}
	std::size_t text_offset = 0, text_size = 0;
	std::uint64_t text_address = 0;
	if (!get_text_section(elf, text_offset, text_size, text_address)) {
		std::fprintf(stderr, "Couldn't find the .text section in %s\n", filename);
		return 1;
	}
	std::vector<std::uint8_t> code(elf.begin() + static_cast<std::ptrdiff_t>(text_offset),
								   elf.begin() + static_cast<std::ptrdiff_t>(text_offset + text_size));
	unsigned long loops = argc >= 3 ? std::strtoul(argv[2], nullptr, 0) : 0;
	if (loops == 0)
		loops = static_cast<unsigned long>(50'000'000 / (code.size() + 1)) + 1;

	std::printf("File: %s, .text: %zu bytes, loops: %lu\n", filename, code.size(), loops);

	bench("decode only:", code, text_address, loops, [](const Instruction&, std::string&) {});

	FastFormatter fast_formatter;
	bench("FastFormatter:", code, text_address, loops,
		  [&fast_formatter](const Instruction& instruction, std::string& output) { fast_formatter.format(instruction, output); });

	SpecializedFormatter<HardCodedTraitOptions> specialized_formatter;
	bench("SpecializedFormatter:", code, text_address, loops, [&specialized_formatter](const Instruction& instruction, std::string& output) {
		specialized_formatter.format(instruction, output);
	});

	return 0;
}
