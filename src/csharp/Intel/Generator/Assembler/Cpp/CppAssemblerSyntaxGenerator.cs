// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using Generator.Enums;
using Generator.Enums.Decoder;
using Generator.Enums.Encoder;
using Generator.IO;
using Generator.Tables;

namespace Generator.Assembler.Cpp {
	/// <summary>
	/// Generates the C++ code assembler (<c>iced_x86::code_asm</c>): register types and constants, memory operand size
	/// functions, all <c>CodeAssembler</c> instruction methods and the instruction tests.
	/// <para/>
	/// Rust uses one trait per mnemonic and generic methods. C++ uses overloaded member functions instead. They're
	/// declared in several classes (<c>code_assembler_fns.hpp</c>: <c>CodeAssemblerBase &lt;- CodeAssemblerFns0 &lt;- ... &lt;-
	/// CodeAssemblerFnsN &lt;- CodeAssembler</c>) and each class is implemented in <c>src/code_asm/fn_asm_impl_N.cpp</c>.
	/// </summary>
	[Generator(TargetLanguage.Cpp)]
	sealed class CppAssemblerSyntaxGenerator : AssemblerSyntaxGenerator {
		const string AsmRegisterPrefix = "AsmRegister";
		const string AsmMemoryOperand = "AsmMemoryOperand";
		const string AsmMemoryOperandArg = "AsmMemoryOperandArg";
		const string CodeLabel = "CodeLabel";
		const string CodeAssembler = "CodeAssembler";
		const string CreatedLabelName = "lbl";
		const string FirstLabelIdName = "FIRST_LABEL_ID";
		const string CodeAsmNamespace = CppConstants.Namespace + "::code_asm";
		const string TestsNamespace = CppConstants.TestsNamespace + "::code_asm_tests";
		// Number of generated .cpp files with the CodeAssembler instruction methods
		const int ImplFileCount = 10;
		// Max number of test cases per generated test file
		const int MaxTestsPerFile = 700;
		readonly IdentifierConverter idConverter;
		readonly EnumType registerType;
		readonly EnumType memoryOperandSizeType;

		public CppAssemblerSyntaxGenerator(GeneratorContext generatorContext)
			: base(generatorContext.Types) {
			idConverter = CppIdentifierConverter.Create();
			registerType = genTypes[TypeIds.Register];
			memoryOperandSizeType = genTypes[TypeIds.CodeAsmMemoryOperandSize];
		}

		readonly struct AsmRegisterInfo {
			public readonly string ClassName;
			public readonly string NamespaceName;
			public readonly string FnIsRegName;
			public readonly string Doc;

			public AsmRegisterInfo(string className, string namespaceName, string fnIsRegName, string doc) {
				ClassName = className;
				NamespaceName = namespaceName;
				FnIsRegName = fnIsRegName;
				Doc = doc;
			}
		}

		static AsmRegisterInfo GetAsmRegisterInfo(RegisterKind kind) =>
			kind switch {
				RegisterKind.None => throw new InvalidOperationException(),
				RegisterKind.GPR8 => new(AsmRegisterPrefix + "8", "gpr8", "is_gpr8", "All 8-bit general purpose registers."),
				RegisterKind.GPR16 => new(AsmRegisterPrefix + "16", "gpr16", "is_gpr16", "All 16-bit general purpose registers."),
				RegisterKind.GPR32 => new(AsmRegisterPrefix + "32", "gpr32", "is_gpr32", "All 32-bit general purpose registers."),
				RegisterKind.GPR64 => new(AsmRegisterPrefix + "64", "gpr64", "is_gpr64", "All 64-bit general purpose registers."),
				RegisterKind.IP => new(AsmRegisterPrefix + "Ip", "ip", "is_ip", "All instruction pointer registers."),
				RegisterKind.Segment => new(AsmRegisterPrefix + "Segment", "segment", "is_segment_register", "All segment registers."),
				RegisterKind.ST => new(AsmRegisterPrefix + "St", "st", "is_st", "All FPU registers."),
				RegisterKind.CR => new(AsmRegisterPrefix + "Cr", "cr", "is_cr", "All control registers."),
				RegisterKind.DR => new(AsmRegisterPrefix + "Dr", "dr", "is_dr", "All debug registers."),
				RegisterKind.TR => new(AsmRegisterPrefix + "Tr", "tr", "is_tr", "All test registers."),
				RegisterKind.BND => new(AsmRegisterPrefix + "Bnd", "bnd", "is_bnd", "All bound registers."),
				RegisterKind.K => new(AsmRegisterPrefix + "K", "k", "is_k", "All opmask registers."),
				RegisterKind.MM => new(AsmRegisterPrefix + "Mm", "mm", "is_mm", "All MMX registers."),
				RegisterKind.XMM => new(AsmRegisterPrefix + "Xmm", "xmm", "is_xmm", "All 128-bit vector registers (XMM)."),
				RegisterKind.YMM => new(AsmRegisterPrefix + "Ymm", "ymm", "is_ymm", "All 256-bit vector registers (YMM)."),
				RegisterKind.ZMM => new(AsmRegisterPrefix + "Zmm", "zmm", "is_zmm", "All 512-bit vector registers (ZMM)."),
				RegisterKind.TMM => new(AsmRegisterPrefix + "Tmm", "tmm", "is_tmm", "All tile registers."),
				_ => throw new InvalidOperationException(),
			};

		static string GetAOrAn(RegisterKind kind) =>
			kind switch {
				RegisterKind.IP or RegisterKind.ST or RegisterKind.MM or RegisterKind.XMM => "an",
				RegisterKind.GPR8 or RegisterKind.GPR16 or RegisterKind.GPR32 or RegisterKind.GPR64 or
				RegisterKind.Segment or RegisterKind.CR or RegisterKind.DR or RegisterKind.TR or
				RegisterKind.BND or RegisterKind.K or RegisterKind.YMM or RegisterKind.ZMM or
				RegisterKind.TMM => "a",
				_ => throw new InvalidOperationException(),
			};

		static void WriteNamespaceBegin(FileWriter writer, string ns) => writer.WriteLine($"namespace {ns} {{");
		static void WriteNamespaceEnd(FileWriter writer, string ns) => writer.WriteLine($"}} // namespace {ns}");

		protected override void GenerateRegisters((RegisterKind kind, RegisterDef[] regs)[] regGroups) {
			foreach (var (_, regs) in regGroups)
				Array.Sort(regs, (a, b) => a.Register.Value.CompareTo(b.Register.Value));

			var registerTypeName = registerType.Name(idConverter);
			var filename = CppConstants.GetIncludeFilename(genTypes, "code_asm", "registers.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include \"iced_x86/code_asm/reg.hpp\"");
				writer.WriteLine("#include \"iced_x86/register.hpp\"");
				writer.WriteLine("#include \"iced_x86/register_ext.hpp\"");
				writer.WriteLine();
				writer.WriteLine("#include <optional>");
				writer.WriteLine();
				writer.WriteLine("/// This namespace contains all registers that can be used by the code assembler.");
				writer.WriteLine("///");
				writer.WriteLine("/// All register identifiers (eg. `eax`, `cr8`) are part of the public API. They're `constexpr`");
				writer.WriteLine("/// constants and they're also available in the `iced_x86::code_asm` namespace:");
				writer.WriteLine("///");
				writer.WriteLine("/// ```cpp");
				writer.WriteLine("/// using namespace iced_x86::code_asm;");
				writer.WriteLine("/// ```");
				writer.WriteLine("///");
				writer.WriteLine("/// or only the registers you need:");
				writer.WriteLine("///");
				writer.WriteLine("/// ```cpp");
				writer.WriteLine("/// using namespace iced_x86::code_asm::registers::gpr32;");
				writer.WriteLine("/// using namespace iced_x86::code_asm::registers::gpr64;");
				writer.WriteLine("/// using namespace iced_x86::code_asm::registers::xmm;");
				writer.WriteLine("/// ```");
				WriteNamespaceBegin(writer, CodeAsmNamespace + "::registers");
				foreach (var (kind, regs) in regGroups) {
					var asmInfo = GetAsmRegisterInfo(kind);
					writer.WriteLine();
					writer.WriteLine($"/// {asmInfo.Doc}");
					WriteNamespaceBegin(writer, asmInfo.NamespaceName);
					writer.WriteLine();
					foreach (var regDef in regs) {
						var asmRegName = regDef.GetAsmRegisterName();
						writer.WriteLine($"/// `{regDef.Register.RawName}` register");
						writer.WriteLine($"inline constexpr {asmInfo.ClassName} {asmRegName}{{{idConverter.ToDeclTypeAndValue(regDef.Register)}}};");
					}
					writer.WriteLine();
					writer.WriteLine($"/// Gets {GetAOrAn(kind)} `{asmInfo.NamespaceName.ToUpperInvariant()}` register or `std::nullopt` if input is invalid.");
					writer.WriteLine("///");
					writer.WriteLine("/// @param register_ Register");
					writer.WriteLine($"[[nodiscard]] constexpr std::optional<{asmInfo.ClassName}> get_{asmInfo.NamespaceName}({registerTypeName} register_) noexcept {{");
					using (writer.Indent()) {
						writer.WriteLine($"if (register_ext::{asmInfo.FnIsRegName}(register_))");
						using (writer.Indent())
							writer.WriteLine($"return {asmInfo.ClassName}(register_);");
						writer.WriteLine("return std::nullopt;");
					}
					writer.WriteLine("}");
					writer.WriteLine();
					WriteNamespaceEnd(writer, asmInfo.NamespaceName);
				}
				writer.WriteLine();
				// Make all registers and get_xxx() fns available in the `registers` namespace (without the sub namespace
				// names since they're common identifiers, eg. `k`, `st`, `cr`)
				foreach (var (kind, regs) in regGroups.OrderBy(a => GetAsmRegisterInfo(a.kind).NamespaceName, StringComparer.Ordinal)) {
					var nsName = GetAsmRegisterInfo(kind).NamespaceName;
					foreach (var regDef in regs)
						writer.WriteLine($"using {nsName}::{regDef.GetAsmRegisterName()};");
					writer.WriteLine($"using {nsName}::get_{nsName};");
				}
				writer.WriteLine();
				WriteNamespaceEnd(writer, CodeAsmNamespace + "::registers");
				writer.WriteLine();
				WriteNamespaceBegin(writer, CodeAsmNamespace);
				writer.WriteLine();
				foreach (var (kind, regs) in regGroups.OrderBy(a => GetAsmRegisterInfo(a.kind).NamespaceName, StringComparer.Ordinal)) {
					var nsName = GetAsmRegisterInfo(kind).NamespaceName;
					foreach (var regDef in regs)
						writer.WriteLine($"using registers::{regDef.GetAsmRegisterName()};");
					writer.WriteLine($"using registers::get_{nsName};");
				}
				writer.WriteLine();
				WriteNamespaceEnd(writer, CodeAsmNamespace);
			}
		}

		protected override void GenerateRegisterClasses(RegisterClassInfo[] infos) {
			var filename = CppConstants.GetIncludeFilename(genTypes, "code_asm", "reg.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				var registerTypeName = registerType.Name(idConverter);

				writer.WriteLine("#include \"iced_x86/code_asm/op_state.hpp\"");
				writer.WriteLine("#include \"iced_x86/register.hpp\"");
				writer.WriteLine();
				WriteNamespaceBegin(writer, CodeAsmNamespace);
				foreach (var reg in infos) {
					var asmInfo = GetAsmRegisterInfo(reg.Kind);
					var className = asmInfo.ClassName;

					writer.WriteLine();
					writer.WriteLine($"/// {asmInfo.Doc}");
					writer.WriteLine("///");
					writer.WriteLine("/// The register constants (eg. `eax`, `xmm0`) are part of the public API. The register *types* are");
					writer.WriteLine("/// an implementation detail. See the `iced_x86::code_asm::registers` namespace.");
					writer.WriteLine($"class {className} {{");
					writer.WriteLine("public:");
					using (writer.Indent()) {
						writer.WriteLine("/// Creates a register. The register isn't verified, see also");
						writer.WriteLine($"/// `registers::get_{asmInfo.NamespaceName}()` which verifies the input register.");
						writer.WriteLine("///");
						writer.WriteLine("/// @param register_ Register");
						if (reg.NeedsState)
							writer.WriteLine($"explicit constexpr {className}({registerTypeName} register_) noexcept : reg_(register_), state_() {{}}");
						else
							writer.WriteLine($"explicit constexpr {className}({registerTypeName} register_) noexcept : reg_(register_) {{}}");
						writer.WriteLine();
						writer.WriteLine("/// Gets the register");
						writer.WriteLine($"[[nodiscard]] constexpr {registerTypeName} register_() const noexcept {{ return reg_; }}");
						writer.WriteLine();
						writer.WriteLine("/// Gets the register");
						writer.WriteLine($"explicit constexpr operator {registerTypeName}() const noexcept {{ return reg_; }}");
						if (reg.NeedsState) {
							writer.WriteLine();
							writer.WriteLine("/// Gets the operand state (opmask, `{z}`, `{sae}`, rounding control)");
							writer.WriteLine("[[nodiscard]] constexpr CodeAsmOpState state() const noexcept { return state_; }");
							for (int i = 1; i < 8; i++) {
								writer.WriteLine();
								writer.WriteLine($"/// Adds a `{{k{i}}}` opmask register");
								writer.WriteLine($"[[nodiscard]] constexpr {className} k{i}() const noexcept {{");
								using (writer.Indent()) {
									writer.WriteLine($"{className} result = *this;");
									writer.WriteLine($"result.state_.set_k{i}();");
									writer.WriteLine("return result;");
								}
								writer.WriteLine("}");
							}
							var stateMethods = new (string name, string stateName, string desc)[] {
								("z", "set_zeroing_masking", "Enables zeroing masking `{z}`"),
								("sae", "set_suppress_all_exceptions", "Enables suppress all exceptions `{sae}`"),
								("rn_sae", "rn_sae", "Round to nearest (even)"),
								("rd_sae", "rd_sae", "Round down (toward -inf)"),
								("ru_sae", "ru_sae", "Round up (toward +inf)"),
								("rz_sae", "rz_sae", "Round toward zero (truncate)"),
							};
							foreach (var (name, stateName, desc) in stateMethods) {
								writer.WriteLine();
								writer.WriteLine($"/// {desc}");
								writer.WriteLine($"[[nodiscard]] constexpr {className} {name}() const noexcept {{");
								using (writer.Indent()) {
									writer.WriteLine($"{className} result = *this;");
									writer.WriteLine($"result.state_.{stateName}();");
									writer.WriteLine("return result;");
								}
								writer.WriteLine("}");
							}
						}
						writer.WriteLine();
						if (reg.NeedsState) {
							writer.WriteLine($"constexpr bool operator==(const {className}& other) const noexcept {{ return reg_ == other.reg_ && state_ == other.state_; }}");
							writer.WriteLine($"constexpr bool operator!=(const {className}& other) const noexcept {{ return !(*this == other); }}");
						}
						else {
							writer.WriteLine($"constexpr bool operator==(const {className}& other) const noexcept {{ return reg_ == other.reg_; }}");
							writer.WriteLine($"constexpr bool operator!=(const {className}& other) const noexcept {{ return reg_ != other.reg_; }}");
						}
					}
					writer.WriteLine();
					writer.WriteLine("private:");
					using (writer.Indent()) {
						writer.WriteLine($"{registerTypeName} reg_;");
						if (reg.NeedsState)
							writer.WriteLine("CodeAsmOpState state_;");
					}
					writer.WriteLine("};");
				}
				writer.WriteLine();
				WriteNamespaceEnd(writer, CodeAsmNamespace);
			}
		}

		static string GetName(MemorySizeFuncInfo fnInfo) => fnInfo.Name.Replace(' ', '_');

		protected override void GenerateMemorySizeFunctions(MemorySizeFuncInfo[] infos) {
			var filename = CppConstants.GetIncludeFilename(genTypes, "code_asm", "mem_ptr.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include \"iced_x86/code_asm/mem.hpp\"");
				writer.WriteLine("#include \"iced_x86/code_asm/memory_operand_size.hpp\"");
				writer.WriteLine();
				WriteNamespaceBegin(writer, CodeAsmNamespace);
				foreach (var info in infos) {
					var fnName = GetName(info);
					var calledFnName = info.IsBroadcast ? "with_bcst" : "with_ptr";
					var doc = info.GetMethodDocs("Creates", s => $"`{s}`");
					var enumValue = memoryOperandSizeType[info.Size.ToString()];

					writer.WriteLine();
					writer.WriteLine($"/// {doc}");
					writer.WriteLine("///");
					writer.WriteLine("/// @param mem Memory operand, register (base), label or displacement");
					writer.WriteLine("///");
					writer.WriteLine("/// # Examples");
					writer.WriteLine("///");
					writer.WriteLine("/// ```cpp");
					writer.WriteLine("/// using namespace iced_x86::code_asm;");
					writer.WriteLine("///");
					writer.WriteLine($"/// auto m1 = {fnName}(rax);");
					writer.WriteLine($"/// auto m2 = {fnName}(0x12345678).fs();");
					writer.WriteLine($"/// auto m3 = {fnName}(rdx * 4 + rcx - 123);");
					writer.WriteLine("/// ```");
					writer.WriteLine($"[[nodiscard]] constexpr {AsmMemoryOperand} {fnName}(const {AsmMemoryOperandArg}& mem) noexcept {{");
					using (writer.Indent())
						writer.WriteLine($"return mem.value().{calledFnName}({idConverter.ToDeclTypeAndValue(enumValue)});");
					writer.WriteLine("}");
				}
				writer.WriteLine();
				WriteNamespaceEnd(writer, CodeAsmNamespace);
			}
		}

		sealed class TraitGroup {
			public readonly List<OpCodeInfoGroup> Groups = new();
			public string Name => Groups[0].Name;
			public int ArgCount => Groups[0].Signature.ArgCount;
		}

		// Same as the Rust generator: all groups with the same name and arg count are grouped together. If there are
		// several arg counts, the name gets an `_<argcount>` suffix (except the one with the fewest args), eg. `ret_1()`.
		static TraitGroup[] CreateTraitGroups(OpCodeInfoGroup[] groups) {
			var dict = new Dictionary<string, Dictionary<int, List<OpCodeInfoGroup>>>(StringComparer.Ordinal);

			foreach (var group in groups) {
				if (!dict.TryGetValue(group.Name, out var traitDict))
					dict.Add(group.Name, traitDict = new());
				if (!traitDict.TryGetValue(group.Signature.ArgCount, out var list))
					traitDict.Add(group.Signature.ArgCount, list = new());
				list.Add(group);
			}

			var result = new List<TraitGroup>(dict.Count);
			foreach (var kv in dict.OrderBy(x => x.Key, StringComparer.Ordinal)) {
				bool addSuffix = false;
				foreach (var list in kv.Value.Select(x => x.Value).OrderBy(x => x[0].Signature.ArgCount)) {
					var traitGroup = new TraitGroup();
					result.Add(traitGroup);
					foreach (var group in list) {
						group.AddNameSuffix = addSuffix;
						traitGroup.Groups.Add(group);
					}
					addSuffix = true;
				}
			}
			return result.ToArray();
		}

		protected override void Generate(Dictionary<GroupKey, OpCodeInfoGroup> map, OpCodeInfoGroup[] groups) {
			var traitGroups = CreateTraitGroups(groups);
			var fnsClasses = SplitIntoClasses(traitGroups);
			GenerateAsmDecls(fnsClasses);
			GenerateAsmImpl(fnsClasses);
			GenerateTests(traitGroups);
		}

		// The instruction methods are split into several classes (CodeAssemblerFns0, CodeAssemblerFns1, ...) since some
		// compilers (GCC) are very slow (quadratic) when a class has thousands of members. All overloads with the same
		// name must be in the same class. Each class is implemented in its own .cpp file.
		static List<TraitGroup>[] SplitIntoClasses(TraitGroup[] traitGroups) {
			var weights = traitGroups.Select(a => a.Groups.Sum(GetImplWeight)).ToArray();
			long totalWeight = weights.Sum();
			var result = new List<TraitGroup>[ImplFileCount];
			int index = 0;
			long weight = 0;
			for (int classIndex = 0; classIndex < result.Length; classIndex++) {
				var list = result[classIndex] = new List<TraitGroup>();
				long maxWeight = totalWeight * (classIndex + 1) / ImplFileCount;
				while (index < traitGroups.Length && (weight < maxWeight || classIndex == result.Length - 1)) {
					list.Add(traitGroups[index]);
					weight += weights[index];
					index++;
				}
				if (list.Count == 0)
					throw new InvalidOperationException();
			}
			if (index != traitGroups.Length)
				throw new InvalidOperationException();
			return result;
		}

		static string GetFnsClassName(int index) => "CodeAssemblerFns" + index.ToString(CultureInfo.InvariantCulture);

		static string GetFnName(OpCodeInfoGroup group) {
			if (group.AddNameSuffix)
				return CppIdentifierConverter.Escape(group.Name + "_" + group.Signature.ArgCount.ToString(CultureInfo.InvariantCulture));
			return CppIdentifierConverter.Escape(group.Name);
		}

		static string GetFnArgName(int argIndex) => "op" + argIndex.ToString(CultureInfo.InvariantCulture);

		static bool IsLabelOrMemory(ArgKind argKind) => argKind == ArgKind.Memory || argKind == ArgKind.Label;

		static string ToTypeString(ArgKind argKind, int maxArgSize) =>
			argKind switch {
				ArgKind.Register8 => AsmRegisterPrefix + "8",
				ArgKind.Register16 => AsmRegisterPrefix + "16",
				ArgKind.Register32 => AsmRegisterPrefix + "32",
				ArgKind.Register64 => AsmRegisterPrefix + "64",
				ArgKind.RegisterK => AsmRegisterPrefix + "K",
				ArgKind.RegisterSt => AsmRegisterPrefix + "St",
				ArgKind.RegisterSegment => AsmRegisterPrefix + "Segment",
				ArgKind.RegisterBnd => AsmRegisterPrefix + "Bnd",
				ArgKind.RegisterMm => AsmRegisterPrefix + "Mm",
				ArgKind.RegisterXmm => AsmRegisterPrefix + "Xmm",
				ArgKind.RegisterYmm => AsmRegisterPrefix + "Ymm",
				ArgKind.RegisterZmm => AsmRegisterPrefix + "Zmm",
				ArgKind.RegisterCr => AsmRegisterPrefix + "Cr",
				ArgKind.RegisterDr => AsmRegisterPrefix + "Dr",
				ArgKind.RegisterTr => AsmRegisterPrefix + "Tr",
				ArgKind.RegisterTmm => AsmRegisterPrefix + "Tmm",
				ArgKind.Memory => AsmMemoryOperand,
				ArgKind.Immediate => maxArgSize switch {
					1 or 2 or 4 => "std::int32_t",
					8 => "std::int64_t",
					_ => throw new InvalidOperationException(),
				},
				ArgKind.ImmediateUnsigned => maxArgSize switch {
					1 or 2 or 4 => "std::uint32_t",
					8 => "std::uint64_t",
					_ => throw new InvalidOperationException(),
				},
				ArgKind.Label => CodeLabel,
				ArgKind.LabelU64 => "std::uint64_t",
				_ => throw new InvalidOperationException($"Invalid arg kind: {argKind}"),
			};

		static void WriteParams(FileWriter writer, OpCodeInfoGroup group) {
			for (int i = 0; i < group.Signature.ArgCount; i++) {
				if (i != 0)
					writer.Write(", ");
				writer.Write(ToTypeString(group.Signature.GetArgKind(i), group.MaxArgSizes[i]));
				writer.Write(" ");
				writer.Write(GetFnArgName(i));
			}
		}

		static List<InstructionDef> GetSortedDefs(OpCodeInfoGroup group) {
			var defHash = new HashSet<InstructionDef>();
			var sortedDefs = new List<InstructionDef>();
			foreach (var def in group.GetDefsAndParentDefs()) {
				if (defHash.Add(def))
					sortedDefs.Add(def);
			}
			if (sortedDefs.Count == 0)
				throw new InvalidOperationException();
			sortedDefs.Sort(CompareInstructionDefs);
			return sortedDefs;
		}

		// Same order as the Rust generator
		static int CompareInstructionDefs(InstructionDef? x, InstructionDef? y) {
			if ((object?)x == (object?)y) return 0;
			if (x is null) return -1;
			if (y is null) return 1;
			int c;
			c = x.Table.CompareTo(y.Table);
			if (c != 0) return c;
			c = x.OpCode.CompareTo(y.OpCode);
			if (c != 0) return c;
			c = x.MandatoryPrefix.CompareTo(y.MandatoryPrefix);
			if (c != 0) return c;
			c = x.GroupIndex.CompareTo(y.GroupIndex);
			if (c != 0) return c;
			c = x.RmGroupIndex.CompareTo(y.RmGroupIndex);
			if (c != 0) return c;
			c = x.Encoding.CompareTo(y.Encoding);
			if (c != 0) return c;
			c = x.WBit.CompareTo(y.WBit);
			if (c != 0) return c;
			c = x.LBit.CompareTo(y.LBit);
			if (c != 0) return c;
			c = x.OperandSize.CompareTo(y.OperandSize);
			if (c != 0) return c;
			c = x.AddressSize.CompareTo(y.AddressSize);
			if (c != 0) return c;
			c = GetBitnessCompareValue(x).CompareTo(GetBitnessCompareValue(y));
			if (c != 0) return c;
			c = CompareSignatures(x, y);
			if (c != 0) return c;
			throw new InvalidOperationException($"Couldn't sort: {x.Code.RawName} vs {y.Code.RawName}");

			static int GetBitnessCompareValue(InstructionDef def) {
				int result = 0;
				if ((def.Flags1 & InstructionDefFlags1.Bit16) != 0)
					result |= 1;
				if ((def.Flags1 & InstructionDefFlags1.Bit32) != 0)
					result |= 2;
				if ((def.Flags1 & InstructionDefFlags1.Bit64) != 0)
					result |= 4;
				return result;
			}

			static int CompareSignatures(InstructionDef x, InstructionDef y) {
				var xOps = x.OpKindDefs;
				var yOps = y.OpKindDefs;
				int c = xOps.Length.CompareTo(yOps.Length);
				if (c != 0) return c;
				for (int i = 0; i < xOps.Length; i++) {
					var xOp = xOps[i];
					var yOp = yOps[i];
					c = xOp.HasRegister.CompareTo(yOp.HasRegister);
					if (c != 0) return c;
					if (xOp.HasRegister) {
						c = xOp.Register.CompareTo(yOp.Register);
						if (c != 0) return c;
					}
					c = xOp.Memory.CompareTo(yOp.Memory);
					if (c != 0) return c;
				}

				return 0;
			}
		}

		static string FixDocLine(string line) => line.EndsWith('\\') ? line + " " : line;

		void GenerateAsmDecls(List<TraitGroup>[] fnsClasses) {
			var filename = CppConstants.GetIncludeFilename(genTypes, "code_asm", "code_assembler_fns.hpp");
			using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
				CppConstants.WriteHeaderFileHeader(writer);
				writer.WriteLine("#include \"iced_x86/code_asm/code_assembler_base.hpp\"");
				writer.WriteLine("#include \"iced_x86/code_asm/code_label.hpp\"");
				writer.WriteLine("#include \"iced_x86/code_asm/mem.hpp\"");
				writer.WriteLine("#include \"iced_x86/code_asm/reg.hpp\"");
				writer.WriteLine();
				writer.WriteLine("#include <cstdint>");
				writer.WriteLine();
				WriteNamespaceBegin(writer, CodeAsmNamespace);
				writer.WriteLine();
				writer.WriteLine($"class {CodeAssembler};");
				writer.WriteLine();
				WriteNamespaceBegin(writer, "internal");
				for (int classIndex = 0; classIndex < fnsClasses.Length; classIndex++) {
					var className = GetFnsClassName(classIndex);
					var baseClassName = classIndex == 0 ? "CodeAssemblerBase" : GetFnsClassName(classIndex - 1);
					writer.WriteLine();
					writer.WriteLine($"/// `{CodeAssembler}` instruction methods (part {classIndex + 1}/{fnsClasses.Length}), see `{CodeAssembler}`.");
					writer.WriteLine("///");
					writer.WriteLine("/// All methods set the error (see `has_error()`) if an operand is invalid (basic checks only).");
					writer.WriteLine($"class {className} : public {baseClassName} {{");
					writer.WriteLine("protected:");
					using (writer.Indent())
						writer.WriteLine($"explicit {className}(std::uint32_t bitness) : {baseClassName}(bitness) {{}}");
					writer.WriteLine();
					writer.WriteLine("public:");
					using (writer.Indent()) {
						bool needNewLine = false;
						foreach (var traitGroup in fnsClasses[classIndex]) {
							foreach (var group in traitGroup.Groups) {
								if (needNewLine)
									writer.WriteLine();
								needNewLine = true;
								writer.WriteLine($"/// `{traitGroup.Name.ToUpperInvariant()}` instruction");
								writer.WriteLine("///");
								writer.WriteLine("/// Instruction | Opcode | CPUID");
								writer.WriteLine("/// ------------|--------|------");
								foreach (var def in GetSortedDefs(group)) {
									var cpuid = string.Join(" ", def.CpuidFeatureStrings);
									writer.WriteLine(FixDocLine($"/// `{def.InstructionString}` | `{def.OpCodeString}` | `{cpuid}`"));
								}
								writer.Write($"{CodeAssembler}& {GetFnName(group)}(");
								WriteParams(writer, group);
								writer.WriteLine(");");
							}
						}
					}
					writer.WriteLine("};");
				}
				writer.WriteLine();
				writer.WriteLine("/// The last instruction methods class. `CodeAssembler` derives from it.");
				writer.WriteLine($"using CodeAssemblerFnsAll = {GetFnsClassName(fnsClasses.Length - 1)};");
				writer.WriteLine();
				WriteNamespaceEnd(writer, "internal");
				writer.WriteLine();
				WriteNamespaceEnd(writer, CodeAsmNamespace);
			}
		}

		static bool SpecialInstructionHasSegmentArg(string mnemonicName) =>
			!(mnemonicName.StartsWith("Ins", StringComparison.Ordinal) ||
			mnemonicName.StartsWith("Scas", StringComparison.Ordinal) ||
			mnemonicName.StartsWith("Stos", StringComparison.Ordinal) ||
			mnemonicName.StartsWith("Xbegin", StringComparison.Ordinal));

		static string GetOverloadedCreateName(int argCount) => argCount == 0 ? "with" : "with" + argCount.ToString(CultureInfo.InvariantCulture);

		static void WriteArg(FileWriter writer, string argExpr, ArgKind kind) {
			if (IsRegister(kind))
				writer.Write($"{argExpr}.register_()");
			else if (kind == ArgKind.Label)
				writer.Write($"{argExpr}.id()");
			else if (kind == ArgKind.Memory)
				writer.Write($"to_memory_operand({argExpr})");
			else
				writer.Write(argExpr);
		}

		void DeleteGeneratedFiles(string dir, string pattern) {
			if (Directory.Exists(dir)) {
				foreach (var file in Directory.GetFiles(dir, pattern))
					File.Delete(file);
			}
		}

		void GenerateAsmImpl(List<TraitGroup>[] fnsClasses) {
			var dir = Path.GetDirectoryName(CppConstants.GetSrcFilename(genTypes, "code_asm", "x.cpp")) ?? throw new InvalidOperationException();
			DeleteGeneratedFiles(dir, "fn_asm_impl_*.cpp");

			for (int classIndex = 0; classIndex < fnsClasses.Length; classIndex++) {
				var filename = CppConstants.GetSrcFilename(genTypes, "code_asm", $"fn_asm_impl_{classIndex}.cpp");
				var className = GetFnsClassName(classIndex);
				using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
					writer.WriteFileHeader();
					writer.WriteLine("#include \"iced_x86/code_asm/code_assembler.hpp\"");
					writer.WriteLine("#include \"iced_x86/code.hpp\"");
					writer.WriteLine("#include \"iced_x86/instruction.hpp\"");
					writer.WriteLine("#include \"iced_x86/register.hpp\"");
					writer.WriteLine("#include \"iced_x86/register_ext.hpp\"");
					writer.WriteLine("#include \"iced_x86/rep_prefix_kind.hpp\"");
					writer.WriteLine();
					WriteNamespaceBegin(writer, CodeAsmNamespace + "::internal");
					foreach (var traitGroup in fnsClasses[classIndex]) {
						foreach (var group in traitGroup.Groups) {
							writer.WriteLine();
							WriteMethodImpl(writer, className, group);
						}
					}
					writer.WriteLine();
					WriteNamespaceEnd(writer, CodeAsmNamespace + "::internal");
				}
			}
		}

		static int GetImplWeight(OpCodeInfoGroup group) {
			static int GetNodeWeight(OpCodeNode node) {
				if (node.Def is not null)
					return 1;
				if (node.Selector is OpCodeSelector selector)
					return 2 + GetNodeWeight(selector.IfTrue) + (selector.IfFalse.IsEmpty ? 1 : GetNodeWeight(selector.IfFalse));
				return 0;
			}
			return 4 + (group.ParentPseudoOpsKind is null && !group.HasSpecialInstructionEncoding ? GetNodeWeight(group.RootOpCodeNode) : 0);
		}

		void WriteMethodImpl(FileWriter writer, string className, OpCodeInfoGroup group) {
			var registerNoneStr = idConverter.ToDeclTypeAndValue(registerType[nameof(Register.None)]);
			var repPrefixNoneStr = idConverter.ToDeclTypeAndValue(genTypes[TypeIds.RepPrefixKind][nameof(RepPrefixKind.None)]);
			int opCount = group.Signature.ArgCount;

			writer.Write($"{CodeAssembler}& {className}::{GetFnName(group)}(");
			WriteParams(writer, group);
			writer.WriteLine(") {");
			using (writer.Indent()) {
				if (group.ParentPseudoOpsKind is OpCodeInfoGroup parent) {
					var intType = ToTypeString(parent.Signature.GetArgKind(group.Signature.ArgCount), parent.MaxArgSizes[group.Signature.ArgCount]);
					// The parent method could be in a derived class
					writer.Write($"return self().{GetFnName(parent)}(");
					for (int i = 0; i < opCount; i++)
						writer.Write($"{GetFnArgName(i)}, ");
					writer.WriteLine($"static_cast<{intType}>({group.PseudoOpsKindImmediateValue}));");
				}
				else if (group.HasSpecialInstructionEncoding) {
					writer.Write($"return add_instr(Instruction::with_{group.MnemonicName.ToLowerInvariant()}(bitness()");
					for (int i = 0; i < opCount; i++) {
						writer.Write(", ");
						WriteArg(writer, GetFnArgName(i), group.Signature.GetArgKind(i));
					}
					if (SpecialInstructionHasSegmentArg(group.MnemonicName)) {
						writer.Write(", ");
						writer.Write(registerNoneStr);
					}
					if ((group.Defs[0].Flags3 & InstructionDefFlags3.IsStringOp) != 0) {
						writer.Write(", ");
						writer.Write(repPrefixNoneStr);
					}
					writer.WriteLine("));");
				}
				else {
					string codeExpr;
					if (group.RootOpCodeNode.Def is InstructionDef def)
						codeExpr = idConverter.ToDeclTypeAndValue(def.Code);
					else {
						codeExpr = "code";
						GenerateCodeEnumSelectorCode(writer, group, codeExpr);
					}
					string withFnName = group.HasLabel ? "with_branch" : GetOverloadedCreateName(opCount);

					var stateArgs = GetStateArgIndexes(group).Select(a => $"{GetFnArgName(a)}.state()").ToList();
					var addInstrName = stateArgs.Count == 0 ? "add_instr" : "add_instr_with_state";
					writer.Write($"return {addInstrName}(Instruction::{withFnName}({codeExpr}");
					for (int i = 0; i < opCount; i++) {
						writer.Write(", ");
						WriteArg(writer, GetFnArgName(i), group.Signature.GetArgKind(i));
					}
					writer.Write(")");
					if (stateArgs.Count != 0) {
						writer.Write(", ");
						for (int i = 0; i < stateArgs.Count; i++) {
							if (i != 0)
								writer.Write(".merge(");
							writer.Write(stateArgs[i]);
							if (i != 0)
								writer.Write(")");
						}
					}
					writer.WriteLine(");");
				}
			}
			writer.WriteLine("}");
		}

		static (ArgKind argKind, int maxArgSize) GetArgInfo(OpCodeInfoGroup group, int argIndex) {
			if (argIndex >= 0)
				return (group.Signature.GetArgKind(argIndex), group.MaxArgSizes[argIndex]);
			else
				return (ArgKind.Unknown, 0);
		}

		void GenerateCodeEnumSelectorCode(FileWriter writer, OpCodeInfoGroup group, string codeVar) {
			var codeStr = genTypes[TypeIds.Code].Name(idConverter);
			var root = group.RootOpCodeNode.Selector ?? throw new InvalidOperationException();
			if (root.IfTrue.Def is InstructionDef defTrue && root.IfFalse.Def is InstructionDef defFalse) {
				var (argKind, maxArgSize) = GetArgInfo(group, root.ArgIndex);
				var condition = GetArgConditionForOpCodeKind(root.ArgIndex, argKind, maxArgSize, root.Kind);
				var trueExpr = idConverter.ToDeclTypeAndValue(defTrue.Code);
				var falseExpr = idConverter.ToDeclTypeAndValue(defFalse.Code);
				writer.WriteLine($"const {codeStr} {codeVar} = {condition} ? {trueExpr} : {falseExpr};");
			}
			else {
				writer.WriteLine($"{codeStr} {codeVar};");
				GenerateCodeEnumSelectorCode(writer, group, group.RootOpCodeNode, codeVar);
			}
		}

		void GenerateCodeEnumSelectorCode(FileWriter writer, OpCodeInfoGroup group, OpCodeNode node, string codeVar) {
			if (node.Def is InstructionDef def)
				writer.WriteLine($"{codeVar} = {idConverter.ToDeclTypeAndValue(def.Code)};");
			else if (node.Selector is OpCodeSelector selector) {
				var (argKind, maxArgSize) = GetArgInfo(group, selector.ArgIndex);
				var condition = GetArgConditionForOpCodeKind(selector.ArgIndex, argKind, maxArgSize, selector.Kind);
				writer.WriteLine($"if ({condition}) {{");
				using (writer.Indent())
					GenerateCodeEnumSelectorCode(writer, group, selector.IfTrue, codeVar);
				if (selector.IfFalse.IsEmpty) {
					writer.WriteLine("} else {");
					using (writer.Indent())
						writer.WriteLine($"return set_error(\"{group.Name}: invalid operands\");");
					writer.WriteLine("}");
				}
				else if (selector.IfFalse.Selector is not null) {
					// `} else if (...) {`
					writer.Write("} else ");
					GenerateCodeEnumSelectorCode(writer, group, selector.IfFalse, codeVar);
				}
				else {
					writer.WriteLine("} else {");
					using (writer.Indent())
						GenerateCodeEnumSelectorCode(writer, group, selector.IfFalse, codeVar);
					writer.WriteLine("}");
				}
			}
			else
				throw new InvalidOperationException();
		}

		string GetArgConditionForOpCodeKind(int argIndex, ArgKind argKind, int maxArgSize, OpCodeSelectorKind selectorKind) {
			var argName = GetFnArgName(argIndex);
			var otherArgName = GetFnArgName(argIndex == 0 ? 1 : 0);
			return selectorKind switch {
				OpCodeSelectorKind.MemOffs64_RAX => $"{otherArgName}.register_() == {GetRegisterString(nameof(Register.RAX))} && bitness() == 64 && {argName}.is_displacement_only()",
				OpCodeSelectorKind.MemOffs64_EAX => $"{otherArgName}.register_() == {GetRegisterString(nameof(Register.EAX))} && bitness() == 64 && {argName}.is_displacement_only()",
				OpCodeSelectorKind.MemOffs64_AX => $"{otherArgName}.register_() == {GetRegisterString(nameof(Register.AX))} && bitness() == 64 && {argName}.is_displacement_only()",
				OpCodeSelectorKind.MemOffs64_AL => $"{otherArgName}.register_() == {GetRegisterString(nameof(Register.AL))} && bitness() == 64 && {argName}.is_displacement_only()",
				OpCodeSelectorKind.MemOffs_RAX => $"{otherArgName}.register_() == {GetRegisterString(nameof(Register.RAX))} && bitness() < 64 && {argName}.is_displacement_only()",
				OpCodeSelectorKind.MemOffs_EAX => $"{otherArgName}.register_() == {GetRegisterString(nameof(Register.EAX))} && bitness() < 64 && {argName}.is_displacement_only()",
				OpCodeSelectorKind.MemOffs_AX => $"{otherArgName}.register_() == {GetRegisterString(nameof(Register.AX))} && bitness() < 64 && {argName}.is_displacement_only()",
				OpCodeSelectorKind.MemOffs_AL => $"{otherArgName}.register_() == {GetRegisterString(nameof(Register.AL))} && bitness() < 64 && {argName}.is_displacement_only()",
				OpCodeSelectorKind.Bitness64 => "bitness() == 64",
				OpCodeSelectorKind.Bitness32 => "bitness() >= 32",
				OpCodeSelectorKind.Bitness16 => "bitness() >= 16",
				OpCodeSelectorKind.ShortBranch => "prefer_short_branch()",
				OpCodeSelectorKind.ImmediateByteEqual1 => $"{argName} == 1",
				OpCodeSelectorKind.ImmediateByteSigned8To32 or OpCodeSelectorKind.ImmediateByteSigned8To64 => argKind == ArgKind.ImmediateUnsigned ?
					$"({argName} <= 0x7FU || 0xFFFFFF80U <= {argName})" :
					$"({argName} >= -0x80 && {argName} <= 0x7F)",
				OpCodeSelectorKind.ImmediateByteSigned8To16 => argKind == ArgKind.ImmediateUnsigned ?
					$"({argName} <= 0x7FU || (0xFF80U <= {argName} && {argName} <= 0xFFFFU))" :
					$"({argName} >= -0x80 && {argName} <= 0x7F)",
				OpCodeSelectorKind.Vex => "instruction_prefer_vex()",
				OpCodeSelectorKind.EvexBroadcastX or OpCodeSelectorKind.EvexBroadcastY or OpCodeSelectorKind.EvexBroadcastZ =>
					$"{argName}.is_broadcast()",
				OpCodeSelectorKind.RegisterCL => $"{argName}.register_() == {GetRegisterString(nameof(Register.CL))}",
				OpCodeSelectorKind.RegisterAL => $"{argName}.register_() == {GetRegisterString(nameof(Register.AL))}",
				OpCodeSelectorKind.RegisterAX => $"{argName}.register_() == {GetRegisterString(nameof(Register.AX))}",
				OpCodeSelectorKind.RegisterEAX => $"{argName}.register_() == {GetRegisterString(nameof(Register.EAX))}",
				OpCodeSelectorKind.RegisterRAX => $"{argName}.register_() == {GetRegisterString(nameof(Register.RAX))}",
				OpCodeSelectorKind.RegisterBND => $"register_ext::is_bnd({argName}.register_())",
				OpCodeSelectorKind.RegisterES => $"{argName}.register_() == {GetRegisterString(nameof(Register.ES))}",
				OpCodeSelectorKind.RegisterCS => $"{argName}.register_() == {GetRegisterString(nameof(Register.CS))}",
				OpCodeSelectorKind.RegisterSS => $"{argName}.register_() == {GetRegisterString(nameof(Register.SS))}",
				OpCodeSelectorKind.RegisterDS => $"{argName}.register_() == {GetRegisterString(nameof(Register.DS))}",
				OpCodeSelectorKind.RegisterFS => $"{argName}.register_() == {GetRegisterString(nameof(Register.FS))}",
				OpCodeSelectorKind.RegisterGS => $"{argName}.register_() == {GetRegisterString(nameof(Register.GS))}",
				OpCodeSelectorKind.RegisterDX => $"{argName}.register_() == {GetRegisterString(nameof(Register.DX))}",
				OpCodeSelectorKind.Register8 => $"register_ext::is_gpr8({argName}.register_())",
				OpCodeSelectorKind.Register16 => $"register_ext::is_gpr16({argName}.register_())",
				OpCodeSelectorKind.Register32 => $"register_ext::is_gpr32({argName}.register_())",
				OpCodeSelectorKind.Register64 => $"register_ext::is_gpr64({argName}.register_())",
				OpCodeSelectorKind.RegisterK => $"register_ext::is_k({argName}.register_())",
				OpCodeSelectorKind.RegisterST0 => $"{argName}.register_() == {GetRegisterString(nameof(Register.ST0))}",
				OpCodeSelectorKind.RegisterST => $"register_ext::is_st({argName}.register_())",
				OpCodeSelectorKind.RegisterSegment => $"register_ext::is_segment_register({argName}.register_())",
				OpCodeSelectorKind.RegisterCR => $"register_ext::is_cr({argName}.register_())",
				OpCodeSelectorKind.RegisterDR => $"register_ext::is_dr({argName}.register_())",
				OpCodeSelectorKind.RegisterTR => $"register_ext::is_tr({argName}.register_())",
				OpCodeSelectorKind.RegisterMM => $"register_ext::is_mm({argName}.register_())",
				OpCodeSelectorKind.RegisterXMM => $"register_ext::is_xmm({argName}.register_())",
				OpCodeSelectorKind.RegisterYMM => $"register_ext::is_ymm({argName}.register_())",
				OpCodeSelectorKind.RegisterZMM => $"register_ext::is_zmm({argName}.register_())",
				OpCodeSelectorKind.RegisterTMM => $"register_ext::is_tmm({argName}.register_())",
				OpCodeSelectorKind.Memory8 => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Byte))}",
				OpCodeSelectorKind.Memory16 => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Word))}",
				OpCodeSelectorKind.Memory32 => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Dword))}",
				OpCodeSelectorKind.Memory48 => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Fword))}",
				OpCodeSelectorKind.Memory64 => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Qword))}",
				OpCodeSelectorKind.Memory80 => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Tbyte))}",
				OpCodeSelectorKind.MemoryMM => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Qword))}",
				OpCodeSelectorKind.MemoryXMM => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Xword))}",
				OpCodeSelectorKind.MemoryYMM => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Yword))}",
				OpCodeSelectorKind.MemoryZMM => $"{argName}.size() == {GetMemOpSizeString(nameof(MemoryOperandSize.Zword))}",
				OpCodeSelectorKind.MemoryIndex32Xmm or OpCodeSelectorKind.MemoryIndex64Xmm => $"register_ext::is_xmm({argName}.index())",
				OpCodeSelectorKind.MemoryIndex32Ymm or OpCodeSelectorKind.MemoryIndex64Ymm => $"register_ext::is_ymm({argName}.index())",
				OpCodeSelectorKind.MemoryIndex32Zmm or OpCodeSelectorKind.MemoryIndex64Zmm => $"register_ext::is_zmm({argName}.index())",
				_ => throw new InvalidOperationException(),
			};
		}

		string GetRegisterString(string fieldName) =>
			idConverter.ToDeclTypeAndValue(registerType[fieldName]);

		string GetMemOpSizeString(string fieldName) =>
			idConverter.ToDeclTypeAndValue(memoryOperandSizeType[fieldName]);

		void GenerateTests(TraitGroup[] traitGroups) {
			var dir = Path.GetDirectoryName(CppConstants.GetTestFilename(genTypes, "code_asm", "generated", "x.cpp")) ?? throw new InvalidOperationException();
			DeleteGeneratedFiles(dir, "instr*.cpp");

			var sb = new StringBuilder();
			foreach (var bitness in new[] { 16, 32, 64 }) {
				var bitnessFlag = bitness switch {
					16 => InstructionDefFlags1.Bit16,
					32 => InstructionDefFlags1.Bit32,
					64 => InstructionDefFlags1.Bit64,
					_ => throw new InvalidOperationException(),
				};

				var tests = new List<(TraitGroup traitGroup, OpCodeInfoGroup group, string testName)>();
				foreach (var traitGroup in traitGroups) {
					foreach (var group in traitGroup.Groups) {
						if ((group.AllDefFlags & bitnessFlag) == 0)
							continue;
						if (group.Name == "xbegin")
							continue; // Implemented manually

						var testName = GetTestMethodName(sb, group);
						if (ignoredTestsPerBitness.TryGetValue(bitness, out var ignoredTests) && ignoredTests.Contains(testName))
							continue;
						tests.Add((traitGroup, group, testName));
					}
				}

				int fileCount = (tests.Count + MaxTestsPerFile - 1) / MaxTestsPerFile;
				int testIndex = 0;
				for (int fileIndex = 0; fileIndex < fileCount; fileIndex++) {
					int endIndex = (int)((long)tests.Count * (fileIndex + 1) / fileCount);
					var filename = CppConstants.GetTestFilename(genTypes, "code_asm", "generated", $"instr{bitness}_{fileIndex}.cpp");
					using (var writer = new FileWriter(TargetLanguage.Cpp, FileUtils.OpenWrite(filename))) {
						writer.WriteFileHeader();
						writer.WriteLine("#include \"code_asm/code_asm_test_utils.hpp\"");
						writer.WriteLine();
						WriteNamespaceBegin(writer, TestsNamespace);
						writer.WriteLine();
						writer.WriteLine("using namespace iced_x86::code_asm;");
						for (; testIndex < endIndex; testIndex++) {
							var (traitGroup, group, testName) = tests[testIndex];
							writer.WriteLine();
							writer.WriteLine($"TEST_CASE(\"code_asm/instr{bitness}/{testName}\") {{");
							var args = new TestArgValues(traitGroup.ArgCount);
							using (writer.Indent()) {
								// Rust also generates a 2nd test without an `i32` literal suffix. C++ int literals are
								// always `int` (`std::int32_t`) so it would be identical to the 1st test.
								GenerateTests(writer, bitness, group, OpCodeArgFlags.None, args);
							}
							writer.WriteLine("}");
						}
						writer.WriteLine();
						WriteNamespaceEnd(writer, TestsNamespace);
					}
				}
			}
		}

		void GenerateTests(FileWriter writer, int bitness, OpCodeInfoGroup group, OpCodeArgFlags contextFlags, TestArgValues args) {
			if (group.ParentPseudoOpsKind is OpCodeInfoGroup parent)
				GenerateTestsForInstr(writer, bitness, group, parent.Defs[0], contextFlags, args);
			else
				GenerateTests(writer, bitness, group, group.RootOpCodeNode, contextFlags, args, false);
		}

		static string GetTestMethodName(StringBuilder sb, OpCodeInfoGroup group) {
			sb.Clear();
			sb.Append(group.Name.ToLowerInvariant());
			for (int i = 0; i < group.Signature.ArgCount; i++) {
				sb.Append('_');
				sb.Append(GetTestMethodArgName(group.Signature.GetArgKind(i)));
			}
			return sb.ToString();
		}

		void GenerateTests(FileWriter writer, int bitness, OpCodeInfoGroup group, OpCodeNode node, OpCodeArgFlags contextFlags, TestArgValues args, bool inElse) {
			if (node.Def is InstructionDef def) {
				if (inElse) {
					writer.WriteLine("*/ {");
					using (writer.Indent())
						GenerateTestsForInstr(writer, bitness, group, def, contextFlags, args);
					writer.WriteLine("}");
				}
				else
					GenerateTestsForInstr(writer, bitness, group, def, contextFlags, args);
			}
			else if (node.Selector is OpCodeSelector selector) {
				var (argKind, maxArgSize) = GetArgInfo(group, selector.ArgIndex);
				var condition = GetArgConditionForOpCodeKind(selector.ArgIndex, argKind, maxArgSize, selector.Kind);
				var isSelectorSupportedByBitness = IsSelectorSupportedByBitness(bitness, selector.Kind, out var continueElse);
				var (contextIfFlags, contextElseFlags) = GetIfElseContextFlags(selector.Kind);
				if (!inElse)
					writer.Write("/* ");
				writer.WriteLine($"if ({condition}) */ {{");
				if (isSelectorSupportedByBitness) {
					using (writer.Indent()) {
						foreach (var argValue in GetArgValue(selector.Kind, false, selector.ArgIndex, group.Signature, maxArgSize * 8)) {
							var oldValue = args.Set(selector.ArgIndex, argValue);
							GenerateTests(writer, bitness, group, selector.IfTrue, contextFlags | contextIfFlags, args, false);
							args.Restore(selector.ArgIndex, oldValue);
						}
					}
				}
				else {
					using (writer.Indent())
						writer.WriteLine($"// skip `if ({condition})` since it's not supported by the current test bitness");
				}
				if (selector.IfFalse.IsEmpty) {
					writer.WriteLine("} /* else */ {");
					if (isSelectorSupportedByBitness && selector.ArgIndex >= 0) {
						var newArg = GetInvalidArgValue(selector.Kind, selector.ArgIndex);
						if (newArg is not null) {
							using (writer.Indent()) {
								int testBitness = GetInvalidTestBitness(bitness, group);
								var oldValue = args.Set(selector.ArgIndex, newArg);
								GenerateTests(writer, testBitness, group, selector.IfTrue,
									contextFlags | contextIfFlags | OpCodeArgFlags.GenerateInvalidTest, args, false);
								args.Restore(selector.ArgIndex, oldValue);
							}
						}
					}
					writer.WriteLine("}");
				}
				else {
					if (continueElse) {
						writer.Write("} /* else ");
						foreach (var argValue in GetArgValue(selector.Kind, true, selector.ArgIndex, group.Signature, maxArgSize * 8)) {
							var oldValue = args.Set(selector.ArgIndex, argValue);
							GenerateTests(writer, bitness, group, selector.IfFalse, contextFlags | contextElseFlags, args, true);
							args.Restore(selector.ArgIndex, oldValue);
						}
					}
					else {
						writer.WriteLine("} /* else */ {");
						using (writer.Indent())
							writer.WriteLine($"// skip `if (!({condition}))` since it's not supported by the current test bitness");
						writer.WriteLine("}");
					}
				}
			}
			else
				throw new InvalidOperationException();
		}

		void GenerateTestsForInstr(FileWriter writer, int bitness, OpCodeInfoGroup group, InstructionDef def, OpCodeArgFlags contextFlags,
			TestArgValues args) {
			if (!IsBitnessSupported(bitness, def.Flags1)) {
				writer.WriteLine($"// Skipping {def.Code.Name(idConverter)} - Not supported by current bitness");
				return;
			}
			writer.WriteLine($"// {def.Code.RawName}");

			var withFns = new List<(string pre, string post)>();
			var asmArgs = new List<string>();
			var withArgs = new List<string>();
			int argBitness = GetArgBitness(bitness, def);
			if (group.HasSpecialInstructionEncoding)
				withArgs.Add(bitness.ToString(CultureInfo.InvariantCulture));
			else
				withArgs.Add(idConverter.ToDeclTypeAndValue(def.Code));
			bool hasLabel = false;
			for (var i = 0; i < args.Args.Count; i++) {
				var argKind = group.Signature.GetArgKind(i);
				if (argKind == ArgKind.Label)
					hasLabel = true;
				var asmArg = args.GetArgValue(argBitness, i)?.AsmStr;
				var withArg = args.GetArgValue(argBitness, i)?.WithStr;

				if (asmArg is null || withArg is null) {
					var argValue = GetDefaultArgument(def.OpKindDefs[group.NumberOfLeadingArgsToDiscard + i], i, argKind, group.MaxArgSizes[i] * 8);
					asmArg = argValue.Get(argBitness).AsmStr;
					withArg = argValue.Get(argBitness).WithStr;
				}

				if ((def.Flags1 & InstructionDefFlags1.OpMaskRegister) != 0 && i == 0) {
					asmArg += ".k1()";
					var opMask = idConverter.ToDeclTypeAndValue(GetRegisterDef(Register.K1).Register);
					withFns.Add(("add_op_mask(", $", {opMask})"));
				}

				asmArgs.Add(asmArg);
				withArgs.Add(withArg);
			}
			int extraArgsCount = 0;
			if (group.ParentPseudoOpsKind is not null) {
				extraArgsCount++;
				withArgs.Add(SignedImmToTestArgValue(group.PseudoOpsKindImmediateValue, 8, 8, 8).WithStr);
			}
			if (group.HasSpecialInstructionEncoding) {
				if (SpecialInstructionHasSegmentArg(group.MnemonicName))
					withArgs.Add(idConverter.ToDeclTypeAndValue(GetRegisterDef(Register.None).Register));
				if ((group.Defs[0].Flags3 & InstructionDefFlags3.IsStringOp) != 0)
					withArgs.Add(idConverter.ToDeclTypeAndValue(genTypes[TypeIds.RepPrefixKind][nameof(RepPrefixKind.None)]));
			}
			if (group.HasLabel && (group.Flags & OpCodeArgFlags.HasLabelUlong) == 0)
				withFns.Add(("assign_label(", $", {withArgs[1]})"));

			var asmName = GetFnName(group);
			var asmArgsStr = string.Join(", ", asmArgs);
			var asmBody = $"a.{asmName}({asmArgsStr});";
			var instrFlags = GetInstrTestFlags(def, group, contextFlags);
			if (instrFlags.Count == 0)
				instrFlags.Add(testInstrFlags[nameof(TestInstrFlags.None)]);
			var testInstrFlagsStr = string.Join(" | ",
				instrFlags.Select(x => $"{x.DeclaringType.Name(idConverter)}::{idConverter.Constant(x.RawName)}"));
			if ((contextFlags & OpCodeArgFlags.GenerateInvalidTest) != 0)
				writer.WriteLine($"test_invalid_instr({bitness}, [](CodeAssembler& a) {{ {asmBody} }}, {testInstrFlagsStr});");
			else {
				if (hasLabel)
					asmBody = $"auto {CreatedLabelName} = create_and_emit_label(a); {asmBody}";
				writer.WriteLine($"test_instr({bitness}, [](CodeAssembler& a) {{ {asmBody} }},");
				using (writer.Indent()) {
					bool returnsResult = true;
					string withFnName;
					if (group.HasSpecialInstructionEncoding)
						withFnName = $"with_{group.MnemonicName.ToLowerInvariant()}";
					else if (group.HasLabel)
						withFnName = "with_branch";
					else {
						if (group.Signature.ArgCount + extraArgsCount == 0)
							returnsResult = false;
						withFnName = GetOverloadedCreateName(group.Signature.ArgCount + extraArgsCount);
					}
					var withArgsStr = string.Join(", ", withArgs);
					var withFnsPreStr = string.Join(string.Empty, ((IEnumerable<(string pre, string post)>)withFns).Reverse().Select(x => x.pre));
					var withFnsPostStr = string.Join(string.Empty, withFns.Select(x => x.post));
					var createExpr = $"Instruction::{withFnName}({withArgsStr})";
					if (returnsResult)
						createExpr = $"unwrap({createExpr})";
					writer.WriteLine($"{withFnsPreStr}{createExpr}{withFnsPostStr},");

					var decOpts = GetDecoderOptions(bitness, def);
					if (decOpts.Count == 0)
						decOpts.Add(decoderOptions[nameof(DecoderOptions.None)]);
					var decOptsStr = string.Join(" | ",
						decOpts.Select(x => $"{x.DeclaringType.Name(idConverter)}::{idConverter.Constant(x.RawName)}"));
					writer.WriteLine($"{testInstrFlagsStr}, {decOptsStr});");
				}
			}
		}

		static string ToHex(ulong value) => "0x" + value.ToString("X", CultureInfo.InvariantCulture);

		protected override TestArgValueBitness MemToTestArgValue(MemorySizeFuncInfo size, int bitness, ulong address) {
			var memName = GetName(size);
			var asmStr = $"{memName}(UINT64_C({ToHex(address)}))";
			var withStr = $"MemoryOperand::with_displ(UINT64_C({ToHex(address)}), {bitness / 8})";
			return new TestArgValueBitness(asmStr, withStr);
		}

		protected override TestArgValueBitness MemToTestArgValue(MemorySizeFuncInfo size, Register @base, Register index, int scale, int displ) {
			if (scale != 1 && scale != 2 && scale != 4 && scale != 8)
				throw new InvalidOperationException();
			if (displ == int.MinValue)
				throw new InvalidOperationException();
			var sb = new StringBuilder();
			sb.Append(GetName(size));
			sb.Append('(');
			bool plus = false;
			if (@base != Register.None) {
				plus = true;
				sb.Append(GetRegisterDef(@base).GetAsmRegisterName());
			}
			if (index != Register.None) {
				if (plus)
					sb.Append('+');
				plus = true;
				sb.Append(GetRegisterDef(index).GetAsmRegisterName());
				if (scale > 1) {
					sb.Append('*');
					sb.Append(scale);
				}
			}
			if (displ != 0) {
				bool isNeg = displ < 0;
				var absDispl = isNeg ? -displ : displ;
				if (plus)
					sb.Append(isNeg ? '-' : '+');
				else if (isNeg)
					throw new InvalidOperationException();
				sb.Append(ToHex((ulong)absDispl));
			}
			sb.Append(')');
			var asmStr = sb.ToString();

			var baseStr = idConverter.ToDeclTypeAndValue(GetRegisterDef(@base).Register);
			var indexStr = idConverter.ToDeclTypeAndValue(GetRegisterDef(index).Register);
			var displStr = displ < 0 ? "-" + ToHex((ulong)(-displ)) : ToHex((ulong)displ);
			var displSize = displ == 0 ? "0" : "1";
			var isBcstStr = size.IsBroadcast ? "true" : "false";
			var regNoneStr = idConverter.ToDeclTypeAndValue(GetRegisterDef(Register.None).Register);
			var withStr = $"MemoryOperand({baseStr}, {indexStr}, {scale}, {displStr}, {displSize}, {isBcstStr}, {regNoneStr})";

			return new(asmStr, withStr);
		}

		protected override TestArgValueBitness RegToTestArgValue(Register register) {
			var regDef = GetRegisterDef(register);
			var asmReg = regDef.GetAsmRegisterName();
			var withReg = idConverter.ToDeclTypeAndValue(regDef.Register);
			return new(asmReg, withReg);
		}

		protected override TestArgValueBitness UnsignedImmToTestArgValue(ulong immediate, int encImmSizeBits, int immSizeBits, int argSizeBits) {
			if (encImmSizeBits > immSizeBits)
				throw new InvalidOperationException();
			var (is64, mask) = immSizeBits switch {
				4 or 8 => (false, byte.MaxValue),
				16 => (false, ushort.MaxValue),
				32 => (false, uint.MaxValue),
				64 => (true, ulong.MaxValue),
				_ => throw new InvalidOperationException(),
			};
			immediate &= mask;
			string numStr;
			if (immediate <= 9)
				numStr = immediate.ToString(CultureInfo.InvariantCulture);
			else
				numStr = ToHex(immediate);

			// `unsigned int` == `std::uint32_t`, UINT64_C() == `std::uint64_t` (`unsigned long long` isn't always `std::uint64_t`)
			var str = is64 ? $"UINT64_C({numStr})" : numStr + "U";
			return new(str, str);
		}

		protected override TestArgValueBitness SignedImmToTestArgValue(long immediate, int encImmSizeBits, int immSizeBits, int argSizeBits) {
			if (encImmSizeBits > immSizeBits)
				throw new InvalidOperationException();
			bool is64 = argSizeBits switch {
				4 or 8 or 16 or 32 => false,
				64 => true,
				_ => throw new InvalidOperationException(),
			};
			string str;
			if (is64) {
				if (immediate == long.MinValue)
					str = "INT64_MIN";
				else
					str = $"INT64_C({SignedToString(immediate)})";
			}
			else {
				if (immediate < int.MinValue || immediate > int.MaxValue)
					throw new InvalidOperationException();
				// `-0x80000000` is an `unsigned int` in C++
				if (immediate == int.MinValue)
					str = "INT32_MIN";
				else
					str = SignedToString(immediate);
			}
			return new(str, str);

			static string SignedToString(long value) {
				bool isNeg = value < 0;
				ulong absValue = isNeg ? (ulong)(-value) : (ulong)value;
				var numStr = absValue <= 9 ? absValue.ToString(CultureInfo.InvariantCulture) : ToHex(absValue);
				return isNeg ? "-" + numStr : numStr;
			}
		}

		protected override TestArgValueBitness LabelToTestArgValue() => new(CreatedLabelName, FirstLabelIdName);
	}
}
