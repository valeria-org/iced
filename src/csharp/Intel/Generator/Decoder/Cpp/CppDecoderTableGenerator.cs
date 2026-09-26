// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using Generator.IO;

namespace Generator.Decoder.Cpp {
	/// <summary>
	/// Generates the serialized decoder tables:
	/// <list type="bullet">
	/// <item><c>src/decoder/data_&lt;name&gt;.cpp</c>: the data</item>
	/// <item><c>src/internal/decoder/data_&lt;name&gt;.hpp</c>: the declaration and the table indexes</item>
	/// </list>
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppDecoderTableGenerator {
		readonly GeneratorContext generatorContext;

		public CppDecoderTableGenerator(GeneratorContext generatorContext) => this.generatorContext = generatorContext;

		public void Generate() {
			var genTypes = generatorContext.Types;
			var serializers = new CppDecoderTableSerializer[] {
				new CppDecoderTableSerializer(genTypes, "legacy", DecoderTableSerializerInfo.Legacy(genTypes)),
				new CppDecoderTableSerializer(genTypes, "vex", DecoderTableSerializerInfo.Vex(genTypes)),
				new CppDecoderTableSerializer(genTypes, "evex", DecoderTableSerializerInfo.Evex(genTypes)),
				new CppDecoderTableSerializer(genTypes, "xop", DecoderTableSerializerInfo.Xop(genTypes)),
				new CppDecoderTableSerializer(genTypes, "mvex", DecoderTableSerializerInfo.Mvex(genTypes)),
			};

			foreach (var serializer in serializers) {
				var dataFilename = CppConstants.GetSrcFilename(genTypes, "decoder", $"data_{serializer.TableName}.cpp");
				using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(dataFilename)))
					serializer.SerializeData(writer);
				var headerFilename = CppConstants.GetInternalFilename(genTypes, "decoder", $"data_{serializer.TableName}.hpp");
				using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(headerFilename)))
					serializer.SerializeHeader(writer);
			}
		}
	}
}
