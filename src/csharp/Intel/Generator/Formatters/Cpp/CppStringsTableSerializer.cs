// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using Generator.IO;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Writes the strings table used by all formatters (namespace <c>iced_x86::internal::strings_data</c>)
	/// </summary>
	sealed class CppStringsTableSerializer {
		// Must match FastStringMnemonic in the C++ fast formatter
		const int FastStringMnemonicSize = 20;

		readonly StringsTable stringsTable;
		readonly int maxStringLength;
		readonly int extraPadding;
		readonly int byteCount;

		const string Namespace = CppConstants.InternalNamespace + "::strings_data";

		public CppStringsTableSerializer(StringsTable stringsTable) {
			if (!stringsTable.IsFrozen)
				throw new InvalidOperationException();
			this.stringsTable = stringsTable;

			var sortedInfos = stringsTable.Infos;
			maxStringLength = 0;
			foreach (var info in sortedInfos)
				maxStringLength = Math.Max(maxStringLength, info.String.Length);
			if (maxStringLength > FastStringMnemonicSize) {
				// Requires updating the C++ fast formatter's `FastStringMnemonic` to match the new aligned size
				throw new InvalidOperationException();
			}
			var last = sortedInfos[^1];
			extraPadding = FastStringMnemonicSize - last.String.Length;
			if (extraPadding < 0)
				throw new InvalidOperationException();
			byteCount = StringsTableSerializerUtils.GetByteCount(sortedInfos) + extraPadding;
		}

		public void SerializeHeader(FileWriter writer) {
			CppConstants.WriteHeaderFileHeader(writer);
			writer.WriteLine("#include <cstddef>");
			writer.WriteLine("#include <cstdint>");
			writer.WriteLine();
			CppConstants.WriteNamespaceBegin(writer, Namespace);
			writer.WriteLine($"constexpr std::size_t STRINGS_COUNT = {stringsTable.Infos.Length};");
			writer.WriteLine($"constexpr std::size_t MAX_STRING_LEN = {maxStringLength};");
			writer.WriteLine($"constexpr std::size_t VALID_STRING_LENGTH = {FastStringMnemonicSize};");
			writer.WriteLine($"constexpr std::size_t PADDING_SIZE = {extraPadding};");
			writer.WriteLine($"constexpr std::size_t STRINGS_TBL_DATA_SIZE = {byteCount};");
			writer.WriteLine("/// Each string is stored as a length byte followed by the ASCII chars. The last string is followed by padding");
			writer.WriteLine("/// so it's possible to read `VALID_STRING_LENGTH` bytes from any string.");
			writer.WriteLine("extern const std::uint8_t STRINGS_TBL_DATA[STRINGS_TBL_DATA_SIZE];");
			CppConstants.WriteNamespaceEnd(writer, Namespace);
		}

		public void SerializeData(FileWriter writer) {
			writer.WriteFileHeader();
			writer.WriteLine("#include \"internal/formatter/strings_data.hpp\"");
			writer.WriteLine();
			CppConstants.WriteNamespaceBegin(writer, Namespace);
			writer.WriteLine("// clang-format off");
			writer.WriteLine("extern const std::uint8_t STRINGS_TBL_DATA[STRINGS_TBL_DATA_SIZE] = {");
			using (writer.Indent())
				StringsTableSerializerUtils.SerializeTable(new TextFileByteTableWriter(writer), stringsTable.Infos, extraPadding, "Padding so it's possible to read FastStringMnemonic::SIZE bytes from the last value");
			writer.WriteLine("};");
			writer.WriteLine("// clang-format on");
			CppConstants.WriteNamespaceEnd(writer, Namespace);
		}
	}
}
