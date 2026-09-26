// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Generator.Enums;
using Generator.Enums.Cpp;
using Generator.Enums.Decoder;
using Generator.Enums.Encoder;
using Generator.IO;
using Generator.Tables;

namespace Generator.Encoder.Cpp {
	/// <summary>
	/// Generates the C++ encoder and op code info tables. Everything is generated as constant data so no heap memory is used
	/// and the tables are stored in read-only memory. Files:
	/// <list type="bullet">
	/// <item><c>src/encoder/op_code_handlers_table.cpp</c>: operand handlers (<c>Op</c> instances) and the op code handlers (one per <c>Code</c>)</item>
	/// <item><c>src/encoder/op_code_info_table.cpp</c>: all <c>OpCodeInfo</c>s and their op code / instruction strings</item>
	/// <item><c>src/internal/encoder/encoder_data.hpp</c>: <c>ENC_FLAGS1..3</c> (only used by the tests to verify the handlers table)</item>
	/// <item><c>src/internal/encoder/imm_sizes.hpp</c>, <c>to_decoder_options.hpp</c></item>
	/// </list>
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppEncoderGenerator : EncoderGenerator {
		readonly GeneratorContext generatorContext;
		readonly IdentifierConverter idConverter;
		readonly CppEnumsGenerator enumGenerator;

		public CppEncoderGenerator(GeneratorContext generatorContext)
			: base(generatorContext.Types) {
			this.generatorContext = generatorContext;
			idConverter = CppIdentifierConverter.Create();
			enumGenerator = new CppEnumsGenerator(generatorContext);
		}

		protected override void Generate(EnumType enumType) => enumGenerator.Generate(enumType);

		string InternalHeader(params string[] names) => CppConstants.GetInternalFilename(genTypes, names);
		string SrcFile(params string[] names) => CppConstants.GetSrcFilename(genTypes, names);

		static void WriteHeaderStart(FileWriter writer, params string[] includes) {
			CppConstants.WriteHeaderFileHeader(writer);
			foreach (var include in includes)
				writer.WriteLine($"#include {include}");
			if (includes.Length > 0)
				writer.WriteLine();
		}

		sealed class OpInfo {
			public readonly OpHandlerKind OpHandlerKind;
			public readonly object[] Args;
			public readonly string Name;
			public OpInfo(OpHandlerKind opHandlerKind, object[] args, string name) {
				OpHandlerKind = opHandlerKind;
				Args = args;
				Name = name;
			}
		}

		sealed class OpKeyComparer : IEqualityComparer<(OpHandlerKind opHandlerKind, object[] args)> {
			public bool Equals((OpHandlerKind opHandlerKind, object[] args) x, (OpHandlerKind opHandlerKind, object[] args) y) {
				if (x.opHandlerKind != y.opHandlerKind)
					return false;
				var xa = x.args;
				var ya = y.args;
				if (xa.Length != ya.Length)
					return false;
				for (int i = 0; i < xa.Length; i++) {
					if (!Equals(xa[i], ya[i]))
						return false;
				}
				return true;
			}

			public int GetHashCode((OpHandlerKind opHandlerKind, object[] args) obj) {
				var args = obj.args;
				int hc = HashCode.Combine((int)obj.opHandlerKind, args.Length);
				for (int i = 0; i < args.Length; i++)
					hc = HashCode.Combine(args[i].GetHashCode());
				return hc;
			}
		}

		OpCodeHandlers? opCodeHandlers;

		// The ops are written to the same file as the op code handlers, see GenerateOpCodeInfo()
		protected override void Generate(OpCodeHandlers handlers) => opCodeHandlers = handlers;

		void WriteOpTables(FileWriter writer, OpCodeHandlers handlers) {
			var sb = new StringBuilder();
			var dict = new Dictionary<(OpHandlerKind opHandlerKind, object[] args), OpInfo>(new OpKeyComparer());
			Add(sb, dict, handlers.Legacy.Select(a => (a.opHandlerKind, a.args)));
			Add(sb, dict, handlers.Vex.Select(a => (a.opHandlerKind, a.args)));
			Add(sb, dict, handlers.Xop.Select(a => (a.opHandlerKind, a.args)));
			Add(sb, dict, handlers.Evex.Select(a => (a.opHandlerKind, a.args)));
			Add(sb, dict, handlers.Mvex.Select(a => (a.opHandlerKind, a.args)));

			var usedNames = new HashSet<string>(dict.Count, StringComparer.Ordinal);
			foreach (var kv in dict) {
				if (!usedNames.Add(kv.Value.Name))
					throw new InvalidOperationException();
			}

			writer.WriteLine("namespace {");
			{
				foreach (var kv in dict.OrderBy(a => a.Value.Name, StringComparer.Ordinal)) {
					var info = kv.Value;
					var structName = idConverter.Type(GetStructName(info.OpHandlerKind));
					writer.Write($"constexpr {structName} {info.Name}");
					switch (info.OpHandlerKind) {
					case OpHandlerKind.OpA:
					case OpHandlerKind.OpImm:
					case OpHandlerKind.OpJdisp:
					case OpHandlerKind.OpJx:
						if (info.Args.Length != 1)
							throw new InvalidOperationException();
						writer.WriteLine($"{{{(int)info.Args[0]}}};");
						break;

					case OpHandlerKind.OpHx:
					case OpHandlerKind.OpIsX:
					case OpHandlerKind.OpModRM_reg:
					case OpHandlerKind.OpModRM_reg_mem:
					case OpHandlerKind.OpModRM_regF0:
					case OpHandlerKind.OpModRM_rm:
					case OpHandlerKind.OpModRM_rm_reg_only:
					case OpHandlerKind.OpRegEmbed8:
					case OpHandlerKind.OpVsib:
						if (info.Args.Length != 2)
							throw new InvalidOperationException();
						writer.WriteLine($"{{{idConverter.ToDeclTypeAndValue((EnumValue)info.Args[0])}, {idConverter.ToDeclTypeAndValue((EnumValue)info.Args[1])}}};");
						break;

					case OpHandlerKind.OpIb:
					case OpHandlerKind.OpId:
					case OpHandlerKind.OpReg:
						if (info.Args.Length != 1)
							throw new InvalidOperationException();
						writer.WriteLine($"{{{idConverter.ToDeclTypeAndValue((EnumValue)info.Args[0])}}};");
						break;

					case OpHandlerKind.OpJ:
						if (info.Args.Length != 2)
							throw new InvalidOperationException();
						writer.WriteLine($"{{{idConverter.ToDeclTypeAndValue((EnumValue)info.Args[0])}, {(int)info.Args[1]}}};");
						break;

					case OpHandlerKind.None:
					case OpHandlerKind.OpI4:
					case OpHandlerKind.OpIq:
					case OpHandlerKind.OpIw:
					case OpHandlerKind.OpMRBX:
					case OpHandlerKind.OpO:
					case OpHandlerKind.OprDI:
					case OpHandlerKind.OpRegSTi:
					case OpHandlerKind.OpX:
					case OpHandlerKind.OpY:
						if (info.Args.Length != 0)
							throw new InvalidOperationException();
						writer.WriteLine("{};");
						break;

					case OpHandlerKind.OpModRM_rm_mem_only:
						if (info.Args.Length != 1)
							throw new InvalidOperationException();
						writer.WriteLine($"{{{((bool)info.Args[0] ? "true" : "false")}}};");
						break;

					default:
						throw new InvalidOperationException();
					}
				}

			}
			writer.WriteLine("} // namespace");

			var tables = new (string name, (EnumValue opCodeOperandKind, OpHandlerKind opHandlerKind, object[] args)[] ops)[] {
				("Legacy", handlers.Legacy),
				("VEX", handlers.Vex),
				("XOP", handlers.Xop),
				("EVEX", handlers.Evex),
				("MVEX", handlers.Mvex),
			};
			int totalOps = tables.Sum(a => a.ops.Length);
			if (totalOps > 0x100)
				throw new InvalidOperationException("EncOpCodeHandler::operands are u8 indexes");
			writer.WriteLine();
			writer.WriteLine($"const Op* const OPS_TABLE[{totalOps}] = {{");
			using (writer.Indent()) {
				int offset = 0;
				foreach (var (name, ops) in tables) {
					writer.WriteLine($"// {name} (offset {offset})");
					foreach (var value in ops) {
						var info = dict[(value.opHandlerKind, value.args)];
						writer.WriteLine($"&{info.Name},// {value.opCodeOperandKind.Name(idConverter)}");
					}
					offset += ops.Length;
				}
			}
			writer.WriteLine("};");

			void Add(StringBuilder sb, Dictionary<(OpHandlerKind opHandlerKind, object[] args), OpInfo> dict, IEnumerable<(OpHandlerKind opHandlerKind, object[] args)> values) {
				foreach (var value in values) {
					if (!dict.ContainsKey(value))
						dict.Add(value, new OpInfo(value.opHandlerKind, value.args, GetName(sb, value.opHandlerKind, value.args)));
				}
			}

			string GetName(StringBuilder sb, OpHandlerKind opHandlerKind, object[] args) {
				sb.Clear();
				sb.Append(opHandlerKind.ToString());
				foreach (var obj in args) {
					sb.Append('_');
					switch (obj) {
					case EnumValue value:
						sb.Append(value.RawName);
						break;
					case int value:
						sb.Append(value);
						break;
					case bool value:
						sb.Append(value ? "true" : "false");
						break;
					default:
						throw new InvalidOperationException();
					}
				}
				return idConverter.Static(sb.ToString());
			}

			static string GetStructName(OpHandlerKind kind) {
				if (kind == OpHandlerKind.None)
					return "InvalidOpHandler";
				return kind.ToString();
			}
		}

		protected override void GenerateOpCodeInfo(InstructionDef[] defs, (MvexTupleTypeLutKind ttLutKind, EnumValue[] tupleTypes)[] mvexTupleTypeData,
			(MvexTupleTypeLutKind ttLutKind, EnumValue[] tupleTypes)[] mvexMemorySizeData) {
			// The MVEX tables (mvex_data, mvex_tt_lut, mvex_memsz_lut) are part of the core (mvex info) and aren't generated here
			var handlers = opCodeHandlers ?? throw new InvalidOperationException();
			var allData = GetData(defs).Select(a => new EncData(a.def, a.encFlags1, a.encFlags2, a.encFlags3, a.opcFlags1, a.opcFlags2)).ToArray();
			for (int i = 0; i < allData.Length; i++) {
				if (allData[i].Def.Code.Value != (uint)i)
					throw new InvalidOperationException();
			}
			GenerateEncoderData(handlers, allData);
			GenerateOpCodeHandlersTable(handlers, allData);
			GenerateOpCodeInfoTable(handlers, allData);
		}

		sealed class EncData {
			public readonly InstructionDef Def;
			public readonly uint EncFlags1;
			public readonly uint EncFlags2;
			public readonly uint EncFlags3;
			public readonly uint OpcFlags1;
			public readonly uint OpcFlags2;
			public EncData(InstructionDef def, uint encFlags1, uint encFlags2, uint encFlags3, uint opcFlags1, uint opcFlags2) {
				Def = def;
				EncFlags1 = encFlags1;
				EncFlags2 = encFlags2;
				EncFlags3 = encFlags3;
				OpcFlags1 = opcFlags1;
				OpcFlags2 = opcFlags2;
			}
		}

		void GenerateEncoderData(OpCodeHandlers handlers, EncData[] allData) {
			var tables = new (string name, Func<EncData, uint> getValue)[] {
				("ENC_FLAGS1", a => a.EncFlags1),
				("ENC_FLAGS2", a => a.EncFlags2),
				("ENC_FLAGS3", a => a.EncFlags3),
			};
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(InternalHeader("encoder", "encoder_data.hpp")))) {
				WriteHeaderStart(writer, "<cstdint>");
				writer.WriteLine("// Only used by the tests. The encoder uses the generated op code handlers table (src/encoder/op_code_handlers_table.cpp).");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine("// Indexes of the first Legacy/VEX/XOP/EVEX/MVEX op in OPS_TABLE");
				uint offset = 0;
				foreach (var (name, ops) in new[] { ("LEGACY", handlers.Legacy), ("VEX", handlers.Vex), ("XOP", handlers.Xop), ("EVEX", handlers.Evex), ("MVEX", handlers.Mvex) }) {
					writer.WriteLine($"inline constexpr std::uint32_t {name}_OPS_OFFSET = {offset};");
					offset += (uint)ops.Length;
				}
				foreach (var (name, getValue) in tables) {
					writer.WriteLine();
					writer.WriteLine($"inline constexpr std::uint32_t {name}[{allData.Length}] = {{");
					using (writer.Indent()) {
						foreach (var data in allData)
							writer.WriteLine($"0x{getValue(data):X8},// {data.Def.Code.Name(idConverter)}");
					}
					writer.WriteLine("};");
				}
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		uint EncFlags1Value(string name) => genTypes[TypeIds.EncFlags1][name].Value;
		uint EnumVal(TypeId typeId, string name) => genTypes[typeId][name].Value;
		string EnumStr(TypeId typeId, uint value) => idConverter.ToDeclTypeAndValue(genTypes[typeId].Values.Single(a => a.Value == value));

		static uint GetOps(uint encFlags1, uint[] shifts, uint mask, uint offset, List<uint> ops) {
			ops.Clear();
			foreach (var shift in shifts)
				ops.Add((encFlags1 >> (int)shift) & mask);
			// Same as Rust: the operands are all ops until the last non-zero op index
			int len = ops.Count;
			while (len > 0 && ops[len - 1] == 0)
				len--;
			for (int i = len; i < ops.Count; i++) {
				if (ops[i] != 0)
					throw new InvalidOperationException();
			}
			ops.RemoveRange(len, ops.Count - len);
			for (int i = 0; i < ops.Count; i++)
				ops[i] += offset;
			return (uint)len;
		}

		(uint[] shifts, uint mask) GetOpShifts(string prefix, int count) {
			var shifts = new uint[count];
			for (int i = 0; i < shifts.Length; i++)
				shifts[i] = EncFlags1Value($"{prefix}Op{i}Shift");
			return (shifts, EncFlags1Value($"{prefix}OpMask"));
		}

		static int FindOpIndex((EnumValue opCodeOperandKind, OpHandlerKind opHandlerKind, object[] args)[] table, OpHandlerKind kind, string reg1, string reg2) {
			for (int i = 0; i < table.Length; i++) {
				var info = table[i];
				if (info.opHandlerKind == kind && info.args.Length == 2 && info.args[0] is EnumValue r1 && info.args[1] is EnumValue r2 &&
					r1.RawName == reg1 && r2.RawName == reg2) {
					return i;
				}
			}
			throw new InvalidOperationException();
		}

		// Same as the Rust handler constructors (encoder/op_code_handler.rs)
		void GenerateOpCodeHandlersTable(OpCodeHandlers handlers, EncData[] allData) {
			uint legacyOffset = 0;
			uint vexOffset = legacyOffset + (uint)handlers.Legacy.Length;
			uint xopOffset = vexOffset + (uint)handlers.Vex.Length;
			uint evexOffset = xopOffset + (uint)handlers.Xop.Length;
			uint mvexOffset = evexOffset + (uint)handlers.Evex.Length;
			var legacyOps = GetOpShifts("Legacy_", 4);
			var vexOps = GetOpShifts("VEX_", 5);
			var xopOps = GetOpShifts("XOP_", 4);
			var evexOps = GetOpShifts("EVEX_", 4);
			var mvexOps = GetOpShifts("MVEX_", 4);
			// Rust's D3nowHandler uses OpModRM_reg(MM0, MM7) and OpModRM_rm(MM0, MM7), the same ops as some legacy op kinds
			var d3nowOps = new uint[] {
				legacyOffset + (uint)FindOpIndex(handlers.Legacy, OpHandlerKind.OpModRM_reg, "MM0", "MM7"),
				legacyOffset + (uint)FindOpIndex(handlers.Legacy, OpHandlerKind.OpModRM_rm, "MM0", "MM7"),
			};
			var codeType = genTypes[TypeIds.Code];
			uint codeInvalid = codeType["INVALID"].Value;
			uint codeDeclareByte = codeType["DeclareByte"].Value;
			uint codeDeclareWord = codeType["DeclareWord"].Value;
			uint codeDeclareDword = codeType["DeclareDword"].Value;
			uint codeDeclareQword = codeType["DeclareQword"].Value;
			uint codeZeroBytes = codeType["Zero_bytes"].Value;

			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(SrcFile("encoder", "op_code_handlers_table.cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"iced_x86/code_size.hpp\"");
				writer.WriteLine("#include \"iced_x86/register.hpp\"");
				writer.WriteLine("#include \"iced_x86/tuple_type.hpp\"");
				writer.WriteLine("#include \"internal/encoder/const_init.hpp\"");
				writer.WriteLine("#include \"internal/encoder/op_code_handler.hpp\"");
				writer.WriteLine("#include \"internal/encoder/ops.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				WriteOpTables(writer, handlers);
				writer.WriteLine();
				writer.WriteLine("namespace {");
				writer.WriteLine("using K = OpCodeHandlerKind;");
				writer.WriteLine("using H = EncOpCodeHandler;");
				writer.WriteLine("} // namespace");
				writer.WriteLine();
				writer.WriteLine("// enc_flags3, op_code, kind, operands_len, operands, group_index, rm_group_index, op_size, addr_size, is_2byte_opcode, is_special_instr, u");
				writer.WriteLine("// clang-format off");
				writer.WriteLine("ICED_CONSTINIT const EncOpCodeHandler OP_CODE_HANDLERS[IcedConstants::CODE_ENUM_COUNT] = {");
				using (writer.Indent()) {
					var ops = new List<uint>();
					foreach (var data in allData) {
						var encFlags1 = data.EncFlags1;
						var encFlags2 = data.EncFlags2;
						var encFlags3 = data.EncFlags3;
						var code = data.Def.Code.Value;

						string kind;
						uint handlerEncFlags3 = 0;
						uint opCode = 0;
						int groupIndex = -1;
						int rmGroupIndex = -1;
						uint opSize = 0;
						uint addrSize = 0;
						bool is2ByteOpCode = false;
						bool isSpecialInstr = false;
						string u = "{}";
						ops.Clear();

						uint encOpCode = (encFlags2 >> (int)EncFlags2.OpCodeShift) & 0xFFFF;
						int encGroupIndex = (encFlags2 & (uint)EncFlags2.HasGroupIndex) == 0 ? -1 : (int)((encFlags2 >> (int)EncFlags2.GroupIndexShift) & 7);
						int encRmGroupIndex = (encFlags3 & (uint)EncFlags3.HasRmGroupIndex) == 0 ? -1 : (int)((encFlags2 >> (int)EncFlags2.GroupIndexShift) & 7);
						bool encIs2ByteOpCode = (encFlags2 & (uint)EncFlags2.OpCodeIs2Bytes) != 0;
						uint tableIndex = (encFlags2 >> (int)EncFlags2.TableShift) & (uint)EncFlags2.TableMask;
						uint mpByte = (encFlags2 >> (int)EncFlags2.MandatoryPrefixShift) & (uint)EncFlags2.MandatoryPrefixMask;
						var wbit = (WBit)((encFlags2 >> (int)EncFlags2.WBitShift) & (uint)EncFlags2.WBitMask);
						var lbit = (LBit)((encFlags2 >> (int)EncFlags2.LBitShift) & (uint)EncFlags2.LBitMask);
						var encoding = (EncodingKind)((encFlags3 >> (int)EncFlags3.EncodingShift) & (uint)EncFlags3.EncodingMask);

						void InitVecBase() {
							handlerEncFlags3 = encFlags3;
							opCode = encOpCode;
							groupIndex = encGroupIndex;
							rmGroupIndex = encRmGroupIndex;
							is2ByteOpCode = encIs2ByteOpCode;
						}

						switch (encoding) {
						case EncodingKind.Legacy:
							if (code == codeInvalid)
								kind = "Invalid";
							else if (code <= codeDeclareQword) {
								kind = "DeclareData";
								isSpecialInstr = true;
								uint elemSize;
								if (code == codeDeclareByte)
									elemSize = 1;
								else if (code == codeDeclareWord)
									elemSize = 2;
								else if (code == codeDeclareDword)
									elemSize = 4;
								else if (code == codeDeclareQword)
									elemSize = 8;
								else
									throw new InvalidOperationException();
								u = $"H::DeclareDataData{{{elemSize}}}";
							}
							else if (code == codeZeroBytes) {
								kind = "ZeroBytes";
								isSpecialInstr = true;
							}
							else {
								kind = "Legacy";
								InitVecBase();
								opSize = (encFlags3 >> (int)EncFlags3.OperandSizeShift) & (uint)EncFlags3.OperandSizeMask;
								addrSize = (encFlags3 >> (int)EncFlags3.AddressSizeShift) & (uint)EncFlags3.AddressSizeMask;
								var (tableByte1, tableByte2) = (LegacyOpCodeTable)tableIndex switch {
									LegacyOpCodeTable.MAP0 => (0U, 0U),
									LegacyOpCodeTable.MAP0F => (0x0FU, 0U),
									LegacyOpCodeTable.MAP0F38 => (0x0FU, 0x38U),
									LegacyOpCodeTable.MAP0F3A => (0x0FU, 0x3AU),
									_ => throw new InvalidOperationException(),
								};
								uint mandatoryPrefix = (MandatoryPrefixByte)mpByte switch {
									MandatoryPrefixByte.None => 0U,
									MandatoryPrefixByte.P66 => 0x66U,
									MandatoryPrefixByte.PF3 => 0xF3U,
									MandatoryPrefixByte.PF2 => 0xF2U,
									_ => throw new InvalidOperationException(),
								};
								u = $"H::LegacyData{{0x{tableByte1:X2}, 0x{tableByte2:X2}, 0x{mandatoryPrefix:X2}}}";
								GetOps(encFlags1, legacyOps.shifts, legacyOps.mask, legacyOffset, ops);
							}
							break;

						case EncodingKind.VEX: {
							kind = "VEX";
							InitVecBase();
							bool w1 = wbit == WBit.W1;
							uint lastByte = lbit == LBit.L1 || lbit == LBit.L256 ? 4U : 0;
							if (w1)
								lastByte |= 0x80;
							lastByte |= mpByte;
							uint maskWL = wbit == WBit.WIG ? 0x80U : 0;
							uint maskL;
							if (lbit == LBit.LIG) {
								maskWL |= 4;
								maskL = 4;
							}
							else
								maskL = 0;
							u = $"H::VexData{{{tableIndex}, 0x{lastByte:X2}, 0x{maskWL:X2}, 0x{maskL:X2}, {(w1 ? "true" : "false")}}}";
							GetOps(encFlags1, vexOps.shifts, vexOps.mask, vexOffset, ops);
							break;
						}

						case EncodingKind.XOP: {
							kind = "XOP";
							InitVecBase();
							uint lastByte = lbit == LBit.L1 || lbit == LBit.L256 ? 4U : 0;
							if (wbit == WBit.W1)
								lastByte |= 0x80;
							lastByte |= mpByte;
							u = $"H::XopData{{{8 + tableIndex}, 0x{lastByte:X2}}}";
							GetOps(encFlags1, xopOps.shifts, xopOps.mask, xopOffset, ops);
							break;
						}

						case EncodingKind.EVEX: {
							kind = "EVEX";
							InitVecBase();
							uint p1Bits = 4 | mpByte;
							if (wbit == WBit.W1)
								p1Bits |= 0x80;
							uint maskLL = 0;
							uint llBits = lbit switch {
								LBit.LIG => 0U << 5,
								LBit.L0 or LBit.LZ or LBit.L128 => 0U << 5,
								LBit.L1 or LBit.L256 => 1U << 5,
								LBit.L512 => 2U << 5,
								_ => throw new InvalidOperationException(),
							};
							if (lbit == LBit.LIG)
								maskLL = 3 << 5;
							uint maskW = wbit == WBit.WIG ? 0x80U : 0;
							uint tupleType = (encFlags3 >> (int)EncFlags3.TupleTypeShift) & (uint)EncFlags3.TupleTypeMask;
							u = $"H::EvexData{{{tableIndex}, 0x{p1Bits:X2}, 0x{llBits:X2}, 0x{maskW:X2}, 0x{maskLL:X2}, {EnumStr(TypeIds.TupleType, tupleType)}}}";
							GetOps(encFlags1, evexOps.shifts, evexOps.mask, evexOffset, ops);
							break;
						}

						case EncodingKind.MVEX: {
							kind = "MVEX";
							InitVecBase();
							uint p1Bits = mpByte;
							if (wbit == WBit.W1)
								p1Bits |= 0x80;
							uint maskW = wbit == WBit.WIG ? 0x80U : 0;
							u = $"H::MvexData{{{tableIndex}, 0x{p1Bits:X2}, 0x{maskW:X2}}}";
							GetOps(encFlags1, mvexOps.shifts, mvexOps.mask, mvexOffset, ops);
							break;
						}

						case EncodingKind.D3NOW:
							kind = "D3NOW";
							handlerEncFlags3 = encFlags3;
							opCode = 0x0F;
							is2ByteOpCode = encIs2ByteOpCode;
							if (encOpCode > 0xFF)
								throw new InvalidOperationException();
							u = $"H::D3nowData{{0x{encOpCode:X2}}}";
							ops.AddRange(d3nowOps);
							break;

						default:
							throw new InvalidOperationException();
						}

						if (ops.Count > 5)
							throw new InvalidOperationException();
						var opsStr = string.Join(", ", ops.Concat(Enumerable.Repeat(0U, 5 - ops.Count)));
						writer.WriteLine($"{{0x{handlerEncFlags3:X8}, 0x{opCode:X4}, K::{kind}, {ops.Count}, {{{opsStr}}}, {groupIndex}, {rmGroupIndex}, " +
							$"{EnumStr(TypeIds.CodeSize, opSize)}, {EnumStr(TypeIds.CodeSize, addrSize)}, {(is2ByteOpCode ? "true" : "false")}, " +
							$"{(isSpecialInstr ? "true" : "false")}, {u}}},// {data.Def.Code.Name(idConverter)}");
					}
				}
				writer.WriteLine("};");
				writer.WriteLine("// clang-format on");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		// Flags used by the C++ OpCodeInfo class (OpCodeInfo::Flags)
		[Flags]
		enum OpCodeInfoFlags : uint {
			None = 0,
			IgnoresRoundingControl = 0x0001,
			AmdLockRegBit = 0x0002,
			LIG = 0x0004,
			W = 0x0008,
			WIG = 0x0010,
			WIG32 = 0x0020,
			CPL0 = 0x0040,
			CPL1 = 0x0080,
			CPL2 = 0x0100,
			CPL3 = 0x0200,
		}

		// Max chunk size (the string literal length limit is 64KB with some compilers)
		const int MaxStringsChunkSize = 0xF000;
		const int StringsChunkShift = 16;

		// Same as Rust's OpCodeInfo::new() (encoder/op_code.rs)
		void GenerateOpCodeInfoTable(OpCodeHandlers handlers, EncData[] allData) {
			var legacyOps = GetOpShifts("Legacy_", 4);
			var vexOps = GetOpShifts("VEX_", 5);
			var xopOps = GetOpShifts("XOP_", 4);
			var evexOps = GetOpShifts("EVEX_", 4);
			var mvexOps = GetOpShifts("MVEX_", 4);
			var tableKindType = genTypes[TypeIds.OpCodeTableKind];
			var mandatoryPrefixType = genTypes[TypeIds.MandatoryPrefix];
			var opKindType = genTypes[TypeIds.OpCodeOperandKind];
			var encodingType = genTypes[TypeIds.EncodingKind];
			var encFlags1IgnoresRoundingControl = EncFlags1Value(nameof(InstructionDefFlags3.IgnoresRoundingControl));
			var encFlags1AmdLockRegBit = EncFlags1Value(nameof(InstructionDefFlags3.AmdLockRegBit));

			// Split the strings into chunks
			var chunks = new List<List<(EncData data, string opCodeString, string instructionString)>>();
			var stringsOffsets = new uint[allData.Length];
			{
				List<(EncData data, string opCodeString, string instructionString)>? chunk = null;
				int chunkSize = 0;
				foreach (var data in allData) {
					var opCodeString = data.Def.OpCodeString;
					var instructionString = data.Def.InstructionString;
					foreach (var s in new[] { opCodeString, instructionString }) {
						if (s.Length > byte.MaxValue)
							throw new InvalidOperationException();
						foreach (var c in s) {
							if (c < 0x20 || c > 0x7E)
								throw new InvalidOperationException("Only printable ASCII chars are supported");
						}
					}
					int size = opCodeString.Length + instructionString.Length;
					if (chunk is null || chunkSize + size > MaxStringsChunkSize) {
						chunk = new List<(EncData data, string opCodeString, string instructionString)>();
						chunks.Add(chunk);
						chunkSize = 0;
					}
					stringsOffsets[data.Def.Code.Value] = ((uint)(chunks.Count - 1) << StringsChunkShift) | (uint)chunkSize;
					chunk.Add((data, opCodeString, instructionString));
					chunkSize += size;
				}
			}

			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(SrcFile("encoder", "op_code_info_table.cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"internal/encoder/const_init.hpp\"");
				writer.WriteLine("#include \"internal/encoder/op_code_info_internal.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine($"static_assert(OpCodeInfoInternal::STRINGS_CHUNK_SHIFT == {StringsChunkShift}, \"\");");
				writer.WriteLine();
				writer.WriteLine("namespace {");
				writer.WriteLine("// clang-format off");
				for (int i = 0; i < chunks.Count; i++) {
					if (i != 0)
						writer.WriteLine();
					writer.WriteLine($"constexpr char STRINGS_{i}[] =");
					using (writer.Indent()) {
						foreach (var (data, opCodeString, instructionString) in chunks[i])
							writer.WriteLine($"\"{CppConstants.EscapeString(opCodeString)}\" \"{CppConstants.EscapeString(instructionString)}\"// {data.Def.Code.Name(idConverter)}");
						writer.WriteLine(";");
					}
				}
				writer.WriteLine("// clang-format on");
				writer.WriteLine("} // namespace");
				writer.WriteLine();
				writer.WriteLine($"const char* const OpCodeInfoInternal::STRINGS[{chunks.Count}] = {{");
				using (writer.Indent()) {
					for (int i = 0; i < chunks.Count; i++)
						writer.WriteLine($"STRINGS_{i},");
				}
				writer.WriteLine("};");
				writer.WriteLine();
				writer.WriteLine("// code, enc_flags2, enc_flags3, opc_flags1, opc_flags2, op_code, flags, encoding, operand_size, address_size, l, tuple_type, table,");
				writer.WriteLine("// mandatory_prefix, group_index, rm_group_index, op0_kind, op1_kind, op2_kind, op3_kind, op4_kind, strings_offset,");
				writer.WriteLine("// op_code_string_len, instruction_string_len");
				writer.WriteLine("// clang-format off");
				writer.WriteLine("ICED_CONSTINIT const OpCodeInfo OpCodeInfoInternal::TABLE[IcedConstants::CODE_ENUM_COUNT] = {");
				using (writer.Indent()) {
					foreach (var data in allData) {
						var encFlags1 = data.EncFlags1;
						var encFlags2 = data.EncFlags2;
						var encFlags3 = data.EncFlags3;
						var opcFlags1 = data.OpcFlags1;

						var flags = OpCodeInfoFlags.None;
						uint opCode = (encFlags2 >> (int)EncFlags2.OpCodeShift) & 0xFFFF;
						if ((encFlags1 & encFlags1IgnoresRoundingControl) != 0)
							flags |= OpCodeInfoFlags.IgnoresRoundingControl;
						if ((encFlags1 & encFlags1AmdLockRegBit) != 0)
							flags |= OpCodeInfoFlags.AmdLockRegBit;
						flags |= ((OpCodeInfoFlags1)opcFlags1 & (OpCodeInfoFlags1.Cpl0Only | OpCodeInfoFlags1.Cpl3Only)) switch {
							OpCodeInfoFlags1.Cpl0Only => OpCodeInfoFlags.CPL0,
							OpCodeInfoFlags1.Cpl3Only => OpCodeInfoFlags.CPL3,
							_ => OpCodeInfoFlags.CPL0 | OpCodeInfoFlags.CPL1 | OpCodeInfoFlags.CPL2 | OpCodeInfoFlags.CPL3,
						};

						var encoding = (EncodingKind)((encFlags3 >> (int)EncFlags3.EncodingShift) & (uint)EncFlags3.EncodingMask);
						var mandatoryPrefix = (MandatoryPrefixByte)((encFlags2 >> (int)EncFlags2.MandatoryPrefixShift) & (uint)EncFlags2.MandatoryPrefixMask) switch {
							MandatoryPrefixByte.None => (encFlags2 & (uint)EncFlags2.HasMandatoryPrefix) != 0 ? nameof(MandatoryPrefix.PNP) : nameof(MandatoryPrefix.None),
							MandatoryPrefixByte.P66 => nameof(MandatoryPrefix.P66),
							MandatoryPrefixByte.PF3 => nameof(MandatoryPrefix.PF3),
							MandatoryPrefixByte.PF2 => nameof(MandatoryPrefix.PF2),
							_ => throw new InvalidOperationException(),
						};
						static uint CodeSizeToBits(uint codeSize) =>
							(CodeSize)codeSize switch {
								CodeSize.Unknown => 0,
								CodeSize.Code16 => 16,
								CodeSize.Code32 => 32,
								CodeSize.Code64 => 64,
								_ => throw new InvalidOperationException(),
							};
						uint operandSize = CodeSizeToBits((encFlags3 >> (int)EncFlags3.OperandSizeShift) & (uint)EncFlags3.OperandSizeMask);
						uint addressSize = CodeSizeToBits((encFlags3 >> (int)EncFlags3.AddressSizeShift) & (uint)EncFlags3.AddressSizeMask);
						int groupIndex = (encFlags2 & (uint)EncFlags2.HasGroupIndex) == 0 ? -1 : (int)((encFlags2 >> (int)EncFlags2.GroupIndexShift) & 7);
						int rmGroupIndex = (encFlags3 & (uint)EncFlags3.HasRmGroupIndex) == 0 ? -1 : (int)((encFlags2 >> (int)EncFlags2.GroupIndexShift) & 7);
						uint tupleType = (encFlags3 >> (int)EncFlags3.TupleTypeShift) & (uint)EncFlags3.TupleTypeMask;

						uint l;
						switch ((LBit)((encFlags2 >> (int)EncFlags2.LBitShift) & (uint)EncFlags2.LBitMask)) {
						case LBit.LZ: l = 0; break;
						case LBit.L0: l = 0; break;
						case LBit.L1: l = 1; break;
						case LBit.L128: l = 0; break;
						case LBit.L256: l = 1; break;
						case LBit.L512: l = 2; break;
						case LBit.LIG:
							l = 0;
							flags |= OpCodeInfoFlags.LIG;
							break;
						default: throw new InvalidOperationException();
						}

						switch ((WBit)((encFlags2 >> (int)EncFlags2.WBitShift) & (uint)EncFlags2.WBitMask)) {
						case WBit.W0: break;
						case WBit.W1: flags |= OpCodeInfoFlags.W; break;
						case WBit.WIG: flags |= OpCodeInfoFlags.WIG; break;
						case WBit.WIG32: flags |= OpCodeInfoFlags.WIG32; break;
						default: throw new InvalidOperationException();
						}

						uint tableIndex = (encFlags2 >> (int)EncFlags2.TableShift) & (uint)EncFlags2.TableMask;
						var opKinds = new EnumValue[5];
						string table;
						(uint[] shifts, uint mask) opsInfo;
						(EnumValue opCodeOperandKind, OpHandlerKind opHandlerKind, object[] args)[] opsTable;
						switch (encoding) {
						case EncodingKind.Legacy:
							opsInfo = legacyOps;
							opsTable = handlers.Legacy;
							table = (LegacyOpCodeTable)tableIndex switch {
								LegacyOpCodeTable.MAP0 => nameof(OpCodeTableKind.Normal),
								LegacyOpCodeTable.MAP0F => nameof(OpCodeTableKind.T0F),
								LegacyOpCodeTable.MAP0F38 => nameof(OpCodeTableKind.T0F38),
								LegacyOpCodeTable.MAP0F3A => nameof(OpCodeTableKind.T0F3A),
								_ => throw new InvalidOperationException(),
							};
							break;
						case EncodingKind.VEX:
							opsInfo = vexOps;
							opsTable = handlers.Vex;
							table = (VexOpCodeTable)tableIndex switch {
								VexOpCodeTable.MAP0 => nameof(OpCodeTableKind.Normal),
								VexOpCodeTable.MAP0F => nameof(OpCodeTableKind.T0F),
								VexOpCodeTable.MAP0F38 => nameof(OpCodeTableKind.T0F38),
								VexOpCodeTable.MAP0F3A => nameof(OpCodeTableKind.T0F3A),
								_ => throw new InvalidOperationException(),
							};
							break;
						case EncodingKind.EVEX:
							opsInfo = evexOps;
							opsTable = handlers.Evex;
							table = (EvexOpCodeTable)tableIndex switch {
								EvexOpCodeTable.MAP0F => nameof(OpCodeTableKind.T0F),
								EvexOpCodeTable.MAP0F38 => nameof(OpCodeTableKind.T0F38),
								EvexOpCodeTable.MAP0F3A => nameof(OpCodeTableKind.T0F3A),
								EvexOpCodeTable.MAP5 => nameof(OpCodeTableKind.MAP5),
								EvexOpCodeTable.MAP6 => nameof(OpCodeTableKind.MAP6),
								_ => throw new InvalidOperationException(),
							};
							break;
						case EncodingKind.XOP:
							opsInfo = xopOps;
							opsTable = handlers.Xop;
							table = (XopOpCodeTable)tableIndex switch {
								XopOpCodeTable.MAP8 => nameof(OpCodeTableKind.MAP8),
								XopOpCodeTable.MAP9 => nameof(OpCodeTableKind.MAP9),
								XopOpCodeTable.MAP10 => nameof(OpCodeTableKind.MAP10),
								_ => throw new InvalidOperationException(),
							};
							break;
						case EncodingKind.D3NOW:
							opsInfo = (Array.Empty<uint>(), 0);
							opsTable = Array.Empty<(EnumValue opCodeOperandKind, OpHandlerKind opHandlerKind, object[] args)>();
							opKinds[0] = opKindType["mm_reg"];
							opKinds[1] = opKindType["mm_or_mem"];
							table = nameof(OpCodeTableKind.T0F);
							break;
						case EncodingKind.MVEX:
							opsInfo = mvexOps;
							opsTable = handlers.Mvex;
							table = (MvexOpCodeTable)tableIndex switch {
								MvexOpCodeTable.MAP0F => nameof(OpCodeTableKind.T0F),
								MvexOpCodeTable.MAP0F38 => nameof(OpCodeTableKind.T0F38),
								MvexOpCodeTable.MAP0F3A => nameof(OpCodeTableKind.T0F3A),
								_ => throw new InvalidOperationException(),
							};
							break;
						default:
							throw new InvalidOperationException();
						}
						for (int i = 0; i < opsInfo.shifts.Length; i++)
							opKinds[i] = opsTable[(encFlags1 >> (int)opsInfo.shifts[i]) & opsInfo.mask].opCodeOperandKind;
						for (int i = 0; i < opKinds.Length; i++)
							opKinds[i] ??= opKindType[nameof(OpCodeOperandKind.None)];

						var code = data.Def.Code.Value;
						var opCodeStringLen = data.Def.OpCodeString.Length;
						var instructionStringLen = data.Def.InstructionString.Length;
						writer.WriteLine($"{{{code}, 0x{encFlags2:X8}, 0x{encFlags3:X8}, 0x{opcFlags1:X8}, 0x{data.OpcFlags2:X8}, 0x{opCode:X4}, 0x{(uint)flags:X4}, " +
							$"{encodingType[encoding.ToString()].Value}, {operandSize}, {addressSize}, {l}, {tupleType}, {tableKindType[table].Value}, " +
							$"{mandatoryPrefixType[mandatoryPrefix].Value}, {groupIndex}, {rmGroupIndex}, {string.Join(", ", opKinds.Select(a => a.Value))}, " +
							$"0x{stringsOffsets[code]:X8}, {opCodeStringLen}, {instructionStringLen}}},// {data.Def.Code.Name(idConverter)}");
					}
				}
				writer.WriteLine("};");
				writer.WriteLine("// clang-format on");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		protected override void Generate((EnumValue value, uint size)[] immSizes) {
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(InternalHeader("encoder", "imm_sizes.hpp")))) {
				WriteHeaderStart(writer, "<cstdint>");
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine($"inline constexpr std::uint32_t IMM_SIZES[{immSizes.Length}] = {{");
				using (writer.Indent()) {
					foreach (var info in immSizes)
						writer.WriteLine($"{info.size},// {info.value.Name(idConverter)}");
				}
				writer.WriteLine("};");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		// The op code strings and instruction strings are generated (GenerateOpCodeInfoTable()) so the C++ code doesn't need the
		// op code formatter and instruction formatter
		protected override void GenerateInstructionFormatter((EnumValue code, string result)[] notInstrStrings) { }
		protected override void GenerateOpCodeFormatter((EnumValue code, string result)[] notInstrStrings, EnumValue[] hasModRM, EnumValue[] hasVsib) { }
		protected override void GenerateCore() { }
		protected override void GenerateImpliedOps((EncodingKind Encoding, InstrStrImpliedOp[] Ops, InstructionDef[] defs)[] impliedOpsInfo) { }

		protected override void GenerateInstrSwitch(EnumValue[] jccInstr, EnumValue[] simpleBranchInstr, EnumValue[] callInstr, EnumValue[] jmpInstr, EnumValue[] xbeginInstr) {
			// Block encoder: not part of the encoder
		}

		protected override void GenerateVsib(EnumValue[] vsib32, EnumValue[] vsib64) {
			// Instruction::vsib(): part of the core (Instruction)
		}

		protected override void GenerateDecoderOptionsTable((EnumValue decOptionValue, EnumValue decoderOptions)[] values) {
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(InternalHeader("encoder", "to_decoder_options.hpp")))) {
				WriteHeaderStart(writer, "\"iced_x86/decoder_options.hpp\"", "<cstdint>");
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine("// Index = DecOptionValue");
				writer.WriteLine($"inline constexpr std::uint32_t TO_DECODER_OPTIONS[{values.Length}] = {{");
				using (writer.Indent()) {
					foreach (var (_, decoderOptions) in values)
						writer.WriteLine($"{decoderOptions.DeclaringType.Name(idConverter)}::{idConverter.Constant(decoderOptions.RawName)},");
				}
				writer.WriteLine("};");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}
	}
}
