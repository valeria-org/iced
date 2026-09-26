// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using Generator.Enums;
using Generator.Enums.Cpp;
using Generator.IO;
using Generator.Tables;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Generates the string -> flags dictionaries used by the formatter tests (<c>tests/generated/formatter_test_dicts.hpp</c>)
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppFormatterDictGenerator {
		readonly IdentifierConverter idConverter;
		readonly GeneratorContext generatorContext;

		public CppFormatterDictGenerator(GeneratorContext generatorContext) {
			idConverter = CppIdentifierConverter.Create();
			this.generatorContext = generatorContext;
		}

		public void Generate() {
			var genTypes = generatorContext.Types;
			var enumsGen = new CppEnumsGenerator(generatorContext);
			var dicts = new (string funcName, (string name, EnumValue value)[] constants)[] {
				("create_format_mnemonic_options_dict", FormatMnemonicOptionsConstants.FormatMnemonicOptionsTable(genTypes)),
				("create_symbol_flags_dict", SymbolFlagsConstants.SymbolFlagsTable(genTypes)),
				("create_masm_symbol_test_flags_dict", MasmSymbolOptionsConstants.SymbolTestFlagsTable(genTypes)),
			};
			var filename = CppConstants.GetTestFilename(genTypes, "generated", "formatter_test_dicts.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include <cstdint>");
				writer.WriteLine("#include <string>");
				writer.WriteLine("#include <unordered_map>");
				writer.WriteLine($"#include \"{enumsGen.GetIncludePath(TypeIds.FormatMnemonicOptions)}\"");
				writer.WriteLine($"#include \"{enumsGen.GetIncludePath(TypeIds.SymbolFlags)}\"");
				writer.WriteLine($"#include \"{enumsGen.GetIncludePath(TypeIds.MasmSymbolTestFlags)}\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.TestsNamespace);
				bool first = true;
				foreach (var (funcName, constants) in dicts) {
					if (!first)
						writer.WriteLine();
					first = false;
					WriteDict(writer, funcName, constants);
				}
				CppConstants.WriteNamespaceEnd(writer, CppConstants.TestsNamespace);
			}
		}

		void WriteDict(FileWriter writer, string funcName, (string name, EnumValue value)[] constants) {
			var declType = constants[0].value.DeclaringType;
			var declTypeStr = declType.Name(idConverter);
			var valueType = declType.IsFlags ? "std::uint32_t" : declTypeStr;
			writer.WriteLine($"inline std::unordered_map<std::string, {valueType}> {funcName}() {{");
			using (writer.Indent()) {
				writer.WriteLine("return {");
				using (writer.Indent()) {
					foreach (var constant in constants) {
						var name = declType.IsFlags ? idConverter.Constant(constant.value.RawName) : constant.value.Name(idConverter);
						writer.WriteLine($"{{\"{CppConstants.EscapeString(constant.name)}\", {declTypeStr}::{name}}},");
					}
				}
				writer.WriteLine("};");
			}
			writer.WriteLine("}");
		}
	}
}
