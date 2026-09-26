// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Linq;
using Generator.Enums;
using Generator.Enums.Cpp;
using Generator.IO;

namespace Generator.Tables.Cpp {
	/// <summary>
	/// Generates the dictionaries used by the C++ unit tests (port of <c>RustDictGenerator</c>):
	/// <c>tests/generated/test_dicts.hpp</c> + <c>tests/generated/test_dicts.cpp</c>
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppDictGenerator {
		readonly IdentifierConverter idConverter;
		readonly GeneratorContext generatorContext;

		public CppDictGenerator(GeneratorContext generatorContext) {
			idConverter = CppIdentifierConverter.Create();
			this.generatorContext = generatorContext;
		}

		public void Generate() {
			var genTypes = generatorContext.Types;
			var dicts = new (string name, (string name, EnumValue value)[] constants)[] {
				("OP_ACCESS_DICT", InstrInfoDictConstants.OpAccessConstants(genTypes)),
				("MEMORY_SIZE_FLAGS_DICT", InstrInfoDictConstants.MemorySizeFlagsTable(genTypes)),
				("REGISTER_FLAGS_DICT", InstrInfoDictConstants.RegisterFlagsTable(genTypes)),
				("ENCODING_KIND_DICT", EncoderConstants.EncodingKindTable(genTypes)),
				("MANDATORY_PREFIX_DICT", EncoderConstants.MandatoryPrefixTable(genTypes)),
				("OP_CODE_TABLE_KIND_DICT", EncoderConstants.OpCodeTableKindTable(genTypes)),
				("MASM_SYMBOL_TEST_FLAGS_DICT", MasmSymbolOptionsConstants.SymbolTestFlagsTable(genTypes)),
				("FORMAT_MNEMONIC_OPTIONS_DICT", FormatMnemonicOptionsConstants.FormatMnemonicOptionsTable(genTypes)),
				("SYMBOL_FLAGS_DICT", SymbolFlagsConstants.SymbolFlagsTable(genTypes)),
			};
			var tables = dicts.Select(a => new CppNameValueTable(a.name, a.constants[0].value.DeclaringType, a.constants)).ToArray();
			var ignoredCodes = genTypes.GetObject<HashSet<EnumValue>>(TypeIds.RemovedCodeValues).OrderBy(a => a.Value).Select(a => a.RawName).ToArray();

			var writer = new CppNameValueTableWriter(generatorContext, idConverter);
			writer.Write("test_dicts", tables, w => {
				w.WriteLine();
				w.WriteLine("/// Names of all `Code` values that were removed by the generator (the tests should ignore them)");
				w.WriteLine($"extern const std::array<const char*, {ignoredCodes.Length}> IGNORED_CODE_NAMES;");
			}, w => {
				w.WriteLine();
				if (ignoredCodes.Length == 0)
					w.WriteLine($"const std::array<const char*, 0> IGNORED_CODE_NAMES = {{}};");
				else {
					w.WriteLine($"const std::array<const char*, {ignoredCodes.Length}> IGNORED_CODE_NAMES = {{{{");
					using (w.Indent()) {
						foreach (var name in ignoredCodes)
							w.WriteLine($"\"{CppConstants.EscapeString(name)}\",");
					}
					w.WriteLine("}};");
				}
			});
		}
	}

	/// <summary>
	/// A <c>std::array&lt;NameValue&lt;T&gt;, N&gt;</c> table
	/// </summary>
	sealed class CppNameValueTable {
		public readonly string Name;
		public readonly EnumType EnumType;
		public readonly (string name, EnumValue value)[] Values;

		public CppNameValueTable(string name, EnumType enumType, (string name, EnumValue value)[] values) {
			Name = name;
			EnumType = enumType;
			Values = values;
			foreach (var (_, value) in values) {
				if (value.DeclaringType != enumType)
					throw new InvalidOperationException();
			}
		}
	}

	/// <summary>
	/// Writes <c>tests/generated/&lt;name&gt;.hpp</c> (<c>extern</c> declarations) and <c>tests/generated/&lt;name&gt;.cpp</c> (the data)
	/// with <c>std::array&lt;NameValue&lt;T&gt;, N&gt;</c> tables. Flags enums use <c>std::uint32_t</c> values.
	/// </summary>
	sealed class CppNameValueTableWriter {
		readonly GeneratorContext generatorContext;
		readonly IdentifierConverter idConverter;
		readonly CppEnumsGenerator enumsGenerator;

		public CppNameValueTableWriter(GeneratorContext generatorContext, IdentifierConverter idConverter) {
			this.generatorContext = generatorContext;
			this.idConverter = idConverter;
			// Only used to get the include paths of the enums
			enumsGenerator = new CppEnumsGenerator(generatorContext);
		}

		string GetValueType(EnumType enumType) =>
			enumType.IsFlags ? "std::uint32_t" : enumType.Name(idConverter);

		string GetValue(EnumValue value) {
			var enumType = value.DeclaringType;
			// Deprecated values have a [[deprecated]] attribute so use the numeric value to prevent a warning
			if (enumType.IsFlags) {
				if (value.DeprecatedInfo.IsDeprecated)
					return $"0x{value.Value:X}U";
				return $"{enumType.Name(idConverter)}::{idConverter.Constant(value.RawName)}";
			}
			if (value.DeprecatedInfo.IsDeprecated)
				return $"static_cast<{enumType.Name(idConverter)}>({value.Value})";
			return $"{enumType.Name(idConverter)}::{value.Name(idConverter)}";
		}

		public void Write(string baseName, CppNameValueTable[] tables, Action<FileWriter>? writeHeaderExtra = null, Action<FileWriter>? writeSourceExtra = null) {
			var genTypes = generatorContext.Types;
			var ns = CppConstants.TestsNamespace;

			var includes = tables.Select(a => "\"" + enumsGenerator.GetIncludePath(a.EnumType.TypeId) + "\"").
				Distinct().OrderBy(a => a, StringComparer.Ordinal).ToList();
			includes.Add("\"test_utils/name_value.hpp\"");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetTestFilename(genTypes, "generated", baseName + ".hpp")))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include <array>");
				writer.WriteLine("#include <cstdint>");
				foreach (var include in includes)
					writer.WriteLine($"#include {include}");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, ns);
				bool first = true;
				foreach (var table in tables) {
					if (!first)
						writer.WriteLine();
					first = false;
					writer.WriteLine($"extern const std::array<NameValue<{GetValueType(table.EnumType)}>, {table.Values.Length}> {table.Name};");
				}
				writeHeaderExtra?.Invoke(writer);
				CppConstants.WriteNamespaceEnd(writer, ns);
			}

			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetTestFilename(genTypes, "generated", baseName + ".cpp")))) {
				writer.WriteFileHeader();
				writer.WriteLine($"#include \"generated/{baseName}.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, ns);
				bool first = true;
				foreach (var table in tables) {
					if (!first)
						writer.WriteLine();
					first = false;
					var declType = $"std::array<NameValue<{GetValueType(table.EnumType)}>, {table.Values.Length}>";
					if (table.Values.Length == 0)
						writer.WriteLine($"const {declType} {table.Name} = {{}};");
					else {
						writer.WriteLine($"const {declType} {table.Name} = {{{{");
						using (writer.Indent()) {
							foreach (var (name, value) in table.Values)
								writer.WriteLine($"{{\"{CppConstants.EscapeString(name)}\", {GetValue(value)}}},");
						}
						writer.WriteLine("}};");
					}
				}
				writeSourceExtra?.Invoke(writer);
				CppConstants.WriteNamespaceEnd(writer, ns);
			}
		}
	}
}
