// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Linq;
using Generator.Constants;
using Generator.Enums;
using Generator.IO;
using Generator.Tables;

namespace Generator.Decoder.Cpp {
	[Generator(TargetLanguage.Cpp)]
	sealed class CppInstructionMemorySizesGenerator {
		readonly IdentifierConverter idConverter;
		readonly GeneratorContext generatorContext;

		public CppInstructionMemorySizesGenerator(GeneratorContext generatorContext) {
			idConverter = CppIdentifierConverter.Create();
			this.generatorContext = generatorContext;
		}

		public void Generate() {
			var genTypes = generatorContext.Types;
			var icedConstants = genTypes.GetConstantsType(TypeIds.IcedConstants);
			var defs = genTypes.GetObject<InstructionDefs>(TypeIds.InstructionDefs).Defs;
			var memSizeName = genTypes[TypeIds.MemorySize].Name(idConverter);
			var countName = $"{icedConstants.Name(idConverter)}::{icedConstants[IcedConstants.GetEnumCountName(TypeIds.Code)].Name(idConverter)}";
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetSrcFilename(genTypes, "generated", "instruction_memory_sizes.cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"iced_x86/instruction.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				WriteTable(writer, $"const {memSizeName} SIZES_NORMAL[{countName}]", defs.Select(a => (a.Code, a.Memory)));
				writer.WriteLine();
				WriteTable(writer, $"const {memSizeName} SIZES_BCST[{countName}]", defs.Select(a => (a.Code, a.MemoryBroadcast)));
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		void WriteTable(FileWriter writer, string decl, System.Collections.Generic.IEnumerable<(EnumValue code, EnumValue memSize)> values) {
			writer.WriteLine($"{decl} = {{");
			using (writer.Indent()) {
				foreach (var (code, memSize) in values) {
					if (memSize.Value > byte.MaxValue)
						throw new InvalidOperationException();
					writer.WriteLine($"{CppConstants.ToEnumValue(idConverter, memSize)},// {code.Name(idConverter)}");
				}
			}
			writer.WriteLine("};");
		}
	}
}
