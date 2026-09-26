// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using Generator.Constants;
using Generator.Enums;
using Generator.IO;

namespace Generator.Documentation.Cpp {
	sealed class CppDeprecatedWriter : DeprecatedWriter {
		readonly IdentifierConverter idConverter;

		public CppDeprecatedWriter(IdentifierConverter idConverter) =>
			this.idConverter = idConverter;

		public string? GetAttribute(EnumValue value) {
			if (!value.DeprecatedInfo.IsDeprecated)
				return null;
			string? newName = value.DeprecatedInfo.NewName is string n ? value.DeclaringType[n].Name(idConverter) : null;
			return GetAttribute(newName, value.DeprecatedInfo.Description);
		}

		public string? GetAttribute(Constant value) {
			if (!value.DeprecatedInfo.IsDeprecated)
				return null;
			string? newName = value.DeprecatedInfo.NewName is string n ? value.DeclaringType[n].Name(idConverter) : null;
			return GetAttribute(newName, value.DeprecatedInfo.Description);
		}

		static string GetAttribute(string? newName, string? description) {
			string extra;
			if (description is not null)
				extra = description;
			else if (newName is not null)
				extra = $"Use {newName} instead";
			else
				extra = "Don't use it!";
			return $"[[deprecated(\"{CppConstants.EscapeString(extra)}\")]]";
		}

		public override void WriteDeprecated(FileWriter writer, EnumValue value) {
			if (GetAttribute(value) is string attr)
				writer.WriteLine(attr);
		}

		public override void WriteDeprecated(FileWriter writer, Constant value) {
			if (GetAttribute(value) is string attr)
				writer.WriteLine(attr);
		}
	}
}
