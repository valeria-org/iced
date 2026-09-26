// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using Generator.Enums;
using Generator.IO;

namespace Generator.Tables.Cpp {
	/// <summary>
	/// Generates <c>src/internal/decoder/d3now_code_values.hpp</c> (3DNow! opcode (imm8) -> <c>Code</c>)
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppD3nowCodeValuesTableGenerator : D3nowCodeValuesTableGenerator {
		readonly GeneratorContext generatorContext;
		readonly IdentifierConverter idConverter;

		public CppD3nowCodeValuesTableGenerator(GeneratorContext generatorContext)
			: base(generatorContext.Types) {
			this.generatorContext = generatorContext;
			idConverter = CppIdentifierConverter.Create();
		}

		protected override void Generate((int index, EnumValue enumValue)[] infos) {
			var filename = CppConstants.GetInternalFilename(genTypes, "decoder", "d3now_code_values.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include \"iced_x86/code.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine("// clang-format off");
				writer.WriteLine("inline constexpr Code D3NOW_CODE_VALUES[0x100] = {");
				using (writer.Indent())
					WriteTable(writer, infos);
				writer.WriteLine("};");
				writer.WriteLine("// clang-format on");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}

		void WriteTable(FileWriter writer, (int index, EnumValue enumValue)[] infos) {
			var values = new EnumValue?[0x100];
			foreach (var info in infos) {
				if (values[info.index] is not null)
					throw new InvalidOperationException();
				values[info.index] = info.enumValue;
			}
			var invalid = genTypes[TypeIds.Code][nameof(Code.INVALID)];
			foreach (var value in values) {
				var enumValue = value ?? invalid;
				writer.WriteLine($"{idConverter.ToDeclTypeAndValue(enumValue)},");
			}
		}
	}
}
