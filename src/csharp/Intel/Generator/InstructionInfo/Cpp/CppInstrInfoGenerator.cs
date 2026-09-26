// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Linq;
using Generator.Constants;
using Generator.Constants.Cpp;
using Generator.Documentation.Cpp;
using Generator.Enums;
using Generator.Enums.Cpp;
using Generator.Enums.InstructionInfo;
using Generator.IO;
using Generator.Tables;

namespace Generator.InstructionInfo.Cpp {
	[Generator(TargetLanguage.Cpp)]
	sealed class CppInstrInfoGenerator : InstrInfoGenerator {
		readonly IdentifierConverter idConverter;
		readonly CppEnumsGenerator enumGenerator;
		readonly CppConstantsWriter constantsWriter;
		readonly GeneratorContext generatorContext;
		readonly EnumType opAccessType;
		readonly EnumType registerType;
		readonly EnumType codeSizeType;

		public CppInstrInfoGenerator(GeneratorContext generatorContext)
			: base(generatorContext.Types) {
			idConverter = CppIdentifierConverter.Create();
			enumGenerator = new CppEnumsGenerator(generatorContext);
			constantsWriter = new CppConstantsWriter(genTypes, idConverter, new CppDocCommentWriter(idConverter), new CppDeprecatedWriter(idConverter));
			this.generatorContext = generatorContext;
			opAccessType = generatorContext.Types[TypeIds.OpAccess];
			registerType = generatorContext.Types[TypeIds.Register];
			codeSizeType = generatorContext.Types[TypeIds.CodeSize];
		}

		protected override void Generate(EnumType enumType) => enumGenerator.Generate(enumType);

		protected override void Generate(ConstantsType constantsType) {
			if (constantsType.TypeId != TypeIds.InstrInfoConstants)
				throw new InvalidOperationException();
			var filename = CppConstants.GetInternalFilename(genTypes, "info", "instr_info_constants.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include <cstddef>");
				writer.WriteLine("#include <cstdint>");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				constantsWriter.Write(writer, constantsType);
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		void WriteSrcFile(string name, Action<FileWriter> write) {
			var filename = CppConstants.GetSrcFilename(genTypes, "info", name);
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"internal/info/info_tables.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				write(writer);
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		static string Hex32(uint value) => NumberFormatter.FormatHexUInt32WithSep(value).Replace('_', '\'');

		protected override void Generate((InstructionDef def, uint dword1, uint dword2)[] infos) {
			WriteSrcFile("info_table.cpp", writer => {
				writer.WriteLine($"const InfoTableEntry INFO_TABLE[{infos.Length}] = {{");
				using (writer.Indent()) {
					foreach (var info in infos)
						writer.WriteLine($"{{{Hex32(info.dword1)}, {Hex32(info.dword2)}}},// {info.def.Code.Name(idConverter)}");
				}
				writer.WriteLine("};");
			});
		}

		protected override void Generate(EnumValue[] enumValues, RflagsBits[] read, RflagsBits[] undefined, RflagsBits[] written, RflagsBits[] cleared, RflagsBits[] set, RflagsBits[] modified) {
			WriteSrcFile("rflags_table.cpp", writer => {
				var infos = new (RflagsBits[] rflags, string name)[] {
					(read, "read"),
					(undefined, "undefined"),
					(written, "written"),
					(cleared, "cleared"),
					(set, "set"),
					(modified, "modified"),
				};
				bool first = true;
				foreach (var info in infos) {
					var rflags = info.rflags;
					if (rflags.Length != infos[0].rflags.Length)
						throw new InvalidOperationException();
					if (!first)
						writer.WriteLine();
					first = false;
					var name = idConverter.Static("flags" + info.name[0..1].ToUpperInvariant() + info.name[1..]);
					writer.WriteLine($"const std::uint16_t {name}[{rflags.Length}] = {{");
					using (writer.Indent()) {
						for (int i = 0; i < rflags.Length; i++) {
							uint value = (uint)rflags[i];
							if (value > ushort.MaxValue)
								throw new InvalidOperationException();
							writer.WriteLine($"{Hex32(value)},// {enumValues[i].Name(idConverter)}");
						}
					}
					writer.WriteLine("};");
				}
			});
		}

		protected override void Generate((EnumValue cpuidInternal, EnumValue[] cpuidFeatures)[] cpuidFeatures) {
			WriteSrcFile("cpuid_table.cpp", writer => {
				int total = cpuidFeatures.Sum(a => a.cpuidFeatures.Length);
				if (total > ushort.MaxValue)
					throw new InvalidOperationException();
				writer.WriteLine($"const CpuidFeature CPUID_FEATURES[{total}] = {{");
				using (writer.Indent()) {
					foreach (var info in cpuidFeatures)
						writer.WriteLine($"{string.Join(", ", info.cpuidFeatures.Select(a => idConverter.ToDeclTypeAndValue(a)))},// {info.cpuidInternal.Name(idConverter)}");
				}
				writer.WriteLine("};");
				writer.WriteLine();
				writer.WriteLine($"const CpuidTableEntry CPUID_TABLE[{cpuidFeatures.Length}] = {{");
				using (writer.Indent()) {
					int offset = 0;
					foreach (var info in cpuidFeatures) {
						if (info.cpuidFeatures.Length == 0 || info.cpuidFeatures.Length > byte.MaxValue)
							throw new InvalidOperationException();
						writer.WriteLine($"{{{offset}, {info.cpuidFeatures.Length}}},// {info.cpuidInternal.Name(idConverter)}");
						offset += info.cpuidFeatures.Length;
					}
				}
				writer.WriteLine("};");
			});
		}

		protected override void GenerateCore() => GenerateOpAccesses();

		void GenerateOpAccesses() {
			var filename = CppConstants.GetInternalFilename(genTypes, "info", "op_accesses.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine($"#include \"{enumGenerator.GetIncludePath(TypeIds.OpAccess)}\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);

				var opInfos = instrInfoTypes.EnumOpInfos;
				// We assume max op count is 5, update the code if not
				Static.Assert(IcedConstants.MaxOpCount == 5 ? 0 : -1);

				var indexes = new int[] { 1, 2 };
				var opAccessTypeStr = opAccessType.Name(idConverter);
				bool first = true;
				foreach (var index in indexes) {
					var opInfo = opInfos[index];
					if (!first)
						writer.WriteLine();
					first = false;
					var name = idConverter.Constant($"OpAccess_{index}");
					writer.WriteLine($"inline constexpr {opAccessTypeStr} {name}[{opInfo.Values.Length}] = {{");
					using (writer.Indent()) {
						foreach (var value in opInfo.Values) {
							var v = ToOpAccess(value);
							writer.WriteLine($"{idConverter.ToDeclTypeAndValue(v)},");
						}
					}
					writer.WriteLine("};");
				}

				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		protected override void GenerateImpliedAccesses(ImpliedAccessesDef[] defs) {
			var filename = CppConstants.GetSrcFilename(genTypes, "info", "instruction_info_factory.cpp");
			new FileUpdater(TargetLanguage.Cpp, "ImpliedAccessHandler", filename).Generate(writer => GenerateImpliedAccesses(writer, defs));
		}

		void GenerateImpliedAccesses(FileWriter writer, ImpliedAccessesDef[] defs) {
			foreach (var def in defs) {
				writer.WriteLine($"case {GetEnumName(def.EnumValue)}:");
				using (writer.Indent()) {
					foreach (var cond in def.ImpliedAccesses.Conditions) {
						var condStr = GetConditionString(cond.Kind);
						if (condStr is null) {
							if (cond.FalseStatements.Count > 0)
								throw new InvalidOperationException();
							GenerateImpliedAccesses(writer, cond.TrueStatements);
						}
						else {
							writer.WriteLine($"if ({condStr}) {{");
							using (writer.Indent())
								GenerateImpliedAccesses(writer, cond.TrueStatements);
							if (cond.FalseStatements.Count > 0) {
								writer.WriteLine("} else {");
								using (writer.Indent())
									GenerateImpliedAccesses(writer, cond.FalseStatements);
							}
							writer.WriteLine("}");
						}
					}
					writer.WriteLine("break;");
				}
			}

			static string? GetConditionString(ImplAccConditionKind kind) =>
				kind switch {
					ImplAccConditionKind.None => null,
					ImplAccConditionKind.Bit64 => "(flags & Flags::IS_64BIT) != 0",
					ImplAccConditionKind.NotBit64 => "(flags & Flags::IS_64BIT) == 0",
					_ => throw new InvalidOperationException(),
				};
		}

		void GenerateImpliedAccesses(FileWriter writer, List<ImplAccStatement> stmts) {
			var stmtState = new StmtState(
				"if ((flags & Flags::NO_REGISTER_USAGE) == 0) {",
				"}",
				"if ((flags & Flags::NO_MEMORY_USAGE) == 0) {",
				"}");
			var registerTypeStr = registerType.Name(idConverter);
			foreach (var stmt in stmts) {
				IntArgImplAccStatement arg1;
				IntX2ArgImplAccStatement arg2;
				stmtState.SetKind(writer, stmt.Kind);
				switch (stmt.Kind) {
				case ImplAccStatementKind.MemoryAccess:
					var mem = (MemoryImplAccStatement)stmt;
					writer.WriteLine($"add_memory(info, {GetRegisterString(mem.Segment)}, {GetRegisterString(mem.Base)}, {GetRegisterString(mem.Index)}, {mem.Scale}, 0x{mem.Displacement:X}, {GetMemorySizeString(mem.MemorySize)}, {GetOpAccessString(mem.Access)}, {GetCodeSizeString(mem.AddressSize)}, {mem.VsibSize});");
					break;
				case ImplAccStatementKind.RegisterAccess:
					var reg = (RegisterImplAccStatement)stmt;
					if (reg.IsMemOpSegRead && CouldBeNullSegIn64BitMode(reg.Register, out var definitelyNullSeg)) {
						if (definitelyNullSeg) {
							writer.WriteLine("if ((flags & Flags::IS_64BIT) == 0) {");
							using (writer.Indent())
								writer.WriteLine($"add_register(flags, info, {GetRegisterString(reg.Register)}, {GetOpAccessString(reg.Access)});");
							writer.WriteLine("}");
						}
						else
							writer.WriteLine($"add_memory_segment_register(flags, info, {GetRegisterString(reg.Register)}, {GetOpAccessString(reg.Access)});");
					}
					else
						writer.WriteLine($"add_register(flags, info, {GetRegisterString(reg.Register)}, {GetOpAccessString(reg.Access)});");
					break;
				case ImplAccStatementKind.RegisterRangeAccess:
					var rreg = (RegisterRangeImplAccStatement)stmt;
					writer.WriteLine($"for (std::uint32_t reg_num = static_cast<std::uint32_t>({GetEnumName(rreg.RegisterFirst)}); reg_num <= static_cast<std::uint32_t>({GetEnumName(rreg.RegisterLast)}); reg_num++)");
					using (writer.Indent())
						writer.WriteLine($"add_register(flags, info, static_cast<{registerTypeStr}>(reg_num), {GetOpAccessString(rreg.Access)});");
					break;
				case ImplAccStatementKind.ShiftMask:
					arg1 = (IntArgImplAccStatement)stmt;
					break;
				case ImplAccStatementKind.ShiftMask1FMod:
					arg1 = (IntArgImplAccStatement)stmt;
					Verify_9_or_17(arg1.Arg);
					break;
				case ImplAccStatementKind.ZeroRegRflags:
					writer.WriteLine("command_clear_rflags(instruction, info, flags);");
					break;
				case ImplAccStatementKind.ZeroRegRegmem:
					writer.WriteLine("command_clear_reg_regmem(instruction, info, flags);");
					break;
				case ImplAccStatementKind.ZeroRegRegRegmem:
					writer.WriteLine("command_clear_reg_reg_regmem(instruction, info, flags);");
					break;
				case ImplAccStatementKind.Arpl:
					writer.WriteLine("command_arpl(instruction, info, flags);");
					break;
				case ImplAccStatementKind.LastGpr8:
					writer.WriteLine($"command_last_gpr(instruction, info, flags, {GetEnumName(registerType[nameof(Register.AL)])});");
					break;
				case ImplAccStatementKind.LastGpr16:
					writer.WriteLine($"command_last_gpr(instruction, info, flags, {GetEnumName(registerType[nameof(Register.AX)])});");
					break;
				case ImplAccStatementKind.LastGpr32:
					writer.WriteLine($"command_last_gpr(instruction, info, flags, {GetEnumName(registerType[nameof(Register.EAX)])});");
					break;
				case ImplAccStatementKind.EmmiReg:
					var emmi = (EmmiImplAccStatement)stmt;
					writer.WriteLine($"command_emmi(instruction, info, flags, {GetEnumName(GetOpAccess(opAccessType, emmi.Access))});");
					break;
				case ImplAccStatementKind.Enter:
					arg1 = (IntArgImplAccStatement)stmt;
					writer.WriteLine($"command_enter(instruction, info, flags, {Verify_2_4_or_8(arg1.Arg)});");
					break;
				case ImplAccStatementKind.Leave:
					arg1 = (IntArgImplAccStatement)stmt;
					writer.WriteLine($"command_leave(instruction, info, flags, {Verify_2_4_or_8(arg1.Arg)});");
					break;
				case ImplAccStatementKind.Push:
					arg2 = (IntX2ArgImplAccStatement)stmt;
					if (arg2.Arg1 != 0)
						writer.WriteLine($"command_push(instruction, info, flags, {arg2.Arg1}, {Verify_2_4_or_8(arg2.Arg2)});");
					break;
				case ImplAccStatementKind.Pop:
					arg2 = (IntX2ArgImplAccStatement)stmt;
					if (arg2.Arg1 != 0)
						writer.WriteLine($"command_pop(instruction, info, flags, {arg2.Arg1}, {Verify_2_4_or_8(arg2.Arg2)});");
					break;
				case ImplAccStatementKind.PopRm:
					arg1 = (IntArgImplAccStatement)stmt;
					writer.WriteLine($"command_pop_rm(instruction, info, flags, {Verify_2_4_or_8(arg1.Arg)});");
					break;
				case ImplAccStatementKind.Pusha:
					arg1 = (IntArgImplAccStatement)stmt;
					writer.WriteLine($"command_pusha(instruction, info, flags, {Verify_2_or_4(arg1.Arg)});");
					break;
				case ImplAccStatementKind.Popa:
					arg1 = (IntArgImplAccStatement)stmt;
					writer.WriteLine($"command_popa(instruction, info, flags, {Verify_2_or_4(arg1.Arg)});");
					break;
				case ImplAccStatementKind.lea:
					writer.WriteLine("command_lea(instruction, info, flags);");
					break;
				case ImplAccStatementKind.Cmps:
					writer.WriteLine("command_cmps(instruction, info, flags);");
					break;
				case ImplAccStatementKind.Ins:
					writer.WriteLine("command_ins(instruction, info, flags);");
					break;
				case ImplAccStatementKind.Lods:
					writer.WriteLine("command_lods(instruction, info, flags);");
					break;
				case ImplAccStatementKind.Movs:
					writer.WriteLine("command_movs(instruction, info, flags);");
					break;
				case ImplAccStatementKind.Outs:
					writer.WriteLine("command_outs(instruction, info, flags);");
					break;
				case ImplAccStatementKind.Scas:
					writer.WriteLine("command_scas(instruction, info, flags);");
					break;
				case ImplAccStatementKind.Stos:
					writer.WriteLine("command_stos(instruction, info, flags);");
					break;
				case ImplAccStatementKind.Xstore:
					arg1 = (IntArgImplAccStatement)stmt;
					writer.WriteLine($"command_xstore(instruction, info, flags, {Verify_2_4_or_8(arg1.Arg)});");
					break;
				case ImplAccStatementKind.MemDispl:
					arg1 = (IntArgImplAccStatement)stmt;
					writer.WriteLine($"command_mem_displ(info, flags, {(int)arg1.Arg});");
					break;
				default:
					throw new InvalidOperationException();
				}
			}
			stmtState.Done(writer);
		}

		string GetMemorySizeString(ImplAccMemorySize memorySize) {
			switch (memorySize.Kind) {
			case ImplAccMemorySizeKind.MemorySize:
				if (memorySize.MemorySize is null)
					throw new InvalidOperationException();
				return GetEnumName(memorySize.MemorySize);
			case ImplAccMemorySizeKind.Default:
				return "instruction.memory_size()";
			default:
				throw new InvalidOperationException();
			}
		}

		string GetRegisterString(ImplAccRegister? register) {
			if (register == null)
				return GetEnumName(registerType[nameof(Register.None)]);
			var reg = register.GetValueOrDefault();
			switch (reg.Kind) {
			case ImplAccRegisterKind.Register:
				if (reg.Register is null)
					throw new InvalidOperationException();
				return GetEnumName(reg.Register);
			case ImplAccRegisterKind.SegmentDefaultDS: return "get_seg_default_ds(instruction)";
			case ImplAccRegisterKind.a_rDI: return "get_a_rdi(instruction)";
			case ImplAccRegisterKind.Op0: return "instruction.op0_register()";
			case ImplAccRegisterKind.Op1: return "instruction.op1_register()";
			case ImplAccRegisterKind.Op2: return "instruction.op2_register()";
			case ImplAccRegisterKind.Op3: return "instruction.op3_register()";
			case ImplAccRegisterKind.Op4: return "instruction.op4_register()";
			default: throw new InvalidOperationException();
			}
		}

		string GetOpAccessString(OpAccess access) => GetEnumName(opAccessType[access.ToString()]);
		string GetCodeSizeString(CodeSize codeSize) => GetEnumName(codeSizeType[codeSize.ToString()]);

		string GetEnumName(EnumValue value) => idConverter.ToDeclTypeAndValue(value);

		// The Code tables (ignores segment/index, tile stride index, is string op) are used by the core (always compiled) so they're
		// generated by CppInstrInfoCoreTablesGenerator (src/internal/code_internal.hpp and include/iced_x86/code_ext.hpp)
		protected override void GenerateIgnoresSegmentTable((EncodingKind encoding, InstructionDef[] defs)[] defs) { }
		protected override void GenerateIgnoresIndexTable((EncodingKind encoding, InstructionDef[] defs)[] defs) { }
		protected override void GenerateTileStrideIndexTable((EncodingKind encoding, InstructionDef[] defs)[] defs) { }
		protected override void GenerateIsStringOpTable((EncodingKind encoding, InstructionDef[] defs)[] defs) { }

		protected override void GenerateFpuStackIncrementInfoTable((FpuStackInfo info, InstructionDef[] defs)[] tdefs) {
			var filename = CppConstants.GetSrcFilename(genTypes, "info", "instruction_info_methods.cpp");
			new FileUpdater(TargetLanguage.Cpp, "FpuStackIncrementInfoTable", filename).Generate(writer => {
				foreach (var (info, defs) in tdefs) {
					foreach (var def in defs)
						writer.WriteLine($"case {idConverter.ToDeclTypeAndValue(def.Code)}:");
					var conditionalStr = info.Conditional ? "true" : "false";
					var writesTopStr = info.WritesTop ? "true" : "false";
					using (writer.Indent())
						writer.WriteLine($"return FpuStackIncrementInfo({info.Increment}, {conditionalStr}, {writesTopStr});");
				}
			});
		}

		protected override void GenerateStackPointerIncrementTable((EncodingKind encoding, StackInfo info, InstructionDef[] defs)[] tdefs) {
			var filename = CppConstants.GetSrcFilename(genTypes, "info", "instruction_info_methods.cpp");
			var codeSizeTypeStr = codeSizeType.Name(idConverter);
			new FileUpdater(TargetLanguage.Cpp, "StackPointerIncrementTable", filename).Generate(writer => {
				foreach (var (encoding, info, defs) in tdefs) {
					var expr = info.Kind switch {
						StackInfoKind.Increment => $"return {info.Value};",
						StackInfoKind.Enter => $"return -({info.Value} + (static_cast<std::int32_t>(immediate8_2nd()) & 0x1F) * {info.Value} + static_cast<std::int32_t>(immediate16()));",
						StackInfoKind.Iret => $"return code_size() == {codeSizeTypeStr}::Code64 ? {info.Value} * 5 : {info.Value} * 3;",
						StackInfoKind.PopImm16 => $"return {info.Value} + static_cast<std::int32_t>(immediate16());",
						_ => throw new InvalidOperationException(),
					};
					foreach (var def in defs)
						writer.WriteLine($"case {idConverter.ToDeclTypeAndValue(def.Code)}:");
					using (writer.Indent())
						writer.WriteLine(expr);
				}
			});
		}
	}
}
