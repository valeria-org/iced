// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using Generator.IO;

namespace Generator.Tables.Cpp {
	/// <summary>
	/// Generates <c>src/internal/tuple_type_tbl.hpp</c> (Rust: <c>tuple_type_tbl.rs</c>), used by the decoder and encoder
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppTupleTypeTableGenerator {
		readonly IdentifierConverter idConverter;
		readonly GeneratorContext generatorContext;

		public CppTupleTypeTableGenerator(GeneratorContext generatorContext) {
			idConverter = CppIdentifierConverter.Create();
			this.generatorContext = generatorContext;
		}

		public void Generate() {
			var genTypes = generatorContext.Types;
			var infos = genTypes.GetObject<TupleTypeTable>(TypeIds.TupleTypeTable).Data;
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(CppConstants.GetInternalFilename(genTypes, "tuple_type_tbl.hpp")))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include <cstddef>");
				writer.WriteLine("#include <cstdint>");
				writer.WriteLine("#include \"iced_x86/tuple_type.hpp\"");
				writer.WriteLine("#include \"internal/iced_assert.hpp\"");
				writer.WriteLine();
				CppConstants.WriteNamespaceBegin(writer, CppConstants.InternalNamespace);
				writer.WriteLine("// { N, Nbcst }");
				writer.WriteLine($"inline constexpr std::uint8_t TUPLE_TYPE_TBL[{infos.Length}][2] = {{");
				using (writer.Indent()) {
					for (int i = 0; i < infos.Length; i++) {
						var info = infos[i];
						if (info.Value.Value != (uint)i)
							throw new InvalidOperationException();
						if (info.N > byte.MaxValue)
							throw new InvalidOperationException();
						if (info.Nbcst > byte.MaxValue)
							throw new InvalidOperationException();
						writer.WriteLine($"{{ 0x{info.N:X2}, 0x{info.Nbcst:X2} }},// {idConverter.ToDeclTypeAndValue(info.Value)}");
					}
				}
				writer.WriteLine("};");
				writer.WriteLine();
				writer.WriteLine("ICED_FORCE_INLINE std::uint32_t get_disp8n(TupleType tuple_type, bool bcst) noexcept {");
				using (writer.Indent())
					writer.WriteLine("return TUPLE_TYPE_TBL[static_cast<std::size_t>(tuple_type)][bcst ? 1 : 0];");
				writer.WriteLine("}");
				CppConstants.WriteNamespaceEnd(writer, CppConstants.InternalNamespace);
			}
		}
	}
}
