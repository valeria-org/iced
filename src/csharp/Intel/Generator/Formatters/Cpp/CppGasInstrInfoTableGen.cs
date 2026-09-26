// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Gas formatter instruction infos. The layout of each kind must match the C++ classes in <c>src/formatter/gas/fmt_tbl.cpp</c>
	/// </summary>
	sealed class CppGasInstrInfoTableGen : CppInstrInfoTableGen {
		static readonly string[] Kinds = new[] {
			"Simple", "cc", "AamAad", "nop", "STIG1", "STi_ST", "ST_STi", "as", "maskmovq", "pblendvb", "OpSize", "OpSize2_bnd", "OpSize3",
			"os2", "os", "os_mem2", "Reg16", "mem16", "os_loop", "os_jcc", "movabs", "er", "sae", "far", "bnd", "pops", "pclmulqdq",
			"imul", "Reg32", "DeclareData",
		};

		// C++ NO_CC_INDEX_U8
		const int NoCcIndex = 0xFF;

		public CppGasInstrInfoTableGen(GenTypes genTypes)
			: base(genTypes, "gas", genTypes.GetObject<Gas.CtorInfos>(TypeIds.GasCtorInfos).Infos, Kinds) {
		}

		// Kind                 mnemonic   arg1                              arg2          arg3
		// Simple               mnemonic   mnemonic_suffix                   flags
		// cc                   mnemonic   ARGS: mnemonics, suffix mnemonics                cc_index
		// AamAad               mnemonic
		// nop                  mnemonic                                     register      bitness
		// STIG1, STi_ST        mnemonic                                                   pseudo_op
		// ST_STi               mnemonic
		// as                   mnemonic                                                   bitness
		// maskmovq, pblendvb   mnemonic
		// OpSize               mnemonic   ARGS: mnemonic16/32/64                          code_size
		// OpSize2_bnd          mnemonic   ARGS: mnemonic16/32/64
		// OpSize3              mnemonic   mnemonic_suffix                                 bitness
		// os2                  mnemonic   ARGS: mnemonic_suffix, can_use_bnd, flags       bitness
		// os                   mnemonic   can_use_bnd                       flags         bitness
		// os_mem2              mnemonic   mnemonic_suffix                                 bitness
		// Reg16, imul          mnemonic   mnemonic_suffix
		// mem16                mnemonic   mnemonic_reg_suffix               mnemonic_mem_suffix
		// os_loop              mnemonic   ARGS: reg_size, mnemonics, suffix mnemonics     bitness       cc_index (0xFF = none)
		// os_jcc               mnemonic   ARGS: mnemonics                   bitness       cc_index
		// movabs               mnemonic   ARGS: mnemonic_suffix, mnemonic64, mnemonic_suffix64
		// er                   mnemonic   mnemonic_suffix                   flags         er_index
		// sae                  mnemonic                                                   sae_index
		// far                  mnemonic   mnemonic_suffix                                 bitness
		// bnd                  mnemonic   mnemonic_suffix                   flags
		// pops                 mnemonic   can_use_sae                                     pseudo_ops_kind
		// pclmulqdq            mnemonic                                                   pseudo_ops_kind
		// Reg32, DeclareData   mnemonic
		protected override Entry Create(FmtInstructionDef def, string ctorKind, ArgReader r) {
			var s = def.Mnemonic;
			char c;
			uint v, v2, v3;
			bool b;
			string s2, s3, s4;
			switch (ctorKind) {
			case "Normal_1":
				return E("Simple", Str(s), Str(s));
			case "Normal_2a":
				c = r.C();
				return E("Simple", Str(s), Str(AddSuffix(s, c)));
			case "Normal_2b":
				v = r.U();
				return E("Simple", Str(s), Str(s), (int)v);
			case "Normal_2c":
				c = r.C();
				s = AddSuffix(s, c);
				return E("Simple", Str(s), Str(s));
			case "Normal_3":
				c = r.C();
				v = r.U();
				return E("Simple", Str(s), Str(AddSuffix(s, c)), (int)v);
			case "AamAad":
				return E("AamAad", Str(s));
			case "asz":
				v = r.U();
				VerifyBitness(v);
				return E("as", Str(s), arg3: (int)v);
			case "bnd":
				c = r.C();
				v = r.U();
				return E("bnd", Str(s), Str(AddSuffix(s, c)), (int)v);
			case "DeclareData":
				return E("DeclareData", Str(s));
			case "er_2":
				v = r.U();
				return E("er", Str(s), Str(s), 0, (int)v);
			case "er_4":
				c = r.C();
				v = r.U();
				v2 = r.U();
				return E("er", Str(s), Str(AddSuffix(s, c)), (int)v2, (int)v);
			case "far":
				c = r.C();
				v = r.U();
				VerifyBitness(v);
				return E("far", Str(s), Str(AddSuffix(s, c)), 0, (int)v);
			case "imul":
				c = r.C();
				return E("imul", Str(s), Str(AddSuffix(s, c)));
			case "maskmovq":
				return E("maskmovq", Str(s));
			case "movabs":
				c = r.C();
				s3 = r.S();
				return E("movabs", Str(s), Args(Str(AddSuffix(s, c)), Str(s3), Str(AddSuffix(s3, c))));
			case "nop":
				v = r.U();
				v2 = r.U();
				return E("nop", Str(s), 0, (int)v2, (int)v);
			case "OpSize":
				v = r.U();
				return E("OpSize", Str(s), Args(Str(AddSuffix(s, 'w')), Str(AddSuffix(s, 'l')), Str(AddSuffix(s, 'q'))), 0, (int)v);
			case "OpSize2_bnd":
				s2 = r.S();
				s3 = r.S();
				s4 = r.S();
				return E("OpSize2_bnd", Str(s), Args(Str(s2), Str(s3), Str(s4)));
			case "OpSize3":
				c = r.C();
				v = r.U();
				return E("OpSize3", Str(s), Str(AddSuffix(s, c)), 0, (int)v);
			case "os":
				v = r.U();
				b = r.B();
				v3 = r.U();
				VerifyBitness(v);
				return E("os", Str(s), ToInt(b), (int)v3, (int)v);
			case "CC_1":
				c = r.C();
				v = r.U();
				VerifyCcCount(v, 1);
				return E("cc", Str(s), Args(Str(s), Str(AddSuffix(s, c))), 0, (int)v);
			case "CC_2":
				s2 = r.S();
				c = r.C();
				v = r.U();
				VerifyCcCount(v, 2);
				return E("cc", Str(s), Args(Str(s), Str(s2), Str(AddSuffix(s, c)), Str(AddSuffix(s2, c))), 0, (int)v);
			case "CC_3":
				s2 = r.S();
				s3 = r.S();
				c = r.C();
				v = r.U();
				VerifyCcCount(v, 3);
				return E("cc", Str(s), Args(Str(s), Str(s2), Str(s3), Str(AddSuffix(s, c)), Str(AddSuffix(s2, c)), Str(AddSuffix(s3, c))), 0, (int)v);
			case "os_jcc_1":
				v2 = r.U();
				v = r.U();
				VerifyBitness(v);
				VerifyCcCount(v2, 1);
				return E("os_jcc", Str(s), Args(Str(s)), (int)v, (int)v2);
			case "os_jcc_2":
				s2 = r.S();
				v2 = r.U();
				v = r.U();
				VerifyBitness(v);
				VerifyCcCount(v2, 2);
				return E("os_jcc", Str(s), Args(Str(s), Str(s2)), (int)v, (int)v2);
			case "os_jcc_3":
				s2 = r.S();
				s3 = r.S();
				v2 = r.U();
				v = r.U();
				VerifyBitness(v);
				VerifyCcCount(v2, 3);
				return E("os_jcc", Str(s), Args(Str(s), Str(s2), Str(s3)), (int)v, (int)v2);
			case "os_loopcc":
				s2 = r.S();
				c = r.C();
				v3 = r.U();
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				VerifyCcCount(v3, 2);
				return E("os_loop", Str(s), Args((int)v2, Str(s), Str(s2), Str(AddSuffix(s, c)), Str(AddSuffix(s2, c))), (int)v, (int)v3);
			case "os_loop":
				c = r.C();
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				return E("os_loop", Str(s), Args((int)v2, Str(s), Str(AddSuffix(s, c))), (int)v, NoCcIndex);
			case "Reg16":
				return E("Reg16", Str(s), Str(AddSuffix(s, 'w')));
			case "os_mem2":
				c = r.C();
				v = r.U();
				return E("os_mem2", Str(s), Str(AddSuffix(s, c)), 0, (int)v);
			case "os2_3":
				c = r.C();
				v = r.U();
				b = r.B();
				VerifyBitness(v);
				return E("os2", Str(s), Args(Str(AddSuffix(s, c)), ToInt(b), 0), 0, (int)v);
			case "os2_4":
				c = r.C();
				v = r.U();
				b = r.B();
				v3 = r.U();
				VerifyBitness(v);
				return E("os2", Str(s), Args(Str(AddSuffix(s, c)), ToInt(b), (int)v3), 0, (int)v);
			case "pblendvb":
				return E("pblendvb", Str(s));
			case "pclmulqdq":
				v = r.U();
				return E("pclmulqdq", Str(s), 0, 0, (int)v);
			case "pops":
				v = r.U();
				b = r.B();
				return E("pops", Str(s), ToInt(b), 0, (int)v);
			case "mem16":
				c = r.C();
				return E("mem16", Str(s), Str(AddSuffix(s, c)), Str(AddSuffix(s, 'w')));
			case "Reg32":
				return E("Reg32", Str(s));
			case "sae":
				v = r.U();
				return E("sae", Str(s), 0, 0, (int)v);
			case "ST_STi":
				return E("ST_STi", Str(s));
			case "STi_ST":
				b = r.B();
				return E("STi_ST", Str(s), 0, 0, ToInt(b));
			case "STIG1":
				b = r.B();
				return E("STIG1", Str(s), 0, 0, ToInt(b));
			default:
				throw new InvalidOperationException($"Unknown ctor kind: {ctorKind}");
			}
		}
	}
}
