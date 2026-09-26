// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Nasm formatter instruction infos. The layout of each kind must match the C++ classes in <c>src/formatter/nasm/info.cpp</c>
	/// </summary>
	sealed class CppNasmInstrInfoTableGen : CppInstrInfoTableGen {
		static readonly string[] Kinds = new[] {
			"Simple", "cc", "push_imm8", "push_imm", "SignExt", "imul", "AamAad", "String", "XLAT", "nop", "STIG1", "STIG2", "as", "maskmovq",
			"pblendvb", "reverse", "OpSize", "OpSize2_bnd", "OpSize3", "os", "os_mem", "os_mem2", "os_mem_reg16", "os_jcc", "os_loop", "os_call",
			"far", "far_mem", "movabs", "er", "sae", "bcst", "bnd", "pops", "pclmulqdq", "Reg16", "Reg32", "invlpga", "DeclareData",
		};

		// C++ NO_CC_INDEX_U8
		const int NoCcIndex = 0xFF;

		readonly uint registerToFlag;

		public CppNasmInstrInfoTableGen(GenTypes genTypes)
			: base(genTypes, "nasm", genTypes.GetObject<Nasm.CtorInfos>(TypeIds.NasmCtorInfos).Infos, Kinds) =>
			registerToFlag = genTypes[TypeIds.NasmInstrOpInfoFlags]["RegisterTo"].Value;

		// Nasm always appends the char
		static string Append(string s, char c) => s + c;

		// Kind                         mnemonic   arg1                         arg2                arg3
		// Simple                       mnemonic                                flags
		// cc                           mnemonic   ARGS: mnemonics                                  cc_index
		// push_imm8, push_imm          mnemonic                                sign_extend_info    bitness
		// SignExt                      mnemonic   sign_extend_info_reg         flags               sign_extend_info_mem
		// imul                         mnemonic                                                    sign_extend_info
		// AamAad, String, XLAT         mnemonic
		// nop                          mnemonic                                register            bitness
		// STIG1                        mnemonic                                                    pseudo_op
		// STIG2                        mnemonic   flags == REGISTER_TO                             pseudo_op
		// as                           mnemonic                                                    bitness
		// maskmovq, reverse, movabs    mnemonic
		// pblendvb                     mnemonic                                memory_size
		// OpSize                       mnemonic   ARGS: mnemonic16/32/64                           code_size
		// OpSize2_bnd                  mnemonic   ARGS: mnemonic16/32/64
		// OpSize3                      mnemonic   mnemonic_full                                    bitness
		// os                           mnemonic                                flags               bitness
		// os_mem, os_mem_reg16         mnemonic                                                    bitness
		// os_mem2                      mnemonic                                flags               bitness
		// os_jcc                       mnemonic   ARGS: flags, mnemonics       bitness             cc_index
		// os_loop                      mnemonic   ARGS: register, mnemonics    bitness             cc_index (0xFF = none)
		// os_call                      mnemonic   can_have_bnd_prefix                              bitness
		// far, far_mem                 mnemonic                                                    bitness
		// er                           mnemonic                                flags               er_index
		// sae                          mnemonic                                                    sae_index
		// bcst                         mnemonic                                flags_no_broadcast
		// bnd                          mnemonic                                flags
		// pops, pclmulqdq              mnemonic                                                    pseudo_ops_kind
		// Reg16, Reg32                 mnemonic
		// invlpga                      mnemonic                                                    bitness
		// DeclareData                  mnemonic
		protected override Entry Create(FmtInstructionDef def, string ctorKind, ArgReader r) {
			var s = def.Mnemonic;
			char c;
			uint v, v2, v3;
			bool b;
			string s2, s3, s4;
			switch (ctorKind) {
			case "Normal_1":
				return E("Simple", Str(s));
			case "Normal_2":
				v = r.U();
				return E("Simple", Str(s), 0, (int)v);
			case "AamAad":
				return E("AamAad", Str(s));
			case "asz":
				v = r.U();
				VerifyBitness(v);
				return E("as", Str(s), 0, 0, (int)v);
			case "String":
				return E("String", Str(s));
			case "bcst":
				v = r.U();
				return E("bcst", Str(s), 0, (int)v);
			case "bnd":
				v = r.U();
				return E("bnd", Str(s), 0, (int)v);
			case "DeclareData":
				return E("DeclareData", Str(s));
			case "er_2":
				v = r.U();
				return E("er", Str(s), 0, 0, (int)v);
			case "er_3":
				v = r.U();
				v2 = r.U();
				return E("er", Str(s), 0, (int)v2, (int)v);
			case "far":
				v = r.U();
				VerifyBitness(v);
				return E("far", Str(s), 0, 0, (int)v);
			case "far_mem":
				v = r.U();
				VerifyBitness(v);
				return E("far_mem", Str(s), 0, 0, (int)v);
			case "invlpga":
				v = r.U();
				VerifyBitness(v);
				return E("invlpga", Str(s), 0, 0, (int)v);
			case "maskmovq":
				return E("maskmovq", Str(s));
			case "movabs":
				return E("movabs", Str(s));
			case "nop":
				v = r.U();
				v2 = r.U();
				return E("nop", Str(s), 0, (int)v2, (int)v);
			case "OpSize":
				v = r.U();
				return E("OpSize", Str(s), Args(Str(Append(s, 'w')), Str(Append(s, 'd')), Str(Append(s, 'q'))), 0, (int)v);
			case "OpSize2_bnd":
				s2 = r.S();
				s3 = r.S();
				s4 = r.S();
				return E("OpSize2_bnd", Str(s), Args(Str(s2), Str(s3), Str(s4)));
			case "OpSize3":
				c = r.C();
				v = r.U();
				return E("OpSize3", Str(s), Str(Append(s, c)), 0, (int)v);
			case "os_2":
				v = r.U();
				VerifyBitness(v);
				return E("os", Str(s), 0, 0, (int)v);
			case "os_3":
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				return E("os", Str(s), 0, (int)v2, (int)v);
			case "os_call":
				v = r.U();
				b = r.B();
				VerifyBitness(v);
				return E("os_call", Str(s), ToInt(b), 0, (int)v);
			case "CC_1":
				v = r.U();
				VerifyCcCount(v, 1);
				return E("cc", Str(s), Args(Str(s)), 0, (int)v);
			case "CC_2":
				s2 = r.S();
				v = r.U();
				VerifyCcCount(v, 2);
				return E("cc", Str(s), Args(Str(s), Str(s2)), 0, (int)v);
			case "CC_3":
				s2 = r.S();
				s3 = r.S();
				v = r.U();
				VerifyCcCount(v, 3);
				return E("cc", Str(s), Args(Str(s), Str(s2), Str(s3)), 0, (int)v);
			case "os_jcc_a_1":
				v2 = r.U();
				v = r.U();
				VerifyBitness(v);
				VerifyCcCount(v2, 1);
				return E("os_jcc", Str(s), Args(0, Str(s)), (int)v, (int)v2);
			case "os_jcc_a_2":
				s2 = r.S();
				v2 = r.U();
				v = r.U();
				VerifyBitness(v);
				VerifyCcCount(v2, 2);
				return E("os_jcc", Str(s), Args(0, Str(s), Str(s2)), (int)v, (int)v2);
			case "os_jcc_a_3":
				s2 = r.S();
				s3 = r.S();
				v2 = r.U();
				v = r.U();
				VerifyBitness(v);
				VerifyCcCount(v2, 3);
				return E("os_jcc", Str(s), Args(0, Str(s), Str(s2), Str(s3)), (int)v, (int)v2);
			case "os_jcc_b_1":
				v3 = r.U();
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				VerifyCcCount(v3, 1);
				return E("os_jcc", Str(s), Args((int)v2, Str(s)), (int)v, (int)v3);
			case "os_jcc_b_2":
				s2 = r.S();
				v3 = r.U();
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				VerifyCcCount(v3, 2);
				return E("os_jcc", Str(s), Args((int)v2, Str(s), Str(s2)), (int)v, (int)v3);
			case "os_jcc_b_3":
				s2 = r.S();
				s3 = r.S();
				v3 = r.U();
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				VerifyCcCount(v3, 3);
				return E("os_jcc", Str(s), Args((int)v2, Str(s), Str(s2), Str(s3)), (int)v, (int)v3);
			case "os_loopcc":
				s2 = r.S();
				v3 = r.U();
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				VerifyCcCount(v3, 2);
				return E("os_loop", Str(s), Args((int)v2, Str(s), Str(s2)), (int)v, (int)v3);
			case "os_loop":
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				return E("os_loop", Str(s), Args((int)v2, Str(s)), (int)v, NoCcIndex);
			case "os_mem":
				v = r.U();
				VerifyBitness(v);
				return E("os_mem", Str(s), 0, 0, (int)v);
			case "os_mem_reg16":
				v = r.U();
				VerifyBitness(v);
				return E("os_mem_reg16", Str(s), 0, 0, (int)v);
			case "os_mem2":
				v = r.U();
				v2 = r.U();
				return E("os_mem2", Str(s), 0, (int)v2, (int)v);
			case "pblendvb":
				v = r.U();
				return E("pblendvb", Str(s), 0, (int)v);
			case "pclmulqdq":
				v = r.U();
				return E("pclmulqdq", Str(s), 0, 0, (int)v);
			case "pops":
				v = r.U();
				return E("pops", Str(s), 0, 0, (int)v);
			case "Reg16":
				return E("Reg16", Str(s));
			case "Reg32":
				return E("Reg32", Str(s));
			case "reverse":
				return E("reverse", Str(s));
			case "sae":
				v = r.U();
				return E("sae", Str(s), 0, 0, (int)v);
			case "push_imm8":
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				return E("push_imm8", Str(s), 0, (int)v2, (int)v);
			case "push_imm":
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				return E("push_imm", Str(s), 0, (int)v2, (int)v);
			case "SignExt_3":
				v = r.U();
				v2 = r.U();
				return E("SignExt", Str(s), (int)v, (int)v2, (int)v);
			case "SignExt_4":
				v = r.U();
				v2 = r.U();
				v3 = r.U();
				return E("SignExt", Str(s), (int)v, (int)v3, (int)v2);
			case "imul":
				v = r.U();
				return E("imul", Str(s), 0, 0, (int)v);
			case "STIG1":
				b = r.B();
				return E("STIG1", Str(s), 0, 0, ToInt(b));
			case "STIG2_2a":
				b = r.B();
				return E("STIG2", Str(s), 0, 0, ToInt(b));
			case "STIG2_2b":
				v = r.U();
				if (v != registerToFlag)
					throw new InvalidOperationException();
				return E("STIG2", Str(s), 1, 0, 0);
			case "XLAT":
				return E("XLAT", Str(s));
			default:
				throw new InvalidOperationException($"Unknown ctor kind: {ctorKind}");
			}
		}
	}
}
