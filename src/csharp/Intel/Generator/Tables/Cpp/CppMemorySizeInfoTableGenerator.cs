// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using Generator.Constants;
using Generator.IO;

namespace Generator.Tables.Cpp {
	[Generator(TargetLanguage.Cpp)]
	sealed class CppMemorySizeInfoTableGenerator {
		readonly IdentifierConverter idConverter;
		readonly GeneratorContext generatorContext;

		public CppMemorySizeInfoTableGenerator(GeneratorContext generatorContext) {
			idConverter = CppIdentifierConverter.Create();
			this.generatorContext = generatorContext;
		}

		public void Generate() {
			var genTypes = generatorContext.Types;
			var defs = genTypes.GetObject<MemorySizeDefs>(TypeIds.MemorySizeDefs).Defs;
			var icedConstants = genTypes.GetConstantsType(TypeIds.IcedConstants);
			var countName = $"{icedConstants.Name(idConverter)}::{icedConstants[IcedConstants.GetEnumCountName(TypeIds.MemorySize)].Name(idConverter)}";
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetSrcFilename(genTypes, "generated", "memory_size_infos.cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"iced_x86/memory_size_ext.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine($"const MemorySizeInfo MEMORY_SIZE_INFOS[{countName}] = {{");
				using (writer.Indent()) {
					foreach (var info in defs) {
						if (info.Size > ushort.MaxValue || info.ElementSize > ushort.MaxValue)
							throw new InvalidOperationException();
						writer.WriteLine($"MemorySizeInfo({CppConstants.ToEnumValue(idConverter, info.MemorySize)}, {info.Size}, {info.ElementSize}, {CppConstants.ToEnumValue(idConverter, info.ElementType)}, {(info.IsSigned ? "true" : "false")}, {(info.IsBroadcast ? "true" : "false")}),");
					}
				}
				writer.WriteLine("};");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}
	}
}
