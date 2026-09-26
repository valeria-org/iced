// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using Generator.Enums;
using Generator.IO;

namespace Generator.Formatters.Cpp {
	static class CppFormatterTableSerializerUtils {
		public static string GetNamespace(string syntax) => $"{CppConstants.InternalNamespace}::{syntax}";
		public static string GetDataFilename(GenTypes genTypes, string syntax) => CppConstants.GetSrcFilename(genTypes, "formatter", syntax, "fmt_data.cpp");
		public static string GetHeaderFilename(GenTypes genTypes, string syntax) => CppConstants.GetInternalFilename(genTypes, "formatter", syntax, "fmt_data.hpp");
		public static string GetHeaderIncludePath(string syntax) => $"internal/formatter/{syntax}/fmt_data.hpp";

		public static void WriteDataFileStart(FileWriter writer, string syntax) {
			writer.WriteFileHeader();
			writer.WriteLine($"#include \"{GetHeaderIncludePath(syntax)}\"");
			writer.WriteLine();
			CppConstants.WriteNamespaceBegin(writer, GetNamespace(syntax));
			writer.WriteLine("// clang-format off");
			writer.WriteLine("extern const std::uint8_t FORMATTER_TBL_DATA[] = {");
		}

		public static void WriteDataFileEnd(FileWriter writer, string syntax) {
			writer.WriteLine("};");
			writer.WriteLine("// clang-format on");
			writer.WriteLine("extern const std::size_t FORMATTER_TBL_DATA_SIZE = sizeof(FORMATTER_TBL_DATA);");
			CppConstants.WriteNamespaceEnd(writer, GetNamespace(syntax));
		}

		public static void WriteHeader(FileWriter writer, string syntax) {
			CppConstants.WriteHeaderFileHeader(writer);
			writer.WriteLine("#include <cstddef>");
			writer.WriteLine("#include <cstdint>");
			writer.WriteLine();
			CppConstants.WriteNamespaceBegin(writer, GetNamespace(syntax));
			writer.WriteLine("/// Serialized formatter table data (one entry per `Code` value)");
			writer.WriteLine("extern const std::uint8_t FORMATTER_TBL_DATA[];");
			writer.WriteLine("/// Size of `FORMATTER_TBL_DATA` in bytes");
			writer.WriteLine("extern const std::size_t FORMATTER_TBL_DATA_SIZE;");
			CppConstants.WriteNamespaceEnd(writer, GetNamespace(syntax));
		}
	}

	sealed class CppFastFormatterTableSerializer : FastFormatterTableSerializer {
		readonly GenTypes genTypes;
		readonly string syntax;

		public CppFastFormatterTableSerializer(GenTypes genTypes, string syntax, FastFmtInstructionDef[] defs)
			: base(defs, CppIdentifierConverter.Create()) {
			this.genTypes = genTypes;
			this.syntax = syntax;
		}

		public string DataFilename => CppFormatterTableSerializerUtils.GetDataFilename(genTypes, syntax);
		public string HeaderFilename => CppFormatterTableSerializerUtils.GetHeaderFilename(genTypes, syntax);
		public override string GetFilename(GenTypes genTypes) => DataFilename;

		public void SerializeData(GenTypes genTypes, FileWriter writer, StringsTable stringsTable) {
			CppFormatterTableSerializerUtils.WriteDataFileStart(writer, syntax);
			using (writer.Indent())
				SerializeTable(genTypes, new TextFileByteTableWriter(writer), stringsTable);
			CppFormatterTableSerializerUtils.WriteDataFileEnd(writer, syntax);
		}

		public void SerializeHeader(FileWriter writer) =>
			CppFormatterTableSerializerUtils.WriteHeader(writer, syntax);
	}
}
