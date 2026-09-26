// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using Generator.Constants;
using Generator.Enums;
using Generator.IO;

namespace Generator.Tables.Cpp {
	[Generator(TargetLanguage.Cpp)]
	sealed class CppRegToAddrSizeGenerator {
		readonly GenTypes genTypes;
		readonly IdentifierConverter idConverter;

		public CppRegToAddrSizeGenerator(GeneratorContext generatorContext) {
			genTypes = generatorContext.Types;
			idConverter = CppIdentifierConverter.Create();
		}

		public void Generate() {
			var registerType = genTypes[TypeIds.Register];
			var icedConstantsName = genTypes.GetConstantsType(TypeIds.IcedConstants).Name(idConverter);
			var regCountName = idConverter.Constant(IcedConstants.GetEnumCountName(TypeIds.Register));
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetSrcFilename(genTypes, "generated", "reg_to_addr_size.cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine("#include \"internal/instruction_internal.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine($"const std::uint8_t REG_TO_ADDR_SIZE[{icedConstantsName}::{regCountName}] = {{");
				using (writer.Indent()) {
					foreach (var regEnum in registerType.Values) {
						var reg = (Register)regEnum.Value;
						var size = GetAddrSize(reg);
						if (size > byte.MaxValue)
							throw new InvalidOperationException();
						writer.WriteLine($"{size},// {regEnum.Name(idConverter)}");
					}
				}
				writer.WriteLine("};");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		static uint GetAddrSize(Register reg) {
			if (reg >= Register.AX && reg <= Register.R15W)
				return 2;
			if (reg >= Register.EAX && reg <= Register.R15D)
				return 4;
			if (reg >= Register.RAX && reg <= Register.R15)
				return 8;
			if (reg == Register.EIP)
				return 4;
			if (reg == Register.RIP)
				return 8;
			return 0;
		}
	}
}
