// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Masm formatter instruction infos. The layout of each kind must match the C++ classes in <c>src/formatter/masm/info.cpp</c>
	/// </summary>
	sealed class CppMasmInstrInfoTableGen : CppInstrInfoTableGen {
		static readonly string[] Kinds = new[] {
			"Simple", "cc", "memsize", "AamAad", "Int3", "YD", "DX", "YX", "XY", "YA", "AX", "AY", "XLAT", "nop", "STIG1", "STi_ST", "ST_STi",
			"monitor", "mwait", "mwaitx", "maskmovq", "pblendvb", "reverse", "OpSize", "OpSize_cc", "OpSize2", "fword", "jcc", "bnd", "pops",
			"pclmulqdq", "imul", "Reg16", "Reg32", "reg", "invlpga", "DeclareData",
		};

		public CppMasmInstrInfoTableGen(GenTypes genTypes)
			: base(genTypes, "masm", genTypes.GetObject<Masm.CtorInfos>(TypeIds.MasmCtorInfos).Infos, Kinds) {
		}

		// Masm always appends the char
		static string Append(string s, char c) => s + c;

		// Kind                         mnemonic       arg1                                arg2        arg3
		// Simple                       mnemonic                                           flags
		// cc                           mnemonic       ARGS: mnemonics                     flags       cc_index
		// memsize                      mnemonic                                                       bitness
		// AamAad, Int3                 mnemonic
		// YD, DX, YX, XY, YA, AX, AY   mnemonic_args  mnemonic_no_args
		// XLAT                         mnemonic_args  mnemonic_no_args
		// nop                          mnemonic                                           register    bitness
		// STIG1, STi_ST                mnemonic                                                       pseudo_op
		// ST_STi                       mnemonic
		// monitor                      mnemonic       register1                           register2   register3
		// mwait, mwaitx                mnemonic
		// maskmovq                     mnemonic                                           flags
		// pblendvb, reverse            mnemonic
		// OpSize                       mnemonic       ARGS: mnemonic16/32/64                          code_size
		// OpSize_cc                    mnemonic       ARGS: mnemonics, other mnemonics    code_size   cc_index
		// OpSize2                      mnemonic       ARGS: mnemonic16/32/64                          can_use_bnd
		// fword                        mnemonic       mnemonic2                           flags       code_size
		// jcc                          mnemonic       ARGS: mnemonics                                 cc_index
		// bnd                          mnemonic                                           flags
		// pops                         mnemonic                                           flags       pseudo_ops_kind
		// pclmulqdq                    mnemonic                                                       pseudo_ops_kind
		// imul                         mnemonic
		// Reg16, Reg32                 mnemonic                                           flags
		// reg                          mnemonic                                           register
		// invlpga                      mnemonic                                                       bitness
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
			case "AX":
			case "AY":
			case "DX":
			case "XY":
			case "YA":
			case "YD":
			case "YX":
				c = r.C();
				return E(ctorKind, Str(s), Str(Append(s, c)));
			case "bnd":
				v = r.U();
				return E("bnd", Str(s), 0, (int)v);
			case "DeclareData":
				return E("DeclareData", Str(s));
			case "fword":
				c = r.C();
				v = r.U();
				v2 = r.U();
				return E("fword", Str(s), Str(Append(s, c)), (int)v2, (int)v);
			case "Int3":
				return E("Int3", Str(s));
			case "imul":
				return E("imul", Str(s));
			case "invlpga":
				v = r.U();
				VerifyBitness(v);
				return E("invlpga", Str(s), 0, 0, (int)v);
			case "CCa_1":
				v = r.U();
				VerifyCcCount(v, 1);
				return E("cc", Str(s), Args(Str(s)), 0, (int)v);
			case "CCa_2":
				s2 = r.S();
				v = r.U();
				VerifyCcCount(v, 2);
				return E("cc", Str(s), Args(Str(s), Str(s2)), 0, (int)v);
			case "CCa_3":
				s2 = r.S();
				s3 = r.S();
				v = r.U();
				VerifyCcCount(v, 3);
				return E("cc", Str(s), Args(Str(s), Str(s2), Str(s3)), 0, (int)v);
			case "CCb_1":
				v2 = r.U();
				v = r.U();
				VerifyCcCount(v2, 1);
				return E("cc", Str(s), Args(Str(s)), (int)v, (int)v2);
			case "CCb_2":
				s2 = r.S();
				v2 = r.U();
				v = r.U();
				VerifyCcCount(v2, 2);
				return E("cc", Str(s), Args(Str(s), Str(s2)), (int)v, (int)v2);
			case "CCb_3":
				s2 = r.S();
				s3 = r.S();
				v2 = r.U();
				v = r.U();
				VerifyCcCount(v2, 3);
				return E("cc", Str(s), Args(Str(s), Str(s2), Str(s3)), (int)v, (int)v2);
			case "jcc_1":
				v = r.U();
				VerifyCcCount(v, 1);
				return E("jcc", Str(s), Args(Str(s)), 0, (int)v);
			case "jcc_2":
				s2 = r.S();
				v = r.U();
				VerifyCcCount(v, 2);
				return E("jcc", Str(s), Args(Str(s), Str(s2)), 0, (int)v);
			case "jcc_3":
				s2 = r.S();
				s3 = r.S();
				v = r.U();
				VerifyCcCount(v, 3);
				return E("jcc", Str(s), Args(Str(s), Str(s2), Str(s3)), 0, (int)v);
			case "Loopcc1":
				s2 = r.S();
				v = r.U();
				VerifyCcCount(v, 2);
				return E("cc", Str(s), Args(Str(s), Str(s2)), 0, (int)v);
			case "Loopcc2":
				s2 = r.S();
				c = r.C();
				v2 = r.U();
				v3 = r.U();
				VerifyCcCount(v2, 2);
				return E("OpSize_cc", Str(s), Args(Str(s), Str(s2), Str(Append(s, c)), Str(Append(s2, c))), (int)v3, (int)v2);
			case "maskmovq":
				v = r.U();
				return E("maskmovq", Str(s), 0, (int)v);
			case "memsize":
				v = r.U();
				return E("memsize", Str(s), 0, 0, (int)v);
			case "monitor":
				v = r.U();
				v2 = r.U();
				v3 = r.U();
				return E("monitor", Str(s), (int)v, (int)v2, (int)v3);
			case "mwait":
				return E("mwait", Str(s));
			case "mwaitx":
				return E("mwaitx", Str(s));
			case "nop":
				v = r.U();
				v2 = r.U();
				return E("nop", Str(s), 0, (int)v2, (int)v);
			case "OpSize_1":
				v = r.U();
				return E("OpSize", Str(s), Args(Str(Append(s, 'w')), Str(Append(s, 'd')), Str(Append(s, 'q'))), 0, (int)v);
			case "OpSize_2":
				c = r.C();
				v = r.U();
				s2 = Append(s, c);
				return E("OpSize", Str(s), Args(Str(s2), Str(s2), Str(s2)), 0, (int)v);
			case "OpSize2":
				s2 = r.S();
				s3 = r.S();
				s4 = r.S();
				b = r.B();
				return E("OpSize2", Str(s), Args(Str(s2), Str(s3), Str(s4)), 0, ToInt(b));
			case "pblendvb":
				return E("pblendvb", Str(s));
			case "pclmulqdq":
				v = r.U();
				return E("pclmulqdq", Str(s), 0, 0, (int)v);
			case "pops_2":
				v = r.U();
				return E("pops", Str(s), 0, 0, (int)v);
			case "pops_3":
				v = r.U();
				v2 = r.U();
				return E("pops", Str(s), 0, (int)v2, (int)v);
			case "reg":
				v = r.U();
				return E("reg", Str(s), 0, (int)v);
			case "Reg16":
				v = r.U();
				return E("Reg16", Str(s), 0, (int)v);
			case "Reg32":
				v = r.U();
				return E("Reg32", Str(s), 0, (int)v);
			case "reverse":
				return E("reverse", Str(s));
			case "ST_STi":
				return E("ST_STi", Str(s));
			case "STi_ST":
				b = r.B();
				return E("STi_ST", Str(s), 0, 0, ToInt(b));
			case "STIG1":
				b = r.B();
				return E("STIG1", Str(s), 0, 0, ToInt(b));
			case "XLAT":
				return E("XLAT", Str(s), Str(Append(s, 'b')));
			default:
				throw new InvalidOperationException($"Unknown ctor kind: {ctorKind}");
			}
		}
	}
}
