// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using Generator.Documentation.Cpp;
using Generator.IO;

namespace Generator.Constants.Cpp {
	[Generator(TargetLanguage.Cpp)]
	sealed class CppConstantsGenerator : ConstantsGenerator {
		readonly IdentifierConverter idConverter;
		readonly Dictionary<TypeId, FullConstantsFileInfo?> toFullFileInfo;
		readonly CppConstantsWriter constantsWriter;

		sealed class FullConstantsFileInfo {
			public readonly string Filename;
			public readonly string Namespace;
			public readonly string[] Includes;
			public readonly Action<FileWriter>? WriteExtra;

			public FullConstantsFileInfo(string filename, string ns, string[] includes, Action<FileWriter>? writeExtra = null) {
				Filename = filename;
				Namespace = ns;
				Includes = includes;
				WriteExtra = writeExtra;
			}
		}

		public CppConstantsGenerator(GeneratorContext generatorContext)
			: base(generatorContext.Types) {
			idConverter = CppIdentifierConverter.Create();
			constantsWriter = new CppConstantsWriter(genTypes, idConverter, new CppDocCommentWriter(idConverter), new CppDeprecatedWriter(idConverter));

			toFullFileInfo = new();
			toFullFileInfo.Add(TypeIds.IcedConstants, new FullConstantsFileInfo(CppConstants.GetIncludeFilename(genTypes, "iced_constants.hpp"), CppConstants.Namespace,
				new[] { "<cstddef>", "<cstdint>", "\"iced_x86/code.hpp\"", "\"iced_x86/register.hpp\"", "\"iced_x86/memory_size.hpp\"" }, WriteIcedConstantsExtra));
			toFullFileInfo.Add(TypeIds.DecoderTestParserConstants, TestFile("decoder_test_parser_constants.hpp"));
			toFullFileInfo.Add(TypeIds.DecoderConstants, TestFile("decoder_constants.hpp"));
			toFullFileInfo.Add(TypeIds.InstructionInfoKeys, TestFile("instruction_info_keys.hpp"));
			toFullFileInfo.Add(TypeIds.MiscInstrInfoTestConstants, TestFile("misc_instr_info_test_constants.hpp"));
			toFullFileInfo.Add(TypeIds.RflagsBitsConstants, TestFile("rflags_bits_constants.hpp"));
			toFullFileInfo.Add(TypeIds.MiscSectionNames, TestFile("misc_section_names.hpp"));
			toFullFileInfo.Add(TypeIds.OpCodeInfoKeys, TestFile("op_code_info_keys.hpp"));
			toFullFileInfo.Add(TypeIds.OpCodeInfoFlags, TestFile("op_code_info_flags.hpp"));
		}

		FullConstantsFileInfo TestFile(string filename) =>
			new(CppConstants.GetTestFilename(genTypes, "generated", filename), CppConstants.TestsNamespace,
				new[] { "<cstddef>", "<cstdint>", "\"iced_x86/register.hpp\"", "\"iced_x86/memory_size.hpp\"" });

		static void WriteIcedConstantsExtra(FileWriter writer) {
			writer.WriteLine();
			writer.WriteLine("/// Returns `true` if it's an MVEX instruction");
			writer.WriteLine("static constexpr bool is_mvex(Code code) noexcept {");
			using (writer.Indent())
				writer.WriteLine("return (static_cast<std::uint32_t>(code) - MVEX_START) < MVEX_LENGTH;");
			writer.WriteLine("}");
		}

		public override void Generate(ConstantsType constantsType) {
			if (toFullFileInfo.TryGetValue(constantsType.TypeId, out var info)) {
				if (info is null)
					return;
				using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(info.Filename))) {
					CppConstants.WriteHeaderFileHeader(writer);
					foreach (var inc in info.Includes)
						writer.WriteLine($"#include {inc}");
					writer.WriteLine();
					CppConstants.WriteNamespaceBegin(writer, info.Namespace);
					constantsWriter.Write(writer, constantsType, info.WriteExtra);
					CppConstants.WriteNamespaceEnd(writer, info.Namespace);
				}
			}
			else
				throw new InvalidOperationException();
		}
	}
}
