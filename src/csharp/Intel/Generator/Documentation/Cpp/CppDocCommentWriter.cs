// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;

namespace Generator.Documentation.Cpp {
	sealed class CppDocCommentWriter : MarkdownDocCommentWriter {
		static readonly Dictionary<string, (string type, bool isKeyword)> toTypeInfo = new(StringComparer.Ordinal) {
			{ "bcd", ("bcd", false) },
			{ "bf16", ("bfloat16", false) },
			{ "f16", ("f16", false) },
			{ "f32", ("float", true) },
			{ "f64", ("double", true) },
			{ "f80", ("f80", false) },
			{ "f128", ("f128", false) },
			{ "i8", ("int8_t", true) },
			{ "i16", ("int16_t", true) },
			{ "i32", ("int32_t", true) },
			{ "i64", ("int64_t", true) },
			{ "i128", ("i128", false) },
			{ "i256", ("i256", false) },
			{ "i512", ("i512", false) },
			{ "u8", ("uint8_t", true) },
			{ "u16", ("uint16_t", true) },
			{ "u32", ("uint32_t", true) },
			{ "u52", ("u52", false) },
			{ "u64", ("uint64_t", true) },
			{ "u128", ("u128", false) },
			{ "u256", ("u256", false) },
			{ "u512", ("u512", false) },
		};

		public CppDocCommentWriter(IdentifierConverter idConverter)
			: base(idConverter, "::", "::", "::", "::", "///", "/// ", false, toTypeInfo) { }

		// A backslash at the end of a line comment would continue the comment on the next line
		protected override string FixLine(string line) => line.EndsWith('\\') ? line + " " : line;
	}
}
