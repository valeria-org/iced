// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using Generator.Constants;
using Generator.IO;
using Generator.Tables;

namespace Generator.Decoder.Cpp {
	[Generator(TargetLanguage.Cpp)]
	sealed class CppInstructionOpCountsGenerator {
		readonly IdentifierConverter idConverter;
		readonly GeneratorContext generatorContext;

		public CppInstructionOpCountsGenerator(GeneratorContext generatorContext) {
			idConverter = CppIdentifierConverter.Create();
			this.generatorContext = generatorContext;
		}

		public void Generate() {
			var genTypes = generatorContext.Types;
			var icedConstants = genTypes.GetConstantsType(TypeIds.IcedConstants);
			var defs = genTypes.GetObject<InstructionDefs>(TypeIds.InstructionDefs).Defs;
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetSrcFilename(genTypes, "generated", "instruction_op_counts.cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"iced_x86/instruction.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine($"const std::uint8_t OP_COUNT[{icedConstants.Name(idConverter)}::{icedConstants[IcedConstants.GetEnumCountName(TypeIds.Code)].Name(idConverter)}] = {{");
				using (writer.Indent()) {
					foreach (var def in defs) {
						if ((uint)def.OpCount > byte.MaxValue)
							throw new InvalidOperationException();
						writer.WriteLine($"{def.OpCount},// {def.Code.Name(idConverter)}");
					}
				}
				writer.WriteLine("};");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}
	}
}
