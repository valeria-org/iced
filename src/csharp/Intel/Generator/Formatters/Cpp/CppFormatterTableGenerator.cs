// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Generates the instruction tables of all formatters (gas, intel, masm, nasm, fast):
	/// <list type="bullet">
	/// <item><c>src/formatter/&lt;syntax&gt;/fmt_data.cpp</c> + <c>src/internal/formatter/&lt;syntax&gt;/fmt_data.hpp</c>: the constant gas/intel/masm/nasm
	/// instruction info tables (see <see cref="CppInstrInfoTableGen"/>)</item>
	/// <item><c>src/formatter/fast/fmt_data.cpp</c> + <c>src/internal/formatter/fast/fmt_data.hpp</c>: the constant fast formatter
	/// mnemonic and flags tables (see <see cref="CppFastFmtTableGen"/>)</item>
	/// </list>
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppFormatterTableGenerator {
		readonly GenTypes genTypes;

		public CppFormatterTableGenerator(GeneratorContext generatorContext) =>
			genTypes = generatorContext.Types;

		public void Generate() {
			if (genTypes.Options.HasGasFormatter)
				new CppGasInstrInfoTableGen(genTypes).Generate();
			if (genTypes.Options.HasIntelFormatter)
				new CppIntelInstrInfoTableGen(genTypes).Generate();
			if (genTypes.Options.HasMasmFormatter)
				new CppMasmInstrInfoTableGen(genTypes).Generate();
			if (genTypes.Options.HasNasmFormatter)
				new CppNasmInstrInfoTableGen(genTypes).Generate();
			if (genTypes.Options.HasFastFormatter)
				new CppFastFmtTableGen(genTypes, genTypes.GetObject<Fast.FmtTblInfos>(TypeIds.FastFmtTblInfos).Infos).Generate();
		}
	}
}
