// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Intel formatter instruction infos. The layout of each kind must match the C++ classes in <c>src/formatter/intel/fmt_tbl.cpp</c>
	/// </summary>
	sealed class CppIntelInstrInfoTableGen : CppInstrInfoTableGen {
		static readonly string[] Kinds = new[] {
			"Simple", "cc", "memsize", "StringIg1", "StringIg0", "nop", "ST1", "ST2", "maskmovq", "os", "os_bnd", "as", "os_jcc", "os_loop",
			"movabs", "opmask_op", "bnd", "ST_STi", "STi_ST", "pops", "pclmulqdq", "imul", "Reg16", "Reg32", "reg", "invlpga", "DeclareData",
			"bcst",
		};

		// C++ NO_CC_INDEX_U8
		const int NoCcIndex = 0xFF;

		public CppIntelInstrInfoTableGen(GenTypes genTypes)
			: base(genTypes, "intel", genTypes.GetObject<Intel.CtorInfos>(TypeIds.IntelCtorInfos).Infos, Kinds) {
		}

		// Kind                 mnemonic   arg1                         arg2                arg3
		// Simple               mnemonic                                flags
		// cc                   mnemonic   ARGS: mnemonics                                  cc_index
		// memsize              mnemonic                                                    bitness
		// StringIg0/1          mnemonic
		// nop                  mnemonic                                register            bitness
		// ST1                  mnemonic                                flags               is_load
		// ST2                  mnemonic                                flags
		// maskmovq, movabs     mnemonic
		// os                   mnemonic                                flags               bitness
		// os_bnd, as           mnemonic                                                    bitness
		// os_jcc               mnemonic   ARGS: flags, mnemonics       bitness             cc_index
		// os_loop              mnemonic   ARGS: register, mnemonics    bitness             cc_index (0xFF = none)
		// opmask_op            mnemonic
		// bnd                  mnemonic                                flags
		// ST_STi, STi_ST       mnemonic                                                    pseudo_op
		// pops, pclmulqdq      mnemonic                                                    pseudo_ops_kind
		// imul, Reg16, Reg32   mnemonic
		// reg                  mnemonic                                register
		// invlpga              mnemonic                                                    bitness
		// DeclareData          mnemonic
		// bcst                 mnemonic                                flags_no_broadcast
		protected override Entry Create(FmtInstructionDef def, string ctorKind, ArgReader r) {
			var s = def.Mnemonic;
			uint v, v2, v3;
			bool b;
			string s2, s3;
			switch (ctorKind) {
			case "Normal_1":
				return E("Simple", Str(s));
			case "Normal_2":
				v = r.U();
				return E("Simple", Str(s), 0, (int)v);
			case "asz":
				v = r.U();
				VerifyBitness(v);
				return E("as", Str(s), 0, 0, (int)v);
			case "StringIg0":
				return E("StringIg0", Str(s));
			case "StringIg1":
				return E("StringIg1", Str(s));
			case "bcst":
				v = r.U();
				return E("bcst", Str(s), 0, (int)v);
			case "bnd":
				v = r.U();
				return E("bnd", Str(s), 0, (int)v);
			case "DeclareData":
				return E("DeclareData", Str(s));
			case "imul":
				return E("imul", Str(s));
			case "opmask_op":
				return E("opmask_op", Str(s));
			case "ST_STi":
				b = r.B();
				return E("ST_STi", Str(s), 0, 0, ToInt(b));
			case "STi_ST":
				b = r.B();
				return E("STi_ST", Str(s), 0, 0, ToInt(b));
			case "maskmovq":
				return E("maskmovq", Str(s));
			case "memsize":
				v = r.U();
				return E("memsize", Str(s), 0, 0, (int)v);
			case "movabs":
				return E("movabs", Str(s));
			case "nop":
				v = r.U();
				v2 = r.U();
				return E("nop", Str(s), 0, (int)v2, (int)v);
			case "os2":
				v = r.U();
				VerifyBitness(v);
				return E("os", Str(s), 0, 0, (int)v);
			case "os3":
				v = r.U();
				v2 = r.U();
				VerifyBitness(v);
				return E("os", Str(s), 0, (int)v2, (int)v);
			case "os_bnd":
				v = r.U();
				VerifyBitness(v);
				return E("os_bnd", Str(s), 0, 0, (int)v);
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
			case "pclmulqdq":
				v = r.U();
				return E("pclmulqdq", Str(s), 0, 0, (int)v);
			case "pops":
				v = r.U();
				return E("pops", Str(s), 0, 0, (int)v);
			case "reg":
				v = r.U();
				return E("reg", Str(s), 0, (int)v);
			case "Reg16":
				return E("Reg16", Str(s));
			case "Reg32":
				return E("Reg32", Str(s));
			case "ST1_2":
				v = r.U();
				return E("ST1", Str(s), 0, (int)v, 0);
			case "ST1_3":
				v = r.U();
				b = r.B();
				return E("ST1", Str(s), 0, (int)v, ToInt(b));
			case "ST2":
				v = r.U();
				return E("ST2", Str(s), 0, (int)v);
			case "invlpga":
				v = r.U();
				VerifyBitness(v);
				return E("invlpga", Str(s), 0, 0, (int)v);
			default:
				throw new InvalidOperationException($"Unknown ctor kind: {ctorKind}");
			}
		}
	}
}
