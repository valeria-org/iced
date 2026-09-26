// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using Generator.Constants;
using Generator.IO;

namespace Generator.Tables.Cpp {
	[Generator(TargetLanguage.Cpp)]
	sealed class CppRegisterInfoTableGenerator {
		readonly IdentifierConverter idConverter;
		readonly GeneratorContext generatorContext;

		public CppRegisterInfoTableGenerator(GeneratorContext generatorContext) {
			idConverter = CppIdentifierConverter.Create();
			this.generatorContext = generatorContext;
		}

		public void Generate() {
			var genTypes = generatorContext.Types;
			var defs = genTypes.GetObject<RegisterDefs>(TypeIds.RegisterDefs).Defs;
			if (genTypes[TypeIds.Register].Values.Length > 0x100)
				throw new InvalidOperationException();
			var icedConstants = genTypes.GetConstantsType(TypeIds.IcedConstants);
			var countName = $"{icedConstants.Name(idConverter)}::{icedConstants[IcedConstants.GetEnumCountName(TypeIds.Register)].Name(idConverter)}";
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetSrcFilename(genTypes, "generated", "register_infos.cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"iced_x86/register_ext.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine($"const RegisterInfo REGISTER_INFOS[{countName}] = {{");
				using (writer.Indent()) {
					foreach (var def in defs) {
						if (def.Size > ushort.MaxValue)
							throw new InvalidOperationException();
						writer.WriteLine($"RegisterInfo({CppConstants.ToEnumValue(idConverter, def.Register)}, {CppConstants.ToEnumValue(idConverter, def.BaseRegister)}, {CppConstants.ToEnumValue(idConverter, def.FullRegister32)}, {CppConstants.ToEnumValue(idConverter, def.FullRegister)}, {def.Size}),");
					}
				}
				writer.WriteLine("};");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}
	}
}
