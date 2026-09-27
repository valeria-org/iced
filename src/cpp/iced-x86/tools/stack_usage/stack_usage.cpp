// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Observed stack usage (see run.sh): every workload runs on a pthread whose stack is a caller-owned, pre-painted mmap region.
// After the thread exits, the deepest overwritten byte gives the max stack depth (library + libc/libstdc++ + thread start).
#include "iced_x86/iced_x86.hpp"
#include "iced_x86/code_asm.hpp"
#include <pthread.h>
#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
using namespace iced_x86;

#define NOINLINE __attribute__((noinline))
static constexpr std::size_t STACK_SIZE = 1024 * 1024;
static std::vector<std::uint8_t> g_code, g_rand;
static std::uint64_t g_addr;
static volatile std::uint64_t g_sink;
static std::uint8_t* g_api_sp;
// Every library call goes through api(): records the stack pointer at the API boundary so the library's own stack
// usage (API call -> deepest point) can be separated from the harness' frames (formatter/decoder objects etc.)
template <class F> NOINLINE static void api(F&& f) {
	auto* sp = static_cast<std::uint8_t*>(__builtin_frame_address(0));
	if (g_api_sp == nullptr || sp > g_api_sp) g_api_sp = sp;
	f();
}

struct Resolver final : SymbolResolver {
	std::string name = std::string(40, 's');
	std::optional<SymbolResult> symbol(const Instruction&, std::uint32_t, std::optional<std::uint32_t>, std::uint64_t address, std::uint32_t) override {
		if ((address & 3) != 0)
			return std::nullopt;
		return SymbolResult::with_str(address, name);
	}
};
struct NullOut final : FormatterOutput {
	std::size_t n = 0;
	void write(std::string_view t, FormatterTextKind) override { n += t.size(); }
};

template <class F> static void for_each_instr(F f) {
	{
		auto d = Decoder::with_ip(64, g_code.data(), g_code.size(), g_addr, 0);
		Instruction i; while (d.can_decode()) { api([&] { d.decode_out(i); }); f(i); }
	}
	for (std::uint32_t b : {16u, 32u, 64u}) {
		auto d = Decoder::with_ip(b, g_rand.data(), g_rand.size(), 0x1234'0000, DecoderOptions::KNC);
		Instruction i; while (d.can_decode()) { api([&] { d.decode_out(i); }); f(i); }
	}
}
template <class Fmt> static void fmt_string(Fmt& f) {
	std::string s;
	for_each_instr([&](const Instruction& i) { s.clear(); api([&] { f.format(i, s); }); g_sink += s.size(); });
}
template <class Fmt> static void fmt_output(Fmt& f) {
	NullOut o;
	for_each_instr([&](const Instruction& i) { api([&] { f.format(i, o); }); });
	g_sink += o.n;
}

NOINLINE static void w_baseline() { g_sink += 1; }
NOINLINE static void w_decode() { for_each_instr([](const Instruction& i) { g_sink += i.len(); }); }
NOINLINE static void w_instr_info() { InstructionInfoFactory f; for_each_instr([&](const Instruction& i) { api([&] { g_sink += f.info(i).used_registers().size(); }); }); }
NOINLINE static void w_encode() {
	Encoder e(64);
	for_each_instr([&](const Instruction& i) { api([&] { auto r = e.encode(i, i.ip() + 0x1000'0000); g_sink += r.is_ok() ? r.value() : 1; }); });
}
NOINLINE static void w_encode_valid() {
	Encoder e(64);
	auto d = Decoder::with_ip(64, g_code.data(), g_code.size(), g_addr, 0);
	for (auto& i : d) api([&] { auto r = e.encode(i, i.ip()); g_sink += r.is_ok() ? r.value() : 1; });
}
NOINLINE static void w_decode_one() {
	static const std::uint8_t bytes[] = {0x48, 0x89, 0x5C, 0x24, 0x10};
	Decoder d(64, bytes, 0);
	api([&] { g_sink += d.decode().len(); });
}
NOINLINE static void w_op_code_info() { for_each_instr([](const Instruction& i) { api([&] { g_sink += code_ext::op_code(i.code()).op_code_string().size(); }); }); }
NOINLINE static void w_fast() {
	FastFormatter f;
	char buf[FastFormatter::MAX_FMT_INSTR_LEN + 1];
	for_each_instr([&](const Instruction& i) { api([&] { g_sink += f.format(i, buf); }); });
}
NOINLINE static void w_fast_string() { FastFormatter f; fmt_string(f); }
NOINLINE static void w_fast_sym() {
	auto f = *FastFormatter::try_with_options(std::make_unique<Resolver>());
	char buf[FastFormatter::MAX_FMT_INSTR_LEN + 1];
	for_each_instr([&](const Instruction& i) { api([&] { g_sink += f.format(i, buf); }); });
}
NOINLINE static void w_gas() { GasFormatter f; fmt_string(f); }
NOINLINE static void w_gas_out() { GasFormatter f; fmt_output(f); }
NOINLINE static void w_gas_sym() { GasFormatter f(std::make_unique<Resolver>(), nullptr); fmt_string(f); }
NOINLINE static void w_intel() { IntelFormatter f; fmt_string(f); }
NOINLINE static void w_intel_sym() { IntelFormatter f(std::make_unique<Resolver>(), nullptr); fmt_string(f); }
NOINLINE static void w_masm() { MasmFormatter f; fmt_string(f); }
NOINLINE static void w_masm_out() { MasmFormatter f; fmt_output(f); }
NOINLINE static void w_masm_sym() { MasmFormatter f(std::make_unique<Resolver>(), nullptr); fmt_string(f); }
NOINLINE static void w_nasm() { NasmFormatter f; fmt_string(f); }
NOINLINE static void w_nasm_sym() { NasmFormatter f(std::make_unique<Resolver>(), nullptr); fmt_string(f); }
NOINLINE static void w_block_encoder() {
	std::vector<Instruction> v;
	auto d = Decoder::with_ip(64, g_code.data(), std::min<std::size_t>(g_code.size(), 200000), g_addr, 0);
	for (auto& i : d)
		if (!i.is_invalid()) v.push_back(i);
	g_api_sp = nullptr;
	api([&] {
		auto r = BlockEncoder::encode(64, InstructionBlock(v, g_addr + 0x7000'0000),
			BlockEncoderOptions::RETURN_NEW_INSTRUCTION_OFFSETS | BlockEncoderOptions::RETURN_RELOC_INFOS);
		g_sink += r.is_ok() ? r.value().code_buffer.size() : 0;
	});
}
NOINLINE static void w_code_asm() {
	using namespace code_asm;
	CodeAssembler a(64);
	g_api_sp = nullptr;
	auto l1 = a.create_label();
	auto l2 = a.create_label();
	for (int k = 0; k < 2000; k++) {
		api([&] { a.push(rbp); }); api([&] { a.mov(rbp, rsp); }); api([&] { a.sub(rsp, 0x40); });
		api([&] { a.mov(rax, qword_ptr(rbp + rcx * 8 - 0x10)); }); api([&] { a.vaddps(zmm1.k1().z(), zmm2, zmm3); });
		api([&] { a.je(l1); }); api([&] { a.call(l2); }); api([&] { a.lea(rdx, ptr(l1)); }); api([&] { a.mov(rax, 0x1234'5678'9ABC'DEF0ULL); });
	}
	a.set_label(l1); a.nop(); a.set_label(l2); a.ret();
	api([&] { auto r = a.assemble(0x1000); g_sink += r.is_ok() ? r.value().size() : 0; });
}

struct Work { const char* name; void (*fn)(); };
static const Work WORKS[] = {
	{"baseline", w_baseline}, {"decode", w_decode}, {"instr_info", w_instr_info}, {"encode", w_encode}, {"encode_valid", w_encode_valid}, {"decode_one", w_decode_one}, {"op_code_info", w_op_code_info},
	{"fast", w_fast}, {"fast_string", w_fast_string}, {"fast_sym", w_fast_sym}, {"gas", w_gas}, {"gas_out", w_gas_out}, {"gas_sym", w_gas_sym},
	{"intel", w_intel}, {"intel_sym", w_intel_sym}, {"masm", w_masm}, {"masm_out", w_masm_out}, {"masm_sym", w_masm_sym},
	{"nasm", w_nasm}, {"nasm_sym", w_nasm_sym}, {"block_encoder", w_block_encoder}, {"code_asm", w_code_asm},
};
static void* g_mem;
static std::size_t g_depth, g_api_depth;
NOINLINE static void run_measured(void (*fn)()) {
	// Paint everything below this frame (our own mmap'ed stack), run the workload, find the deepest written byte
	auto* sp = static_cast<std::uint8_t*>(__builtin_frame_address(0));
	auto* lo = static_cast<std::uint8_t*>(g_mem);
	std::size_t n = static_cast<std::size_t>(sp - lo) - 64;
	for (std::size_t i = 0; i < n; i++) static_cast<volatile std::uint8_t*>(g_mem)[i] = 0xCD;
	fn();
	std::size_t i = 0;
	while (i < n && lo[i] == 0xCD) i++;
	g_depth = static_cast<std::size_t>(sp - (lo + i));
	g_api_depth = g_api_sp != nullptr ? static_cast<std::size_t>(g_api_sp - (lo + i)) : 0;
	if (getenv("SHOW_ADDR")) std::printf("deepest=%p\n", static_cast<void*>(lo + i));
}
static void* thread_main(void* arg) {
	for (const auto& w : WORKS)
		if (std::strcmp(w.name, static_cast<const char*>(arg)) == 0) {
			// Warm up the thread's malloc arena (glibc creates it on the first malloc() in a thread, which uses a lot of stack)
			for (std::size_t sz : {16, 100, 1000, 10000, 100000, 1000000}) std::free(std::malloc(sz));
			if (getenv("NO_WARMUP") == nullptr) { std::string s(1000, 'x'); g_sink += s.size(); }
			run_measured(w.fn);
			return nullptr;
		}
	std::exit(2);
}

int main(int argc, char** argv) {
	if (argc < 2) return 1;
	std::ifstream f("/usr/lib/x86_64-linux-gnu/libstdc++.so.6", std::ios::binary);
	std::vector<std::uint8_t> elf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	std::uint64_t shoff; std::memcpy(&shoff, &elf[0x28], 8);
	std::uint16_t shentsize, shnum, shstrndx;
	std::memcpy(&shentsize, &elf[0x3A], 2); std::memcpy(&shnum, &elf[0x3C], 2); std::memcpy(&shstrndx, &elf[0x3E], 2);
	std::uint64_t strtab; std::memcpy(&strtab, &elf[shoff + std::size_t{shstrndx} * shentsize + 0x18], 8);
	for (int i = 0; i < shnum; i++) {
		std::size_t sh = shoff + std::size_t(i) * shentsize;
		std::uint32_t nm; std::memcpy(&nm, &elf[sh], 4);
		if (std::strcmp(reinterpret_cast<const char*>(&elf[strtab + nm]), ".text") == 0) {
			std::uint64_t off, sz;
			std::memcpy(&g_addr, &elf[sh + 0x10], 8); std::memcpy(&off, &elf[sh + 0x18], 8); std::memcpy(&sz, &elf[sh + 0x20], 8);
			g_code.assign(elf.begin() + off, elf.begin() + off + sz);
		}
	}
	std::uint64_t x = 0x9E3779B97F4A7C15;
	g_rand.resize(1 << 20);
	for (auto& b : g_rand) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; b = static_cast<std::uint8_t>(x); }

	void* mem = g_mem = mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	std::memset(mem, 0xCD, STACK_SIZE);
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstack(&attr, mem, STACK_SIZE);
	pthread_t t;
	pthread_create(&t, &attr, thread_main, argv[1]);
	pthread_join(t, nullptr);
	std::printf("%s %zu %zu\n", argv[1], g_depth, g_api_depth);
}
