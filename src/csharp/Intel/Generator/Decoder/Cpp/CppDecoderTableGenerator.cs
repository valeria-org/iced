// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using Generator.IO;

namespace Generator.Decoder.Cpp {
	/// <summary>
	/// Generates the decoder tables as constant data (see <see cref="CppDecoderTableWriter"/>):
	/// <list type="bullet">
	/// <item><c>src/decoder/data_&lt;name&gt;.cpp</c>: the handlers and the 0x100-entry tables</item>
	/// <item><c>src/internal/decoder/data_&lt;name&gt;.hpp</c>: the table declarations</item>
	/// </list>
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppDecoderTableGenerator {
		readonly GeneratorContext generatorContext;

		public CppDecoderTableGenerator(GeneratorContext generatorContext) => this.generatorContext = generatorContext;

		public void Generate() {
			var genTypes = generatorContext.Types;
			var writers = new CppDecoderTableWriter[] {
				new CppDecoderTableWriter("legacy", "legacy", DecoderTableSerializerInfo.Legacy(genTypes)),
				new CppDecoderTableWriter("vex", "vex", DecoderTableSerializerInfo.Vex(genTypes)),
				new CppDecoderTableWriter("evex", "evex", DecoderTableSerializerInfo.Evex(genTypes)),
				new CppDecoderTableWriter("xop", "vex", DecoderTableSerializerInfo.Xop(genTypes)),
				new CppDecoderTableWriter("mvex", "mvex", DecoderTableSerializerInfo.Mvex(genTypes)),
			};

			foreach (var writer in writers) {
				var dataFilename = CppConstants.GetSrcFilename(genTypes, "decoder", $"data_{writer.TableName}.cpp");
				using (var fileWriter = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(dataFilename)))
					writer.WriteSource(fileWriter);
				var headerFilename = CppConstants.GetInternalFilename(genTypes, "decoder", $"data_{writer.TableName}.hpp");
				using (var fileWriter = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(headerFilename)))
					writer.WriteHeader(fileWriter);
			}
		}
	}
}
