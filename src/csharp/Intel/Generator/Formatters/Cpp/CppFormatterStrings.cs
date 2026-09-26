// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Text;
using Generator.IO;

namespace Generator.Formatters.Cpp {
	/// <summary>
	/// Constant string data used by the C++ formatters (C++ <c>FormatterString</c>). Each string is stored as a length byte
	/// followed by the lowercase chars followed by the uppercase chars (no NUL terminator), eg. <c>"\x03" "add" "ADD"</c>.
	/// Each unique string is only stored once. A string is referenced by its offset in the data.
	/// </summary>
	sealed class CppFormatterStrings {
		// Max size of the data. Offsets are u16 values and MSVC doesn't support string literals > 64KB (incl. the NUL char)
		const int MaxSize = 0xFFFE;

		readonly Dictionary<string, int> toOffset = new(StringComparer.Ordinal);
		readonly List<string> strings = new();
		int size;

		public int Size => size;
		public int Count => strings.Count;

		/// <summary>
		/// Adds a string (if it hasn't been added yet) and returns its offset
		/// </summary>
		public int Add(string s) {
			if (toOffset.TryGetValue(s, out var offset))
				return offset;
			if (s.Length > byte.MaxValue)
				throw new InvalidOperationException();
			foreach (var c in s) {
				if (c < 0x20 || c > 0x7E || (c >= 'A' && c <= 'Z'))
					throw new InvalidOperationException($"Invalid string: {s}");
			}
			offset = size;
			size += 1 + s.Length * 2;
			if (size > MaxSize)
				throw new InvalidOperationException("Too many strings");
			toOffset.Add(s, offset);
			strings.Add(s);
			return offset;
		}

		static string Escape(string s) {
			var sb = new StringBuilder(s.Length);
			foreach (var c in s) {
				switch (c) {
				case '\\': sb.Append(@"\\"); break;
				case '"': sb.Append("\\\""); break;
				// Prevent trigraphs
				case '?': sb.Append(@"\?"); break;
				default: sb.Append(c); break;
				}
			}
			return sb.ToString();
		}

		/// <summary>
		/// Writes the string data (<c>{declPrefix} {name}[] = "...";</c>). It's one string literal (the pieces are concatenated)
		/// </summary>
		public void Write(FileWriter writer, string declPrefix, string name) {
			writer.WriteLine("// clang-format off");
			writer.WriteLine($"{declPrefix} {name}[] =");
			using (writer.Indent()) {
				if (strings.Count == 0)
					writer.WriteLine("\"\"");
				int offset = 0;
				foreach (var s in strings) {
					writer.Write($"\"\\x{s.Length:X2}\" \"{Escape(s)}\" \"{Escape(s.ToUpperInvariant())}\"");
					writer.WriteCommentLine($"0x{offset:X4}");
					offset += 1 + s.Length * 2;
				}
				writer.WriteLine(";");
			}
			writer.WriteLine("// clang-format on");
		}
	}
}
