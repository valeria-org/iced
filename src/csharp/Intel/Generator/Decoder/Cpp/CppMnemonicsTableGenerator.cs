// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using Generator.Constants;
using Generator.IO;
using Generator.Tables;

namespace Generator.Decoder.Cpp {
	[Generator(TargetLanguage.Cpp)]
	sealed class CppMnemonicsTableGenerator {
		readonly IdentifierConverter idConverter;
		readonly GeneratorContext generatorContext;

		public CppMnemonicsTableGenerator(GeneratorContext generatorContext) {
			idConverter = CppIdentifierConverter.Create();
			this.generatorContext = generatorContext;
		}

		public void Generate() {
			var genTypes = generatorContext.Types;
			var icedConstants = genTypes.GetConstantsType(TypeIds.IcedConstants);
			var defs = genTypes.GetObject<InstructionDefs>(TypeIds.InstructionDefs).Defs;
			var mnemonicName = genTypes[TypeIds.Mnemonic].Name(idConverter);
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetSrcFilename(genTypes, "generated", "mnemonics.cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"iced_x86/code_ext.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine($"const {mnemonicName} TO_MNEMONIC[{icedConstants.Name(idConverter)}::{icedConstants[IcedConstants.GetEnumCountName(TypeIds.Code)].Name(idConverter)}] = {{");
				using (writer.Indent()) {
					foreach (var def in defs) {
						if (def.Mnemonic.Value > ushort.MaxValue)
							throw new InvalidOperationException();
						writer.WriteLine($"{CppConstants.ToEnumValue(idConverter, def.Mnemonic)},// {def.Code.Name(idConverter)}");
					}
				}
				writer.WriteLine("};");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}
	}
}
