// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System.Linq;
using Generator.Enums;
using Generator.IO;
using Generator.Tables;

namespace Generator.Encoder.Cpp {
	/// <summary>
	/// Generates the block encoder's instruction switch (<c>src/block_encoder/instr.cpp</c>).
	/// Same code lists as <see cref="EncoderGenerator"/> passes to <c>GenerateInstrSwitch()</c>.
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppBlockEncoderGenerator {
		readonly GenTypes genTypes;
		readonly IdentifierConverter idConverter;

		public CppBlockEncoderGenerator(GeneratorContext generatorContext) {
			genTypes = generatorContext.Types;
			idConverter = CppIdentifierConverter.Create();
		}

		public void Generate() {
			var defs = genTypes.GetObject<InstructionDefs>(TypeIds.InstructionDefs).Defs;
			var jccInstr = defs.Where(a =>
				a.BranchKind == BranchKind.JccShort || a.BranchKind == BranchKind.JccNear ||
				a.BranchKind == BranchKind.JkccShort || a.BranchKind == BranchKind.JkccNear).Select(a => a.Code).OrderBy(a => a.Value).ToArray();
			var simpleBranchInstr = defs.Where(a => a.BranchKind == BranchKind.Loop || a.BranchKind == BranchKind.Jrcxz).Select(a => a.Code).OrderBy(a => a.Value).ToArray();
			var callInstr = defs.Where(a => a.BranchKind == BranchKind.CallNear).Select(a => a.Code).OrderBy(a => a.Value).ToArray();
			var jmpInstr = defs.Where(a => a.BranchKind == BranchKind.JmpShort || a.BranchKind == BranchKind.JmpNear).Select(a => a.Code).OrderBy(a => a.Value).ToArray();
			var xbeginInstr = defs.Where(a => a.BranchKind == BranchKind.Xbegin).Select(a => a.Code).OrderBy(a => a.Value).ToArray();

			var filename = CppConstants.GetSrcFilename(genTypes, "block_encoder", "instr.cpp");
			GenerateCases(filename, "JccInstr", jccInstr, "JccInstr::create(block_encoder, base, instr, instruction);");
			GenerateCases(filename, "SimpleBranchInstr", simpleBranchInstr, "SimpleBranchInstr::create(block_encoder, base, instr, instruction);");
			GenerateCases(filename, "CallInstr", callInstr, "CallInstr::create(block_encoder, base, instr, instruction);");
			GenerateCases(filename, "JmpInstr", jmpInstr, "JmpInstr::create(block_encoder, base, instr, instruction);");
			GenerateCases(filename, "XbeginInstr", xbeginInstr, "XbeginInstr::create(block_encoder, base, instr, instruction);");
		}

		void GenerateCases(string filename, string id, EnumValue[] codeValues, string statement) =>
			new FileUpdater(TargetLanguage.Cpp, id, filename).Generate(writer => {
				if (codeValues.Length == 0)
					return;
				foreach (var value in codeValues)
					writer.WriteLine($"case {idConverter.ToDeclTypeAndValue(value)}:");
				using (writer.Indent()) {
					writer.WriteLine(statement);
					writer.WriteLine("return;");
				}
			});
	}
}
