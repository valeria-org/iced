// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Generator.Enums;
using Generator.Enums.Cpp;
using Generator.IO;
using Generator.Tables;

namespace Generator.Encoder.Cpp {
	/// <summary>
	/// Generates the C++ encoder and op code info tables. Files:
	/// <list type="bullet">
	/// <item><c>src/internal/encoder/op_kind_tables.hpp</c>: <c>OpCodeOperandKind</c> tables used by <c>OpCodeInfo</c></item>
	/// <item><c>src/internal/encoder/ops_tables.hpp</c>: operand handlers (<c>Op</c> instances) used by the encoder handlers</item>
	/// <item><c>src/internal/encoder/encoder_data.hpp</c> + <c>src/encoder/encoder_data.cpp</c>: <c>ENC_FLAGS1..3</c></item>
	/// <item><c>src/internal/encoder/op_code_data.hpp</c> + <c>src/encoder/op_code_data.cpp</c>: <c>OPC_FLAGS1..2</c></item>
	/// <item><c>src/internal/encoder/imm_sizes.hpp</c>, <c>mnemonic_str_tbl.hpp</c>, <c>to_decoder_options.hpp</c></item>
	/// <item>Generated regions in <c>src/encoder/instruction_fmt.cpp</c> and <c>src/encoder/op_code_fmt.cpp</c></item>
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

		protected override void Generate(OpCodeHandlers handlers) {
			GenerateOpCodeOperandKindTables(handlers);
			GenerateOpTables(handlers);
		}

		void GenerateOpCodeOperandKindTables(OpCodeHandlers handlers) {
			var filename = InternalHeader("encoder", "op_kind_tables.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				WriteHeaderStart(writer, "\"iced_x86/op_code_operand_kind.hpp\"");
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				Generate(writer, "LEGACY_OP_KINDS", handlers.Legacy, false);
				Generate(writer, "VEX_OP_KINDS", handlers.Vex, true);
				Generate(writer, "XOP_OP_KINDS", handlers.Xop, true);
				Generate(writer, "EVEX_OP_KINDS", handlers.Evex, true);
				Generate(writer, "MVEX_OP_KINDS", handlers.Mvex, true);
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}

			void Generate(FileWriter writer, string name, (EnumValue opCodeOperandKind, OpHandlerKind opHandlerKind, object[] args)[] table, bool addNewLine) {
				var declTypeStr = genTypes[TypeIds.OpCodeOperandKind].Name(idConverter);
				if (addNewLine)
					writer.WriteLine();
				writer.WriteLine($"inline constexpr {declTypeStr} {name}[{table.Length}] = {{");
				using (writer.Indent()) {
					foreach (var info in table)
						writer.WriteLine($"{idConverter.ToDeclTypeAndValue(info.opCodeOperandKind)},");
				}
				writer.WriteLine("};");
			}
		}

		void GenerateOpTables(OpCodeHandlers handlers) {
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

			var filename = InternalHeader("encoder", "ops_tables.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("// Only included by src/encoder/op_code_handler.cpp");
				writer.WriteLine();
				writer.WriteLine("#include \"internal/encoder/ops.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);

				foreach (var kv in dict.OrderBy(a => a.Value.Name, StringComparer.Ordinal)) {
					var info = kv.Value;
					var structName = idConverter.Type(GetStructName(info.OpHandlerKind));
					writer.Write($"static constexpr {structName} {info.Name}");
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

				writer.WriteLine();
				WriteTable(writer, "LEGACY_TABLE", dict, handlers.Legacy.Select(a => (a.opCodeOperandKind, a.opHandlerKind, a.args)));
				writer.WriteLine();
				WriteTable(writer, "VEX_TABLE", dict, handlers.Vex.Select(a => (a.opCodeOperandKind, a.opHandlerKind, a.args)));
				writer.WriteLine();
				WriteTable(writer, "XOP_TABLE", dict, handlers.Xop.Select(a => (a.opCodeOperandKind, a.opHandlerKind, a.args)));
				writer.WriteLine();
				WriteTable(writer, "EVEX_TABLE", dict, handlers.Evex.Select(a => (a.opCodeOperandKind, a.opHandlerKind, a.args)));
				writer.WriteLine();
				WriteTable(writer, "MVEX_TABLE", dict, handlers.Mvex.Select(a => (a.opCodeOperandKind, a.opHandlerKind, a.args)));
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}

			void WriteTable(FileWriter writer, string name, Dictionary<(OpHandlerKind opHandlerKind, object[] args), OpInfo> dict, IEnumerable<(EnumValue opKind, OpHandlerKind opHandlerKind, object[] args)> values) {
				var all = values.ToArray();
				writer.WriteLine($"static constexpr const Op* {name}[{all.Length}] = {{");
				using (writer.Indent()) {
					foreach (var value in all) {
						var info = dict[(value.opHandlerKind, value.args)];
						writer.WriteLine($"&{info.Name},// {value.opKind.Name(idConverter)}");
					}
				}
				writer.WriteLine("};");
			}

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
			var allData = GetData(defs).ToArray();
			var encoderInfo = new (string name, (InstructionDef def, uint value)[] values)[] {
				("ENC_FLAGS1", allData.Select(a => (a.def, a.encFlags1)).ToArray()),
				("ENC_FLAGS2", allData.Select(a => (a.def, a.encFlags2)).ToArray()),
				("ENC_FLAGS3", allData.Select(a => (a.def, a.encFlags3)).ToArray()),
			};
			var opCodeInfo = new (string name, (InstructionDef def, uint value)[] values)[] {
				("OPC_FLAGS1", allData.Select(a => (a.def, a.opcFlags1)).ToArray()),
				("OPC_FLAGS2", allData.Select(a => (a.def, a.opcFlags2)).ToArray()),
			};

			GenerateTables(defs, encoderInfo, "encoder_data");
			GenerateTables(defs, opCodeInfo, "op_code_data");
		}

		void GenerateTables(InstructionDef[] defs, (string name, (InstructionDef def, uint value)[] values)[] tables, string baseName) {
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(InternalHeader("encoder", baseName + ".hpp")))) {
				WriteHeaderStart(writer, "<cstdint>");
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				foreach (var info in tables)
					writer.WriteLine($"extern const std::uint32_t {info.name}[{defs.Length}];");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}

			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(SrcFile("encoder", baseName + ".cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine($"#include \"internal/encoder/{baseName}.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				bool first = true;
				foreach (var info in tables) {
					if (!first)
						writer.WriteLine();
					first = false;
					writer.WriteLine($"const std::uint32_t {info.name}[{defs.Length}] = {{");
					using (writer.Indent()) {
						foreach (var vinfo in info.values)
							writer.WriteLine($"0x{vinfo.value:X8},// {vinfo.def.Code.Name(idConverter)}");
					}
					writer.WriteLine("};");
				}
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

		void GenerateCases(string filename, string id, EnumValue[] values, string statement) =>
			new FileUpdater(TargetLanguage.Cpp, id, filename).Generate(writer => {
				if (values.Length == 0)
					return;
				foreach (var value in values)
					writer.WriteLine($"case {idConverter.ToDeclTypeAndValue(value)}:");
				using (writer.Indent())
					writer.WriteLine($"{statement};");
			});

		void GenerateNotInstrCases(string filename, string id, (EnumValue code, string result)[] notInstrStrings) =>
			new FileUpdater(TargetLanguage.Cpp, id, filename).Generate(writer => {
				foreach (var info in notInstrStrings) {
					writer.WriteLine($"case {idConverter.ToDeclTypeAndValue(info.code)}:");
					using (writer.Indent())
						writer.WriteLine($"return std::string(\"{CppConstants.EscapeString(info.result)}\");");
				}
			});

		protected override void GenerateInstructionFormatter((EnumValue code, string result)[] notInstrStrings) {
			var filename = SrcFile("encoder", "instruction_fmt.cpp");
			GenerateNotInstrCases(filename, "InstrFmtNotInstructionString", notInstrStrings);
		}

		protected override void GenerateOpCodeFormatter((EnumValue code, string result)[] notInstrStrings, EnumValue[] hasModRM, EnumValue[] hasVsib) {
			var filename = SrcFile("encoder", "op_code_fmt.cpp");
			GenerateNotInstrCases(filename, "OpCodeFmtNotInstructionString", notInstrStrings);
			GenerateCases(filename, "HasModRM", hasModRM, "return true");
			GenerateCases(filename, "HasVsib", hasVsib, "return true");
		}

		protected override void GenerateCore() =>
			GenerateMnemonicStringTable();

		void GenerateMnemonicStringTable() {
			var values = genTypes[TypeIds.Mnemonic].Values;
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(InternalHeader("encoder", "mnemonic_str_tbl.hpp")))) {
				CppConstants.WriteHeaderFileHeader(writer);
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine($"inline constexpr const char* TO_MNEMONIC_STR[{values.Length}] = {{");
				using (writer.Indent()) {
					foreach (var value in values)
						writer.WriteLine($"\"{value.RawName.ToLowerInvariant()}\",");
				}
				writer.WriteLine("};");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

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

		protected override void GenerateImpliedOps((EncodingKind Encoding, InstrStrImpliedOp[] Ops, InstructionDef[] defs)[] impliedOpsInfo) {
			var filename = SrcFile("encoder", "instruction_fmt.cpp");
			new FileUpdater(TargetLanguage.Cpp, "PrintImpliedOps", filename).Generate(writer => {
				foreach (var info in impliedOpsInfo) {
					foreach (var def in info.defs)
						writer.WriteLine($"case {idConverter.ToDeclTypeAndValue(def.Code)}:");
					using (writer.Indent()) {
						foreach (var op in info.Ops) {
							writer.WriteLine("write_op_separator();");
							writer.WriteLine($"write(\"{CppConstants.EscapeString(op.Operand)}\", {(op.IsUpper ? "true" : "false")});");
						}
						writer.WriteLine("break;");
					}
				}
			});
		}
	}
}
