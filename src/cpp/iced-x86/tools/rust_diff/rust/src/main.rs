// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust side of the differential test (see ../run.sh). Output format must match ../diff.cpp.

use iced_x86::*;
use std::io::Write;
fn main() {
	let args: Vec<String> = std::env::args().collect();
	let data = std::fs::read(&args[1]).unwrap();
	let bitness: u32 = args[2].parse().unwrap();
	let options: u32 = u32::from_str_radix(&args[3], 16).unwrap();
	let out = std::io::stdout();
	let mut out = std::io::BufWriter::new(out.lock());
	let mut masm = MasmFormatter::new();
	let mut nasm = NasmFormatter::new();
	let mut gas = GasFormatter::new();
	let mut intel = IntelFormatter::new();
	let mut fast = FastFormatter::new();
	let mut factory = InstructionInfoFactory::new();
	let mut decoder = Decoder::with_ip(bitness, &data, 0x1234_5678_9ABC_0000u64 & if bitness == 64 { !0 } else { 0xFFFF_0000 }, options);
	let mut s = String::new();
	let mut instr = Instruction::default();
	while decoder.can_decode() {
		let pos = decoder.position();
		decoder.decode_out(&mut instr);
		write!(out, "{} {} {:?} {:?}", pos, instr.len(), instr.code(), decoder.last_error()).unwrap();
		for f in [&mut masm as &mut dyn Formatter, &mut nasm, &mut gas, &mut intel] { s.clear(); f.format(&instr, &mut s); write!(out, "|{}", s).unwrap(); }
		s.clear(); fast.format(&instr, &mut s); write!(out, "|{}", s).unwrap();
		if !instr.is_invalid() {
			let info = factory.info(&instr);
			write!(out, "|r{}m{}", info.used_registers().len(), info.used_memory().len()).unwrap();
			for r in info.used_registers() { write!(out, " {:?}:{:?}", r.register(), r.access()).unwrap(); }
			for m in info.used_memory() { write!(out, " {:?}:{:?}:{:?}", m.base(), m.memory_size(), m.access()).unwrap(); }
			write!(out, "|{:?} {:?} {:?} {:x} {:x}", instr.flow_control(), instr.cpuid_features(), instr.stack_pointer_increment(), instr.rflags_read(), instr.rflags_modified()).unwrap();
			let mut enc = Encoder::new(bitness);
			match enc.encode(&instr, instr.ip()) {
				Ok(n) => { let b = enc.take_buffer(); write!(out, "|e{} ", n).unwrap(); for x in b { write!(out, "{:02X}", x).unwrap(); } }
				Err(e) => { write!(out, "|E{}", e).unwrap(); }
			}
		}
		writeln!(out).unwrap();
	}
}
