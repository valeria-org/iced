// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System.Collections.Generic;
using Generator.IO;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Generates the instruction tables of all formatters (gas, intel, masm, nasm, fast):
	/// <list type="bullet">
	/// <item><c>src/formatter/strings_data.cpp</c> + <c>src/internal/formatter/strings_data.hpp</c>: the strings table used by the fast formatter</item>
	/// <item><c>src/formatter/fast/fmt_data.cpp</c> + <c>src/internal/formatter/fast/fmt_data.hpp</c>: the serialized fast formatter instruction table</item>
	/// <item><c>src/formatter/&lt;syntax&gt;/fmt_data.cpp</c> + <c>src/internal/formatter/&lt;syntax&gt;/fmt_data.hpp</c>: the constant gas/intel/masm/nasm
	/// instruction info tables (see <see cref="CppInstrInfoTableGen"/>)</item>
	/// </list>
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppFormatterTableGenerator {
		readonly GenTypes genTypes;

		public CppFormatterTableGenerator(GeneratorContext generatorContext) =>
			genTypes = generatorContext.Types;

		interface ICppSerializer {
			IFormatterTableSerializer Serializer { get; }
			void SerializeData(FileWriter writer, StringsTable stringsTable);
			void SerializeHeader(FileWriter writer);
			string DataFilename { get; }
			string HeaderFilename { get; }
		}

		sealed class FastSerializer : ICppSerializer {
			readonly GenTypes genTypes;
			readonly CppFastFormatterTableSerializer serializer;
			public FastSerializer(GenTypes genTypes, CppFastFormatterTableSerializer serializer) {
				this.genTypes = genTypes;
				this.serializer = serializer;
			}
			public IFormatterTableSerializer Serializer => serializer;
			public string DataFilename => serializer.DataFilename;
			public string HeaderFilename => serializer.HeaderFilename;
			public void SerializeData(FileWriter writer, StringsTable stringsTable) => serializer.SerializeData(genTypes, writer, stringsTable);
			public void SerializeHeader(FileWriter writer) => serializer.SerializeHeader(writer);
		}

		public void Generate() {
			if (genTypes.Options.HasGasFormatter)
				new CppGasInstrInfoTableGen(genTypes).Generate();
			if (genTypes.Options.HasIntelFormatter)
				new CppIntelInstrInfoTableGen(genTypes).Generate();
			if (genTypes.Options.HasMasmFormatter)
				new CppMasmInstrInfoTableGen(genTypes).Generate();
			if (genTypes.Options.HasNasmFormatter)
				new CppNasmInstrInfoTableGen(genTypes).Generate();

			// The strings table is only used by the fast formatter
			var serializers = new List<ICppSerializer>();
			if (genTypes.Options.HasFastFormatter)
				serializers.Add(new FastSerializer(genTypes, new CppFastFormatterTableSerializer(genTypes, "fast", genTypes.GetObject<Fast.FmtTblInfos>(TypeIds.FastFmtTblInfos).Infos)));

			var stringsTable = new StringsTable();

			foreach (var info in serializers)
				info.Serializer.Initialize(genTypes, stringsTable);

			stringsTable.Freeze();

			var stringsSerializer = new CppStringsTableSerializer(stringsTable);
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetSrcFilename(genTypes, "formatter", "strings_data.cpp"))))
				stringsSerializer.SerializeData(writer);
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetInternalFilename(genTypes, "formatter", "strings_data.hpp"))))
				stringsSerializer.SerializeHeader(writer);

			foreach (var info in serializers) {
				using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(info.DataFilename)))
					info.SerializeData(writer, stringsTable);
				using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(info.HeaderFilename)))
					info.SerializeHeader(writer);
			}
		}
	}
}
