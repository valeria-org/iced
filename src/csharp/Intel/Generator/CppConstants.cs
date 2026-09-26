// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System.IO;
using System.Linq;
using Generator.IO;

namespace Generator {
	static class CppConstants {
		public const string Namespace = "iced_x86";
		public const string InternalNamespace = "iced_x86::internal";
		public const string TestsNamespace = "iced_x86::tests";

		/// <summary>Public header, eg. <c>include/iced_x86/code.hpp</c></summary>
		public static string GetIncludeFilename(GenTypes genTypes, params string[] names) =>
			Path.Combine(new[] { genTypes.Dirs.CppDir, "include", "iced_x86" }.Concat(names).ToArray());

		/// <summary>Source file, eg. <c>src/code.cpp</c></summary>
		public static string GetSrcFilename(GenTypes genTypes, params string[] names) =>
			Path.Combine(new[] { genTypes.Dirs.CppDir, "src" }.Concat(names).ToArray());

		/// <summary>Internal header, eg. <c>src/internal/decoder/op_size.hpp</c></summary>
		public static string GetInternalFilename(GenTypes genTypes, params string[] names) =>
			Path.Combine(new[] { genTypes.Dirs.CppDir, "src", "internal" }.Concat(names).ToArray());

		/// <summary>Test file, eg. <c>tests/generated/decoder_test_options.hpp</c></summary>
		public static string GetTestFilename(GenTypes genTypes, params string[] names) =>
			Path.Combine(new[] { genTypes.Dirs.CppDir, "tests" }.Concat(names).ToArray());

		/// <summary>Writes the license header + <c>#pragma once</c></summary>
		public static void WriteHeaderFileHeader(FileWriter writer) {
			writer.WriteFileHeader();
			writer.WriteLine("#pragma once");
			writer.WriteLine();
		}

		public static void WriteNamespaceBegin(FileWriter writer, string ns) {
			writer.WriteLine($"namespace {ns} {{");
			writer.WriteLine();
		}

		public static void WriteNamespaceEnd(FileWriter writer, string ns) {
			writer.WriteLine();
			writer.WriteLine($"}} // namespace {ns}");
		}

		/// <summary>
		/// Gets an enum value, eg. <c>Register::EAX</c>. Deprecated values (they have a <c>[[deprecated]]</c> attribute) are
		/// written as <c>static_cast&lt;Register&gt;(249)</c> to prevent deprecation warnings.
		/// </summary>
		public static string ToEnumValue(IdentifierConverter idConverter, Enums.EnumValue value) {
			var enumType = value.DeclaringType;
			if (value.DeprecatedInfo.IsDeprecated)
				return $"static_cast<{enumType.Name(idConverter)}>({value.Value})";
			return idConverter.ToDeclTypeAndValue(value);
		}

		public static string EscapeString(string s) =>
			s.Replace("\\", "\\\\").Replace("\"", "\\\"");
	}
}
