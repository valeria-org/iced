// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using Generator.IO;

namespace Generator.Decoder.Cpp {
	sealed class CppDecoderTableSerializer : DecoderTableSerializer {
		public string TableName { get; }
		string Namespace => $"{CppConstants.InternalNamespace}::decoder_data_{TableName}";

		public CppDecoderTableSerializer(GenTypes genTypes, string tableName, DecoderTableSerializerInfo info)
			: base(genTypes, CppIdentifierConverter.Create(), info) => TableName = tableName;

		public void SerializeData(FileWriter writer) {
			writer.WriteFileHeader();
			writer.WriteLine($"#include \"internal/decoder/data_{TableName}.hpp\"");
			writer.WriteLine();
			CppConstants.WriteNamespaceBegin(writer, Namespace);
			writer.WriteLine("// clang-format off");
			writer.WriteLine("extern const std::uint8_t TBL_DATA[] = {");
			using (writer.Indent())
				SerializeCore(new TextFileByteTableWriter(writer));
			writer.WriteLine("};");
			writer.WriteLine("// clang-format on");
			writer.WriteLine("extern const std::size_t TBL_DATA_SIZE = sizeof(TBL_DATA);");
			CppConstants.WriteNamespaceEnd(writer, Namespace);
		}

		public void SerializeHeader(FileWriter writer) {
			// Must be called after SerializeData() since it initializes the table infos
			CppConstants.WriteHeaderFileHeader(writer);
			writer.WriteLine("#include <cstddef>");
			writer.WriteLine("#include <cstdint>");
			writer.WriteLine();
			CppConstants.WriteNamespaceBegin(writer, Namespace);
			writer.WriteLine("extern const std::uint8_t TBL_DATA[];");
			writer.WriteLine("extern const std::size_t TBL_DATA_SIZE;");
			writer.WriteLine($"constexpr std::size_t MAX_ID_NAMES = {info.TablesToSerialize.Length};");
			foreach (var name in info.TableIndexNames) {
				var constName = idConverter.Constant($"{name}Index");
				writer.WriteLine($"constexpr std::size_t {constName} = {GetInfo(name).Index};");
			}
			CppConstants.WriteNamespaceEnd(writer, Namespace);
		}
	}
}
