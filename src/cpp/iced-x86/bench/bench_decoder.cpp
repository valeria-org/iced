// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Decoder benchmark: decodes the .text section of an x86-64 ELF file N times.
//
// Usage: iced_x86_bench_decoder [elf-file] [loops]
//	elf-file	Default: /usr/lib/x86_64-linux-gnu/libstdc++.so.6 (or this executable if it doesn't exist)
//	loops		Default: 0 = auto (decode at least ~200MB)

#include "iced_x86/decoder.hpp"

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
		loops = static_cast<unsigned long>(200'000'000 / (code.size() + 1)) + 1;

	std::printf("File: %s, .text: %zu bytes, loops: %lu\n", filename, code.size(), loops);

	// Warm up (also creates the decoder tables)
	std::uint64_t instr_count_1 = 0;
	std::uint64_t checksum = 0;
	{
		auto decoder = Decoder::with_ip(64, code.data(), code.size(), text_address, DecoderOptions::NONE);
		Instruction instruction;
		while (decoder.can_decode()) {
			decoder.decode_out(instruction);
			instr_count_1++;
		}
	}

	// decode_out()
	auto start = std::chrono::steady_clock::now();
	std::uint64_t instr_count = 0;
	for (unsigned long i = 0; i < loops; i++) {
		auto decoder = Decoder::with_ip(64, code.data(), code.size(), text_address, DecoderOptions::NONE);
		Instruction instruction;
		while (decoder.can_decode()) {
			decoder.decode_out(instruction);
			checksum += static_cast<std::uint64_t>(instruction.code());
			instr_count++;
		}
	}
	double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	double total_bytes = static_cast<double>(code.size()) * static_cast<double>(loops);
	std::printf("decode_out(): %.3f s, %.1f MB/s, %.2f M instr/s (%llu instrs/loop)\n", secs, total_bytes / secs / 1e6,
				static_cast<double>(instr_count) / secs / 1e6, static_cast<unsigned long long>(instr_count_1));

	// Iterator
	start = std::chrono::steady_clock::now();
	instr_count = 0;
	for (unsigned long i = 0; i < loops; i++) {
		auto decoder = Decoder::with_ip(64, code.data(), code.size(), text_address, DecoderOptions::NONE);
		for (const Instruction& instruction : decoder) {
			checksum += static_cast<std::uint64_t>(instruction.code());
			instr_count++;
		}
	}
	secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	std::printf("iterator:     %.3f s, %.1f MB/s, %.2f M instr/s\n", secs, total_bytes / secs / 1e6, static_cast<double>(instr_count) / secs / 1e6);
	std::printf("(checksum %llu)\n", static_cast<unsigned long long>(checksum));
	return 0;
}
