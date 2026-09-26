// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System.Collections.Generic;
using Generator.IO;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Generates the serialized formatter tables of all formatters (gas, intel, masm, nasm, fast):
	/// <list type="bullet">
	/// <item><c>src/formatter/strings_data.cpp</c> + <c>src/internal/formatter/strings_data.hpp</c>: the strings table shared by all formatters</item>
	/// <item><c>src/formatter/&lt;syntax&gt;/fmt_data.cpp</c> + <c>src/internal/formatter/&lt;syntax&gt;/fmt_data.hpp</c>: the per-syntax instruction tables</item>
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

		sealed class FmtSerializer : ICppSerializer {
			readonly CppFormatterTableSerializer serializer;
			public FmtSerializer(CppFormatterTableSerializer serializer) => this.serializer = serializer;
			public IFormatterTableSerializer Serializer => serializer;
			public string DataFilename => serializer.DataFilename;
			public string HeaderFilename => serializer.HeaderFilename;
			public void SerializeData(FileWriter writer, StringsTable stringsTable) => serializer.SerializeData(writer, stringsTable);
			public void SerializeHeader(FileWriter writer) => serializer.SerializeHeader(writer);
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
			var serializers = new List<ICppSerializer>();
			if (genTypes.Options.HasGasFormatter)
				serializers.Add(new FmtSerializer(new CppFormatterTableSerializer(genTypes, "gas", genTypes.GetObject<Gas.CtorInfos>(TypeIds.GasCtorInfos).Infos, genTypes[TypeIds.GasCtorKind])));
			if (genTypes.Options.HasIntelFormatter)
				serializers.Add(new FmtSerializer(new CppFormatterTableSerializer(genTypes, "intel", genTypes.GetObject<Intel.CtorInfos>(TypeIds.IntelCtorInfos).Infos, genTypes[TypeIds.IntelCtorKind])));
			if (genTypes.Options.HasMasmFormatter)
				serializers.Add(new FmtSerializer(new CppFormatterTableSerializer(genTypes, "masm", genTypes.GetObject<Masm.CtorInfos>(TypeIds.MasmCtorInfos).Infos, genTypes[TypeIds.MasmCtorKind])));
			if (genTypes.Options.HasNasmFormatter)
				serializers.Add(new FmtSerializer(new CppFormatterTableSerializer(genTypes, "nasm", genTypes.GetObject<Nasm.CtorInfos>(TypeIds.NasmCtorInfos).Infos, genTypes[TypeIds.NasmCtorKind])));
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
