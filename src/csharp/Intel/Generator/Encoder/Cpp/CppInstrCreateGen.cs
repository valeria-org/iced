// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Linq;
using System.Text;
using Generator.Documentation.Cpp;
using Generator.Enums;
using Generator.Enums.Encoder;
using Generator.IO;

namespace Generator.Encoder.Cpp {
	/// <summary>
	/// Generates the <c>Instruction::with*()</c> methods: the declarations are written to the <c>Create</c> region in
	/// <c>include/iced_x86/instruction.hpp</c> (inside class <c>Instruction</c>) and the definitions are written to the
	/// <c>Create</c> region in <c>src/encoder/instruction_create.cpp</c>.
	/// Rust's generic <c>with1..with5&lt;T, U, ..&gt;()</c> methods are overloads taking <c>Register</c>, <c>int32_t</c>,
	/// <c>uint32_t</c>, <c>int64_t</c>, <c>uint64_t</c> or <c>MemoryOperand</c>.
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppInstrCreateGenerator {
		readonly GeneratorContext generatorContext;

		public CppInstrCreateGenerator(GeneratorContext generatorContext) => this.generatorContext = generatorContext;

		public void Generate() {
			new CppInstrCreateGen(generatorContext, declarations: true).Generate();
			new CppInstrCreateGen(generatorContext, declarations: false).Generate();
		}
	}

	sealed class CppInstrCreateGen : InstrCreateGen {
		readonly GeneratorContext generatorContext;
		readonly IdentifierConverter idConverter;
		readonly CppDocCommentWriter docWriter;
		readonly bool declarations;
		readonly StringBuilder sb;

		public CppInstrCreateGen(GeneratorContext generatorContext, bool declarations)
			: base(generatorContext.Types) {
			this.generatorContext = generatorContext;
			idConverter = CppIdentifierConverter.Create();
			docWriter = new CppDocCommentWriter(idConverter);
			this.declarations = declarations;
			sb = new StringBuilder();
		}

		protected override (TargetLanguage language, string id, string filename) GetFileInfo() {
			if (declarations)
				return (TargetLanguage.Cpp, "Create", CppConstants.GetIncludeFilename(genTypes, "instruction.hpp"));
			return (TargetLanguage.Cpp, "Create", CppConstants.GetSrcFilename(genTypes, "encoder", "instruction_create.cpp"));
		}

		protected override void Generate(FileWriter writer) {
			GenCreateMethods(writer, 0);
			GenTheRest(writer);
		}

		string GetArgTypeString(MethodArg arg) =>
			arg.Type switch {
				MethodArgType.Code => genTypes[TypeIds.Code].Name(idConverter),
				MethodArgType.Register => genTypes[TypeIds.Register].Name(idConverter),
				MethodArgType.RepPrefixKind => genTypes[TypeIds.RepPrefixKind].Name(idConverter),
				MethodArgType.Memory => "const MemoryOperand&",
				MethodArgType.UInt8 => "std::uint8_t",
				MethodArgType.UInt16 => "std::uint16_t",
				MethodArgType.Int32 => "std::int32_t",
				MethodArgType.PreferredInt32 or MethodArgType.UInt32 => "std::uint32_t",
				MethodArgType.Int64 => "std::int64_t",
				MethodArgType.UInt64 => "std::uint64_t",
				_ => throw new InvalidOperationException(),
			};

		static string? GetSliceElemType(MethodArgType type) =>
			type switch {
				MethodArgType.ByteSlice => "std::uint8_t",
				MethodArgType.WordSlice => "std::uint16_t",
				MethodArgType.DwordSlice => "std::uint32_t",
				MethodArgType.QwordSlice => "std::uint64_t",
				_ => null,
			};

		string GetDefaultValue(object? value) =>
			value switch {
				EnumValue enumValue => idConverter.ToDeclTypeAndValue(enumValue),
				_ => throw new InvalidOperationException(),
			};

		string ArgName(MethodArg arg) => idConverter.Argument(arg.Name);

		void WriteDocs(FileWriter writer, CreateMethod method, string? errorMsg, bool isSlice) {
			const string typeName = "Instruction";
			docWriter.BeginWrite(writer);
			foreach (var doc in method.Docs)
				docWriter.WriteDocLine(writer, doc, typeName);
			docWriter.WriteLine(writer, string.Empty);
			if (errorMsg is not null) {
				docWriter.WriteLine(writer, "# Errors");
				docWriter.WriteLine(writer, string.Empty);
				docWriter.WriteLine(writer, errorMsg);
				docWriter.WriteLine(writer, string.Empty);
			}
			docWriter.WriteLine(writer, "# Arguments");
			docWriter.WriteLine(writer, string.Empty);
			for (int i = 0; i < method.Args.Count; i++) {
				var arg = method.Args[i];
				docWriter.Write($"* `{ArgName(arg)}`: ");
				docWriter.WriteDocLine(writer, arg.Doc, typeName);
			}
			if (isSlice)
				docWriter.WriteLine(writer, "* `size`: Number of elements in `data`");
			docWriter.EndWrite(writer);
		}

		// Writes the method declaration (and docs) or the start of the definition. Returns true if the caller should write the body
		bool WriteMethod(FileWriter writer, CreateMethod method, string name, bool returnsResult, string? errorMsg) {
			var isSlice = method.Args.Count == 1 && GetSliceElemType(method.Args[0].Type) is not null;
			if (declarations)
				WriteDocs(writer, method, errorMsg, isSlice);
			sb.Clear();
			if (declarations)
				sb.Append("static ");
			sb.Append(returnsResult ? "Result<Instruction>" : "Instruction");
			sb.Append(' ');
			if (!declarations)
				sb.Append("Instruction::");
			sb.Append(name);
			sb.Append('(');
			for (int i = 0; i < method.Args.Count; i++) {
				var arg = method.Args[i];
				if (i > 0)
					sb.Append(", ");
				if (GetSliceElemType(arg.Type) is string elemType) {
					sb.Append($"const {elemType}* {ArgName(arg)}, std::size_t size");
					continue;
				}
				sb.Append(GetArgTypeString(arg));
				sb.Append(' ');
				sb.Append(ArgName(arg));
				if (declarations && arg.DefaultValue is not null) {
					sb.Append(" = ");
					sb.Append(GetDefaultValue(arg.DefaultValue));
				}
			}
			sb.Append(')');
			if (declarations) {
				sb.Append(';');
				writer.WriteLine(sb.ToString());
				return false;
			}
			sb.Append(" {");
			writer.WriteLine(sb.ToString());
			return true;
		}

		void WriteMethodEnd(FileWriter writer) => writer.WriteLine("}");

		void WriteInitializeInstruction(FileWriter writer, string codeExpr) {
			writer.WriteLine("Instruction instruction;");
			writer.WriteLine($"instruction.set_code({codeExpr});");
		}

		static void WriteMethodFooter(FileWriter writer, int opCount) {
			writer.WriteLine();
			writer.WriteLine($"ICED_DEBUG_ASSERT(instruction.op_count() == {opCount});");
			writer.WriteLine("return instruction;");
		}

		protected override void GenCreate(FileWriter writer, CreateMethod method, InstructionGroup group, int id) {
			if (id != 0)
				throw new InvalidOperationException();
			int opCount = method.Args.Count - 1;
			bool returnsResult = opCount != 0;
			var name = opCount == 0 ? "with" : "with" + opCount.ToString();
			var errorMsg = returnsResult ? "Fails if one of the operands is invalid (basic checks)" : null;
			if (!WriteMethod(writer, method, name, returnsResult, errorMsg))
				return;
			using (writer.Indent()) {
				var args = method.Args;
				if (args.Count == 0 || args[0].Type != MethodArgType.Code)
					throw new InvalidOperationException();
				WriteInitializeInstruction(writer, ArgName(args[0]));
				for (int i = 1; i < args.Count; i++) {
					int op = i - 1;
					var arg = args[i];
					writer.WriteLine();
					switch (arg.Type) {
					case MethodArgType.Register:
						writer.WriteLine($"// OpKind::Register == 0 so set_op{op}_kind() isn't needed");
						writer.WriteLine($"instruction.set_op{op}_register({ArgName(arg)});");
						break;

					case MethodArgType.Memory:
						writer.WriteLine($"instruction.set_op{op}_kind({idConverter.ToDeclTypeAndValue(genTypes[TypeIds.OpKind][nameof(OpKind.Memory)])});");
						writer.WriteLine($"init_memory_operand(instruction, {ArgName(arg)});");
						break;

					case MethodArgType.Int32:
						writer.WriteLine($"ICED_TRY(InstructionInternal::initialize_signed_immediate(instruction, {op}, static_cast<std::int64_t>({ArgName(arg)})));");
						break;

					case MethodArgType.UInt32:
						writer.WriteLine($"ICED_TRY(InstructionInternal::initialize_unsigned_immediate(instruction, {op}, static_cast<std::uint64_t>({ArgName(arg)})));");
						break;

					case MethodArgType.Int64:
						writer.WriteLine($"ICED_TRY(InstructionInternal::initialize_signed_immediate(instruction, {op}, {ArgName(arg)}));");
						break;

					case MethodArgType.UInt64:
						writer.WriteLine($"ICED_TRY(InstructionInternal::initialize_unsigned_immediate(instruction, {op}, {ArgName(arg)}));");
						break;

					default:
						throw new InvalidOperationException();
					}
				}
				WriteMethodFooter(writer, opCount);
			}
			WriteMethodEnd(writer);
		}

		protected override void GenCreateBranch(FileWriter writer, CreateMethod method) {
			if (method.Args.Count != 2)
				throw new InvalidOperationException();
			if (!WriteMethod(writer, method, "with_branch", true, "Fails if the created instruction doesn't have a near branch operand"))
				return;
			using (writer.Indent()) {
				WriteInitializeInstruction(writer, ArgName(method.Args[0]));
				writer.WriteLine();
				writer.WriteLine($"auto op_kind = InstructionInternal::get_near_branch_op_kind({ArgName(method.Args[0])}, 0);");
				writer.WriteLine("if (op_kind.is_err())");
				using (writer.Indent())
					writer.WriteLine("return op_kind.error();");
				writer.WriteLine("instruction.set_op0_kind(op_kind.value());");
				writer.WriteLine($"instruction.set_near_branch64({ArgName(method.Args[1])});");
				WriteMethodFooter(writer, 1);
			}
			WriteMethodEnd(writer);
		}

		protected override void GenCreateFarBranch(FileWriter writer, CreateMethod method) {
			if (method.Args.Count != 3)
				throw new InvalidOperationException();
			if (!WriteMethod(writer, method, "with_far_branch", true, "Fails if the created instruction doesn't have a far branch operand"))
				return;
			using (writer.Indent()) {
				WriteInitializeInstruction(writer, ArgName(method.Args[0]));
				writer.WriteLine();
				writer.WriteLine($"auto op_kind = InstructionInternal::get_far_branch_op_kind({ArgName(method.Args[0])}, 0);");
				writer.WriteLine("if (op_kind.is_err())");
				using (writer.Indent())
					writer.WriteLine("return op_kind.error();");
				writer.WriteLine("instruction.set_op0_kind(op_kind.value());");
				writer.WriteLine($"instruction.set_far_branch_selector({ArgName(method.Args[1])});");
				writer.WriteLine($"instruction.set_far_branch32({ArgName(method.Args[2])});");
				WriteMethodFooter(writer, 1);
			}
			WriteMethodEnd(writer);
		}

		string GetAddrSizeOrBitnessError(CreateMethod method) {
			var arg = method.Args[0];
			if (arg.Name != "addressSize" && arg.Name != "bitness")
				throw new InvalidOperationException();
			return $"Fails if `{ArgName(arg)}` is not one of 16, 32, 64.";
		}

		protected override void GenCreateXbegin(FileWriter writer, CreateMethod method) {
			if (method.Args.Count != 2)
				throw new InvalidOperationException();
			if (!WriteMethod(writer, method, "with_xbegin", true, GetAddrSizeOrBitnessError(method)))
				return;
			var bitness = ArgName(method.Args[0]);
			var target = ArgName(method.Args[1]);
			var opKind = genTypes[TypeIds.OpKind];
			using (writer.Indent()) {
				writer.WriteLine("Instruction instruction;");
				writer.WriteLine();
				writer.WriteLine($"switch ({bitness}) {{");
				writer.WriteLine("case 16:");
				using (writer.Indent()) {
					writer.WriteLine($"instruction.set_code({idConverter.ToDeclTypeAndValue(codeType[nameof(Code.Xbegin_rel16)])});");
					writer.WriteLine($"instruction.set_op0_kind({idConverter.ToDeclTypeAndValue(opKind[nameof(OpKind.NearBranch32)])});");
					writer.WriteLine($"instruction.set_near_branch32(static_cast<std::uint32_t>({target}));");
					writer.WriteLine("break;");
				}
				writer.WriteLine();
				writer.WriteLine("case 32:");
				using (writer.Indent()) {
					writer.WriteLine($"instruction.set_code({idConverter.ToDeclTypeAndValue(codeType[nameof(Code.Xbegin_rel32)])});");
					writer.WriteLine($"instruction.set_op0_kind({idConverter.ToDeclTypeAndValue(opKind[nameof(OpKind.NearBranch32)])});");
					writer.WriteLine($"instruction.set_near_branch32(static_cast<std::uint32_t>({target}));");
					writer.WriteLine("break;");
				}
				writer.WriteLine();
				writer.WriteLine("case 64:");
				using (writer.Indent()) {
					writer.WriteLine($"instruction.set_code({idConverter.ToDeclTypeAndValue(codeType[nameof(Code.Xbegin_rel32)])});");
					writer.WriteLine($"instruction.set_op0_kind({idConverter.ToDeclTypeAndValue(opKind[nameof(OpKind.NearBranch64)])});");
					writer.WriteLine($"instruction.set_near_branch64({target});");
					writer.WriteLine("break;");
				}
				writer.WriteLine();
				writer.WriteLine("default:");
				using (writer.Indent())
					writer.WriteLine("return IcedError(\"Invalid bitness\");");
				writer.WriteLine("}");
				WriteMethodFooter(writer, 1);
			}
			WriteMethodEnd(writer);
		}

		string Enum(EnumValue value) => idConverter.ToDeclTypeAndValue(value);
		EnumValue RegNone => genTypes[TypeIds.Register][nameof(Register.None)];
		EnumValue Rep(RepPrefixKind kind) => genTypes[TypeIds.RepPrefixKind][kind.ToString()];

		void GenCreateStringCall(FileWriter writer, CreateMethod method, string methodBaseName, string helperName, string[] helperArgs) {
			var methodName = idConverter.Method("With" + methodBaseName);
			if (!WriteMethod(writer, method, methodName, true, GetAddrSizeOrBitnessError(method)))
				return;
			using (writer.Indent())
				writer.WriteLine($"return InstructionInternal::{helperName}({string.Join(", ", helperArgs)});");
			WriteMethodEnd(writer);
		}

		protected override void GenCreateString_Reg_SegRSI(FileWriter writer, CreateMethod method, StringMethodKind kind, string methodBaseName, EnumValue code, EnumValue register) {
			var args = kind switch {
				StringMethodKind.Full when method.Args.Count == 3 => new[] { Enum(code), ArgName(method.Args[0]), Enum(register), ArgName(method.Args[1]), ArgName(method.Args[2]) },
				StringMethodKind.Rep when method.Args.Count == 1 => new[] { Enum(code), ArgName(method.Args[0]), Enum(register), Enum(RegNone), Enum(Rep(RepPrefixKind.Repe)) },
				_ => throw new InvalidOperationException(),
			};
			GenCreateStringCall(writer, method, methodBaseName, "with_string_reg_segrsi", args);
		}

		protected override void GenCreateString_Reg_ESRDI(FileWriter writer, CreateMethod method, StringMethodKind kind, string methodBaseName, EnumValue code, EnumValue register) {
			var args = kind switch {
				StringMethodKind.Full when method.Args.Count == 2 => new[] { Enum(code), ArgName(method.Args[0]), Enum(register), ArgName(method.Args[1]) },
				StringMethodKind.Repe when method.Args.Count == 1 => new[] { Enum(code), ArgName(method.Args[0]), Enum(register), Enum(Rep(RepPrefixKind.Repe)) },
				StringMethodKind.Repne when method.Args.Count == 1 => new[] { Enum(code), ArgName(method.Args[0]), Enum(register), Enum(Rep(RepPrefixKind.Repne)) },
				_ => throw new InvalidOperationException(),
			};
			GenCreateStringCall(writer, method, methodBaseName, "with_string_reg_esrdi", args);
		}

		protected override void GenCreateString_ESRDI_Reg(FileWriter writer, CreateMethod method, StringMethodKind kind, string methodBaseName, EnumValue code, EnumValue register) {
			var args = kind switch {
				StringMethodKind.Full when method.Args.Count == 2 => new[] { Enum(code), ArgName(method.Args[0]), Enum(register), ArgName(method.Args[1]) },
				StringMethodKind.Rep when method.Args.Count == 1 => new[] { Enum(code), ArgName(method.Args[0]), Enum(register), Enum(Rep(RepPrefixKind.Repe)) },
				_ => throw new InvalidOperationException(),
			};
			GenCreateStringCall(writer, method, methodBaseName, "with_string_esrdi_reg", args);
		}

		protected override void GenCreateString_SegRSI_ESRDI(FileWriter writer, CreateMethod method, StringMethodKind kind, string methodBaseName, EnumValue code) {
			var args = kind switch {
				StringMethodKind.Full when method.Args.Count == 3 => new[] { Enum(code), ArgName(method.Args[0]), ArgName(method.Args[1]), ArgName(method.Args[2]) },
				StringMethodKind.Repe when method.Args.Count == 1 => new[] { Enum(code), ArgName(method.Args[0]), Enum(RegNone), Enum(Rep(RepPrefixKind.Repe)) },
				StringMethodKind.Repne when method.Args.Count == 1 => new[] { Enum(code), ArgName(method.Args[0]), Enum(RegNone), Enum(Rep(RepPrefixKind.Repne)) },
				_ => throw new InvalidOperationException(),
			};
			GenCreateStringCall(writer, method, methodBaseName, "with_string_segrsi_esrdi", args);
		}

		protected override void GenCreateString_ESRDI_SegRSI(FileWriter writer, CreateMethod method, StringMethodKind kind, string methodBaseName, EnumValue code) {
			var args = kind switch {
				StringMethodKind.Full when method.Args.Count == 3 => new[] { Enum(code), ArgName(method.Args[0]), ArgName(method.Args[1]), ArgName(method.Args[2]) },
				StringMethodKind.Rep when method.Args.Count == 1 => new[] { Enum(code), ArgName(method.Args[0]), Enum(RegNone), Enum(Rep(RepPrefixKind.Repe)) },
				_ => throw new InvalidOperationException(),
			};
			GenCreateStringCall(writer, method, methodBaseName, "with_string_esrdi_segrsi", args);
		}

		protected override void GenCreateMaskmov(FileWriter writer, CreateMethod method, string methodBaseName, EnumValue code) {
			if (method.Args.Count != 4)
				throw new InvalidOperationException();
			var args = new[] { Enum(code), ArgName(method.Args[0]), ArgName(method.Args[1]), ArgName(method.Args[2]), ArgName(method.Args[3]) };
			GenCreateStringCall(writer, method, methodBaseName, "with_maskmov", args);
		}

		static (EnumValue code, string setValueName, string methodName) GetDeclareDataInfo(EnumType codeType, DeclareDataKind kind) =>
			kind switch {
				DeclareDataKind.Byte => (codeType[nameof(Code.DeclareByte)], "set_declare_byte_value", "with_declare_byte"),
				DeclareDataKind.Word => (codeType[nameof(Code.DeclareWord)], "set_declare_word_value", "with_declare_word"),
				DeclareDataKind.Dword => (codeType[nameof(Code.DeclareDword)], "set_declare_dword_value", "with_declare_dword"),
				DeclareDataKind.Qword => (codeType[nameof(Code.DeclareQword)], "set_declare_qword_value", "with_declare_qword"),
				_ => throw new InvalidOperationException(),
			};

		protected override void GenCreateDeclareData(FileWriter writer, CreateMethod method, DeclareDataKind kind) {
			var (code, setValueName, methodName) = GetDeclareDataInfo(codeType, kind);
			methodName = methodName + "_" + method.Args.Count.ToString();
			WriteItemSeparator(writer);
			if (WriteMethod(writer, method, methodName, false, null)) {
				using (writer.Indent()) {
					WriteInitializeInstruction(writer, Enum(code));
					writer.WriteLine($"InstructionInternal::internal_set_declare_data_len(instruction, {method.Args.Count});");
					writer.WriteLine();
					for (int i = 0; i < method.Args.Count; i++)
						writer.WriteLine($"instruction.{setValueName}({i}, {ArgName(method.Args[i])});");
					WriteMethodFooter(writer, 0);
				}
				WriteMethodEnd(writer);
			}

			// Rust also has a (hidden) try_ method that returns a Result
			WriteItemSeparator(writer);
			var tryMethod = method.Copy();
			tryMethod.Docs.Clear();
			tryMethod.Docs.Add($"Same as `{methodName}()` but returns a `Result<Instruction>` (it never fails)");
			if (WriteMethod(writer, tryMethod, "try_" + methodName, true, null)) {
				using (writer.Indent())
					writer.WriteLine($"return {methodName}({string.Join(", ", method.Args.Select(a => ArgName(a)))});");
				WriteMethodEnd(writer);
			}
		}

		// Declarations only: std::initializer_list, std::vector and C array overloads of a slice (ptr + size) method
		void WriteSliceOverloads(FileWriter writer, CreateMethod method, string methodName) {
			if (!declarations)
				return;
			var elemType = GetSliceElemType(method.Args[0].Type) ?? throw new InvalidOperationException();
			var dataName = ArgName(method.Args[0]);
			writer.WriteLine();
			writer.WriteLine($"/// Same as `{methodName}(data, size)`");
			writer.WriteLine($"static Result<Instruction> {methodName}(std::initializer_list<{elemType}> {dataName}) {{ return {methodName}({dataName}.begin(), {dataName}.size()); }}");
			writer.WriteLine();
			writer.WriteLine($"/// Same as `{methodName}(data, size)`");
			writer.WriteLine($"static Result<Instruction> {methodName}(const std::vector<{elemType}>& {dataName}) {{ return {methodName}({dataName}.data(), {dataName}.size()); }}");
			writer.WriteLine();
			writer.WriteLine($"/// Same as `{methodName}(data, size)`");
			writer.WriteLine("template <std::size_t N>");
			writer.WriteLine($"static Result<Instruction> {methodName}(const {elemType} (&{dataName})[N]) {{ return {methodName}({dataName}, N); }}");
		}

		void GenCreateDeclareDataSlice(FileWriter writer, CreateMethod method, int elemSize, EnumValue code, string methodName, string setDeclValueName) {
			WriteItemSeparator(writer);
			var dataName = ArgName(method.Args[0]);
			if (!WriteMethod(writer, method, methodName, true, $"Fails if `size` is not 1-{16 / elemSize}")) {
				WriteSliceOverloads(writer, method, methodName);
				return;
			}
			using (writer.Indent()) {
				writer.WriteLine($"if (size - 1 > {16 / elemSize} - 1)");
				using (writer.Indent())
					writer.WriteLine("return IcedError(\"Invalid slice length\");");
				writer.WriteLine();
				WriteInitializeInstruction(writer, Enum(code));
				writer.WriteLine("InstructionInternal::internal_set_declare_data_len(instruction, static_cast<std::uint32_t>(size));");
				writer.WriteLine();
				writer.WriteLine("for (std::size_t i = 0; i < size; i++)");
				using (writer.Indent())
					writer.WriteLine($"instruction.{setDeclValueName}(i, {dataName}[i]);");
				WriteMethodFooter(writer, 0);
			}
			WriteMethodEnd(writer);
		}

		void GenCreateDeclareDataSliceU8(FileWriter writer, CreateMethod method, int elemSize, EnumValue code, string methodName, string setDeclValueName, string elemType) {
			WriteItemSeparator(writer);
			var dataName = ArgName(method.Args[0]);
			if (!WriteMethod(writer, method, methodName, true, $"Fails if `size` is not {elemSize}-16 or not a multiple of {elemSize}")) {
				WriteSliceOverloads(writer, method, methodName);
				return;
			}
			using (writer.Indent()) {
				writer.WriteLine($"if (size - 1 > 16 - 1 || (size & {elemSize - 1}) != 0)");
				using (writer.Indent())
					writer.WriteLine("return IcedError(\"Invalid slice length\");");
				writer.WriteLine();
				WriteInitializeInstruction(writer, Enum(code));
				writer.WriteLine($"InstructionInternal::internal_set_declare_data_len(instruction, static_cast<std::uint32_t>(size / {elemSize}));");
				writer.WriteLine();
				writer.WriteLine($"for (std::size_t i = 0; i < size / {elemSize}; i++) {{");
				using (writer.Indent()) {
					var parts = new string[elemSize];
					for (int j = 0; j < elemSize; j++)
						parts[j] = j == 0 ? $"static_cast<{elemType}>({dataName}[i * {elemSize}])" : $"(static_cast<{elemType}>({dataName}[i * {elemSize} + {j}]) << {j * 8})";
					writer.WriteLine($"const {elemType} v = static_cast<{elemType}>({string.Join(" | ", parts)});");
					writer.WriteLine($"instruction.{setDeclValueName}(i, v);");
				}
				writer.WriteLine("}");
				WriteMethodFooter(writer, 0);
			}
			WriteMethodEnd(writer);
		}

		protected override void GenCreateDeclareDataArray(FileWriter writer, CreateMethod method, DeclareDataKind kind, ArrayType arrayType) {
			var (code, setValueName, methodName) = GetDeclareDataInfo(codeType, kind);
			switch (arrayType) {
			case ArrayType.BytePtr:
			case ArrayType.WordPtr:
			case ArrayType.DwordPtr:
			case ArrayType.QwordPtr:
			case ArrayType.ByteArray:
			case ArrayType.WordArray:
			case ArrayType.DwordArray:
			case ArrayType.QwordArray:
				// Same as Rust: only slices (ptr + size) are supported
				break;

			case ArrayType.ByteSlice:
				switch (kind) {
				case DeclareDataKind.Byte:
					GenCreateDeclareDataSlice(writer, method, 1, code, methodName, setValueName);
					break;
				case DeclareDataKind.Word:
					GenCreateDeclareDataSliceU8(writer, method, 2, code, methodName + "_slice_u8", setValueName, "std::uint16_t");
					break;
				case DeclareDataKind.Dword:
					GenCreateDeclareDataSliceU8(writer, method, 4, code, methodName + "_slice_u8", setValueName, "std::uint32_t");
					break;
				case DeclareDataKind.Qword:
					GenCreateDeclareDataSliceU8(writer, method, 8, code, methodName + "_slice_u8", setValueName, "std::uint64_t");
					break;
				default:
					throw new InvalidOperationException();
				}
				break;

			case ArrayType.WordSlice:
				if (kind != DeclareDataKind.Word)
					throw new InvalidOperationException();
				GenCreateDeclareDataSlice(writer, method, 2, code, methodName, setValueName);
				break;

			case ArrayType.DwordSlice:
				if (kind != DeclareDataKind.Dword)
					throw new InvalidOperationException();
				GenCreateDeclareDataSlice(writer, method, 4, code, methodName, setValueName);
				break;

			case ArrayType.QwordSlice:
				if (kind != DeclareDataKind.Qword)
					throw new InvalidOperationException();
				GenCreateDeclareDataSlice(writer, method, 8, code, methodName, setValueName);
				break;

			default:
				throw new InvalidOperationException();
			}
		}

		protected override void GenCreateDeclareDataArrayLength(FileWriter writer, CreateMethod method, DeclareDataKind kind, ArrayType arrayType) {
			// Same as Rust: only slices (ptr + size) are supported
		}
	}
}
