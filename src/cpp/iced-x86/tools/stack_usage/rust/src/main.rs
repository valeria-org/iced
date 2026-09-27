// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Same measurement as the C++ harness: paint the stack below the API boundary, run, find the deepest written byte.
use iced_x86::code_asm::*;
use iced_x86::*;
use std::cell::Cell;
use std::hint::black_box;

const PAINT: usize = 256 * 1024;
thread_local!(static API_SP: Cell<usize> = Cell::new(0));

#[inline(never)]
fn api<R>(f: impl FnOnce() -> R) -> R {
	let marker = 0u8;
	let sp_addr = &marker as *const u8 as usize;
	API_SP.with(|c| if c.get() == 0 || sp_addr > c.get() { c.set(sp_addr) });
	black_box(f())
}

#[inline(never)]
fn paint() -> usize {
	let marker = 0u8;
	let top = &marker as *const u8 as usize - 1024;
	unsafe { for i in 0..PAINT { std::ptr::write_volatile((top - PAINT + i) as *mut u8, 0xCD) } }
	top
}
#[inline(never)]
fn deepest(top: usize) -> usize {
	let mut i = 0;
	unsafe { while i < PAINT && std::ptr::read_volatile((top - PAINT + i) as *const u8) == 0xCD { i += 1 } }
	top - PAINT + i
}

struct Resolver { name: String }
impl SymbolResolver for Resolver {
	fn symbol(&mut self, _: &Instruction, _: u32, _: Option<u32>, address: u64, _: u32) -> Option<SymbolResult<'_>> {
		if address & 3 != 0 { None } else { Some(SymbolResult::with_str(address, &self.name)) }
	}
}

fn for_each(code: &[u8], addr: u64, rnd: &[u8], mut f: impl FnMut(&Instruction)) {
	let mut d = Decoder::with_ip(64, code, addr, 0);
	let mut i = Instruction::default();
	while d.can_decode() { api(|| d.decode_out(&mut i)); f(&i); }
	for b in [16, 32, 64] {
		let mut d = Decoder::with_ip(b, rnd, 0x1234_0000, DecoderOptions::KNC);
		while d.can_decode() { api(|| d.decode_out(&mut i)); f(&i); }
	}
}
fn fmt<F: Formatter>(code: &[u8], addr: u64, rnd: &[u8], mut f: F) {
	let mut s = String::new();
	for_each(code, addr, rnd, |i| { s.clear(); api(|| f.format(i, &mut s)); });
}

fn run(w: &str, code: &[u8], addr: u64, rnd: &[u8]) {
	let sym = || Some(Box::new(Resolver { name: "s".repeat(40) }) as Box<dyn SymbolResolver>);
	match w {
		"decode" => for_each(code, addr, rnd, |i| { black_box(i.len()); }),
		"instr_info" => { let mut f = InstructionInfoFactory::new(); for_each(code, addr, rnd, |i| { api(|| black_box(f.info(i).used_registers().len())); }) }
		"encode" => { let mut e = Encoder::new(64); for_each(code, addr, rnd, |i| { let _ = api(|| e.encode(i, i.ip() + 0x1000_0000)); }) }
		"fast" => { let mut f = FastFormatter::new(); let mut s = String::new(); for_each(code, addr, rnd, |i| { s.clear(); api(|| f.format(i, &mut s)); }) }
		"fast_sym" => { let mut f = FastFormatter::try_with_options(sym()).unwrap(); let mut s = String::new(); for_each(code, addr, rnd, |i| { s.clear(); api(|| f.format(i, &mut s)); }) }
		"gas" => fmt(code, addr, rnd, GasFormatter::new()),
		"gas_sym" => fmt(code, addr, rnd, GasFormatter::with_options(sym(), None)),
		"intel" => fmt(code, addr, rnd, IntelFormatter::new()),
		"intel_sym" => fmt(code, addr, rnd, IntelFormatter::with_options(sym(), None)),
		"masm" => fmt(code, addr, rnd, MasmFormatter::new()),
		"masm_sym" => fmt(code, addr, rnd, MasmFormatter::with_options(sym(), None)),
		"nasm" => fmt(code, addr, rnd, NasmFormatter::new()),
		"nasm_sym" => fmt(code, addr, rnd, NasmFormatter::with_options(sym(), None)),
		"block_encoder" => {
			let mut v = Vec::new();
			let mut d = Decoder::with_ip(64, &code[..200000.min(code.len())], addr, 0);
			for i in &mut d { if !i.is_invalid() { v.push(i) } }
			API_SP.with(|c| c.set(0));
			api(|| { let _ = BlockEncoder::encode(64, InstructionBlock::new(&v, addr + 0x7000_0000), BlockEncoderOptions::RETURN_NEW_INSTRUCTION_OFFSETS | BlockEncoderOptions::RETURN_RELOC_INFOS); });
		}
		"code_asm" => {
			let mut a = CodeAssembler::new(64).unwrap();
			API_SP.with(|c| c.set(0));
			let mut l1 = a.create_label();
			let mut l2 = a.create_label();
			for _ in 0..2000 {
				api(|| a.push(rbp)).unwrap(); api(|| a.mov(rbp, rsp)).unwrap(); api(|| a.sub(rsp, 0x40)).unwrap();
				api(|| a.mov(rax, qword_ptr(rbp + rcx * 8 - 0x10))).unwrap(); api(|| a.vaddps(zmm1.k1().z(), zmm2, zmm3)).unwrap();
				api(|| a.je(l1)).unwrap(); api(|| a.call(l2)).unwrap(); api(|| a.lea(rdx, ptr(l1))).unwrap(); api(|| a.mov(rax, 0x1234_5678_9ABC_DEF0u64)).unwrap();
			}
			a.set_label(&mut l1).unwrap(); a.nop().unwrap(); a.set_label(&mut l2).unwrap(); a.ret().unwrap();
			let _ = api(|| a.assemble(0x1000));
		}
		_ => panic!("unknown"),
	}
}

fn main() {
	let w = std::env::args().nth(1).unwrap();
	let elf = std::fs::read("/usr/lib/x86_64-linux-gnu/libstdc++.so.6").unwrap();
	let rd = |o: usize, n: usize| { let mut v = 0u64; for i in 0..n { v |= (elf[o + i] as u64) << (8 * i) } v };
	let (shoff, shentsize, shnum, shstrndx) = (rd(0x28, 8) as usize, rd(0x3A, 2) as usize, rd(0x3C, 2) as usize, rd(0x3E, 2) as usize);
	let strtab = rd(shoff + shstrndx * shentsize + 0x18, 8) as usize;
	let (mut addr, mut off, mut size) = (0, 0, 0);
	for i in 0..shnum { let sh = shoff + i * shentsize; let nm = rd(sh, 4) as usize + strtab; if &elf[nm..nm + 6] == b".text\0" { addr = rd(sh + 0x10, 8); off = rd(sh + 0x18, 8) as usize; size = rd(sh + 0x20, 8) as usize; } }
	let code = elf[off..off + size].to_vec();
	let mut x: u64 = 0x9E3779B97F4A7C15;
	let rnd: Vec<u8> = (0..1 << 20).map(|_| { x ^= x << 13; x ^= x >> 7; x ^= x << 17; x as u8 }).collect();
	let h = std::thread::Builder::new().stack_size(4 << 20).spawn(move || {
		if std::env::var("WARM").is_ok() {
			// Create the lazily initialized tables first (decoder, encoder, formatters) so only steady state is measured
			run(&w, &code[..4096], addr, &rnd[..4096]);
			API_SP.with(|c| c.set(0));
		}
		let top = paint();
		run(&w, &code, addr, &rnd);
		let d = deepest(top);
		let api_sp = API_SP.with(|c| c.get());
		println!("{} {}", w, api_sp - d);
	}).unwrap();
	h.join().unwrap();
}
