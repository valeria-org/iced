// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System.Collections.Generic;
using System.Linq;
using Generator.IO;
using Generator.Tables;

namespace Generator.InstructionInfo.Cpp {
	/// <summary>
	/// Generates the <c>Code</c> tables that are created by the instruction info generator but are needed by the C++ core (always compiled):
	/// <c>ignores_segment()</c>, <c>ignores_index()</c>, <c>is_tile_stride_index()</c> (<c>src/internal/code_internal.hpp</c>)
	/// and <c>code_ext::is_string_instruction()</c> (<c>include/iced_x86/code_ext.hpp</c>).
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppInstrInfoCoreTablesGenerator {
		readonly GenTypes genTypes;
		readonly IdentifierConverter idConverter;

		public CppInstrInfoCoreTablesGenerator(GeneratorContext generatorContext) {
			genTypes = generatorContext.Types;
			idConverter = CppIdentifierConverter.Create();
		}

		public void Generate() {
			var defs = genTypes.GetObject<InstructionDefs>(TypeIds.InstructionDefs).Defs;
			var codeInternal = CppConstants.GetInternalFilename(genTypes, "code_internal.hpp");
			GenerateCases(codeInternal, "IgnoresSegmentTable", defs.Where(a => (a.Flags1 & InstructionDefFlags1.IgnoresSegment) != 0));
			GenerateCases(codeInternal, "IgnoresIndexTable", defs.Where(a => (a.Flags3 & InstructionDefFlags3.IgnoresIndex) != 0));
			GenerateCases(codeInternal, "TileStrideIndexTable", defs.Where(a => (a.Flags3 & InstructionDefFlags3.TileStrideIndex) != 0));
			GenerateCases(CppConstants.GetIncludeFilename(genTypes, "code_ext.hpp"), "IsStringOpTable", defs.Where(a => (a.Flags3 & InstructionDefFlags3.IsStringOp) != 0));
		}

		void GenerateCases(string filename, string id, IEnumerable<InstructionDef> defs) {
			var sortedDefs = defs.OrderBy(a => a.Encoding).ThenBy(a => a.Code.Value).ToArray();
			new FileUpdater(TargetLanguage.Cpp, id, filename).Generate(writer => {
				foreach (var def in sortedDefs)
					writer.WriteLine($"case {CppConstants.ToEnumValue(idConverter, def.Code)}:");
			});
		}
	}
}
