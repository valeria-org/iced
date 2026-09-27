iced-x86 (C++)
==============

iced-x86 is a blazing fast and correct x86 (16/32/64-bit) instruction decoder, disassembler and assembler.
This is the C++17 port of the [Rust crate](../../rust/iced-x86/README.md). It has the same features and the
API mirrors the Rust API (with `snake_case` names).

- 👍 Supports all Intel and AMD instructions
- 👍 Correct: All instructions are tested and iced has been tested against other disassemblers/assemblers (xed, gas, objdump, masm, dumpbin, nasm, ndisasm) and fuzzed
- 👍 The formatter supports masm, nasm, gas (AT&T), Intel (XED) and there are many options to customize the output
- 👍 Small decoded instructions, only 40 bytes and the decoder doesn't allocate any memory
- 👍 Create instructions with code assembler, eg. `a.mov(eax, edx)`
- 👍 The encoder can be used to re-encode decoded instructions at any address
- 👍 API to get instruction info, eg. read/written registers, memory and rflags bits; CPUID feature flag, control flow info, etc
- 👍 C++17, no dependencies, compiles with `-fno-exceptions -fno-rtti`
- 👍 Embedded friendly: small stack usage, no startup code, all tables are constant data (flash/ROM), the decoder and the fast formatter never allocate memory
- 👍 License: MIT

## Requirements

- A C++17 compiler. Tested with GCC 15 and Clang 21 (x86-64 Linux); the code is standard C++17 and has no platform
  specific code (MSVC should work but isn't tested yet).
- CMake 3.16 or later (or any other build system, see [Using it without CMake](#using-it-without-cmake)).
- Tests only: the whole repository (the tests read the test data in `src/UnitTests/Intel`) and Python 3 (optional,
  verifies that the examples in this README are up to date).

## Quick start

```cpp
#include <cstdio>
#include "iced_x86/iced_x86.hpp"

int main() {
	using namespace iced_x86;
	static const std::uint8_t code[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x48, 0x8D, 0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF};
	Decoder decoder = Decoder::with_ip(64, code, 0x7FF7'1FF3'2800, DecoderOptions::NONE);
	FastFormatter formatter;
	char text[FastFormatter::MAX_FMT_INSTR_LEN + 1];
	for (const Instruction& instr : decoder) {
		formatter.format(instr, text);
		std::printf("%016llX %s\n", static_cast<unsigned long long>(instr.ip()), text);
	}
}
```

```sh
cmake -S src/cpp/iced-x86 -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
c++ -std=c++17 -O2 -Isrc/cpp/iced-x86/include main.cpp build/libiced_x86.a -o main
```

More examples: [Examples](#examples).

## Usage

The library is built with CMake (3.16 or later) and it's a static library. Link with the `iced_x86::iced_x86` target,
it adds the include directory and requires C++17. Include `"iced_x86/iced_x86.hpp"` (everything except the code
assembler) and/or `"iced_x86/code_asm.hpp"` (code assembler), or only the headers you need, eg. `"iced_x86/decoder.hpp"`.
The linker only links the components you use (eg. if you only use the decoder, the formatters aren't linked).

`add_subdirectory()`:

```cmake
add_subdirectory(path/to/iced/src/cpp/iced-x86 iced_x86)
target_link_libraries(my_app PRIVATE iced_x86::iced_x86)
```

`FetchContent` (CMake 3.18 or later):

```cmake
include(FetchContent)
FetchContent_Declare(
	iced_x86
	GIT_REPOSITORY https://github.com/valeria-org/iced.git
	GIT_TAG        <commit-or-tag>
	SOURCE_SUBDIR  src/cpp/iced-x86
)
FetchContent_MakeAvailable(iced_x86)
target_link_libraries(my_app PRIVATE iced_x86::iced_x86)
```

Install it and use `find_package()`:

```sh
cmake -S src/cpp/iced-x86 -B build -DCMAKE_BUILD_TYPE=Release -DICED_X86_BUILD_TESTS=OFF -DICED_X86_BUILD_BENCH=OFF -DICED_X86_BUILD_EXAMPLES=OFF
cmake --build build
cmake --install build --prefix /path/to/install
```

```cmake
find_package(iced_x86 1.21 REQUIRED)    # pass -DCMAKE_PREFIX_PATH=/path/to/install to cmake
target_link_libraries(my_app PRIVATE iced_x86::iced_x86)
```

CMake options:

- `ICED_X86_BUILD_TESTS`: Build the unit tests (default: `ON` if it's the top level project)
- `ICED_X86_BUILD_EXAMPLES`: Build the examples and add a test per example (default: `ON` if it's the top level project)
- `ICED_X86_BUILD_BENCH`: Build the benchmarks (default: `ON` if it's the top level project)
- `ICED_X86_NO_EXCEPTIONS_RTTI`: Compile the library with `-fno-exceptions -fno-rtti` (default: `OFF`: the library
  never uses exceptions or RTTI but it's compiled with the same flags as the rest of the program. If your program
  uses `-fno-exceptions -fno-rtti`, add them to `CMAKE_CXX_FLAGS` instead)
- `ICED_X86_FORMATTER_STRING_SPECIALIZATION`: The gas/intel/masm/nasm formatters get a second, faster copy of their
  formatting code that's used by `format(const Instruction&, std::string&)` (default: `ON`, about 17-20 KB of code per
  formatter; `OFF` = the `std::string` overload uses the generic `FormatterOutput` code, ~8-10% slower)
- `ICED_X86_WARNINGS_AS_ERRORS`: Treat warnings as errors (default: `OFF`)
- `ICED_X86_MAX_FRAME_SIZE`: Warn if a function's stack frame is larger than this many bytes, `0` = disabled (default: `1024`)

### Using it without CMake

The library is plain C++17 with no generated build steps, so any build system works: compile every
`src/cpp/iced-x86/src/**/*.cpp` file with `-Isrc/cpp/iced-x86/include -Isrc/cpp/iced-x86/src` (C++17) and add
`src/cpp/iced-x86/include` to your program's include path. Recommended flags: `-ffunction-sections -fdata-sections`
(link with `-Wl,--gc-sections`) so only the parts you use are linked. Eg. with a Makefile:

```make
ICED_DIR := path/to/iced/src/cpp/iced-x86
ICED_SRCS := $(shell find $(ICED_DIR)/src -name '*.cpp')
ICED_OBJS := $(ICED_SRCS:.cpp=.o)
CXXFLAGS += -std=c++17 -O2 -ffunction-sections -fdata-sections -I$(ICED_DIR)/include

$(ICED_DIR)/src/%.o: $(ICED_DIR)/src/%.cpp
	$(CXX) $(CXXFLAGS) -I$(ICED_DIR)/src -c $< -o $@

libiced_x86.a: $(ICED_OBJS)
	$(AR) rcs $@ $^
```

### Embedded targets / cross compiling

Use your toolchain file as usual (the library has no host specific code or build steps). Recommended settings:

```sh
cmake -S src/cpp/iced-x86 -B build-arm -G Ninja \
	-DCMAKE_TOOLCHAIN_FILE=path/to/your-toolchain.cmake \
	-DCMAKE_BUILD_TYPE=MinSizeRel \
	-DCMAKE_CXX_FLAGS="-fno-exceptions -fno-rtti" \
	-DICED_X86_FORMATTER_STRING_SPECIALIZATION=OFF \
	-DICED_X86_BUILD_TESTS=OFF -DICED_X86_BUILD_EXAMPLES=OFF -DICED_X86_BUILD_BENCH=OFF
cmake --build build-arm      # -> build-arm/libiced_x86.a
```

- `-fno-exceptions -fno-rtti` for the whole program (the library doesn't use exceptions or RTTI; errors are returned
  with `iced_x86::Result<T>`, bugs/invalid arguments call `std::abort()`).
- Keep position dependent code (don't set `CMAKE_POSITION_INDEPENDENT_CODE`): the constant tables then stay in
  `.rodata` (flash/ROM). See *PIC* in [Embedded use](#embedded-use).
- Link with `-Wl,--gc-sections` (the library is compiled with `-ffunction-sections -fdata-sections`).
- `ICED_X86_FORMATTER_STRING_SPECIALIZATION=OFF` saves ~17-20 KB of code per gas/intel/masm/nasm formatter.
- Stack: see [Embedded use](#embedded-use) for the measured stack usage of each API. The library needs no heap
  except: the gas/intel/masm/nasm formatters' state (~100 bytes each), `std::string` output (use the fast formatter
  or your own `FormatterOutput` instead, see the *Embedded: no heap allocations* example), the encoder's output
  buffer (`std::vector`, re-used), `InstructionInfoFactory` (two re-used vectors), the block encoder and the code
  assembler (they create `std::vector`s).

### Regenerating the generated code

Most tables and enums (all files that start with `// ⚠️This file was generated by GENERATOR!🦹‍♂️`) are generated by
`src/csharp/Intel/Generator` (C#, needs the [.NET 10 SDK](https://dotnet.microsoft.com/download)). The generated
files are committed so you only need it if you change the generator or the instruction definitions
(`src/csharp/Intel/Generator/Tables/*.txt`):

```sh
cd src/csharp/Intel/Generator
dotnet run -c Release -- -l cpp     # only the C++ files (no -l = all languages)
```

CI (`build/build-dotnet`) runs the generator and fails if any file changes. `build/build-cpp` builds and tests the
C++ code with GCC and Clang (Release, Debug, MinSizeRel without exceptions/RTTI).

### Tools

- `bench/`: decoder, encoder and formatter benchmarks (`build/bench/iced_x86_bench_formatter [elf_file] [loops]`)
- `tools/rust_diff/run.sh`: differential test against the Rust crate (needs `cargo`), see [Correctness](#correctness)
- `tools/stack_usage/run.sh [--rust]`: measures the stack usage of each API (Linux), see [Embedded use](#embedded-use)
- [PORTING.md](PORTING.md): design, conventions and internals of the C++ code

## Building and running tests

The unit tests use the test data in `src/UnitTests/Intel` so you need the whole repository.

```sh
cmake -S src/cpp/iced-x86 -B build -G Ninja -DICED_X86_WARNINGS_AS_ERRORS=ON
cmake --build build
cd build && ctest --output-on-failure
```

`ctest` runs the unit tests (`iced_x86_tests`), every example (the test fails if the example's output isn't identical
to `examples/expected/<name>.txt`) and verifies that the example code in this README is up to date. You can also run
the unit tests directly: `./build/iced_x86_tests [-l] [filter...]` (`-l` lists the tests, a filter only runs the tests
whose names contain it, eg. `./build/iced_x86_tests decoder/`).

The examples are in `examples/*.cpp`. The code blocks in the [Examples](#examples) section below are generated
from them, so if you edit an example, run:

```sh
python3 src/cpp/iced-x86/examples/update_readme.py
```

## Design, performance and embedded use

The C++ code is a port of the Rust code: same algorithms, same table driven design and the same data (the tables are
generated by the same generator, `src/csharp/Intel/Generator`, which now also emits C++). `Instruction` has the same
40 byte layout as in Rust.

### Correctness

- All the Rust unit tests were ported (26 000+ test cases) and they read the same test data files (`src/UnitTests/Intel`).
- `tools/rust_diff/run.sh` is a differential test: it decodes the same random bytes with the Rust crate and with the
  C++ library (16/32/64-bit, several decoder options) and compares everything per instruction: length, `Code`,
  decoder error, the output of all 5 formatters, used registers/memory, flow control, CPUID features, rflags, stack
  pointer increment and the re-encoded bytes. ~20 million instructions were compared with no differences.
- The tests are also run with ASan/UBSan and the library builds warning free (`-Wall -Wextra -Werror`) with GCC and Clang.

### Performance

Measured on the same machine (x86-64, one pinned core), decoding (and formatting) the whole `.text` section of
`libstdc++.so` (1.38 MB, ~330 K instructions) 20 times. Rust: iced-x86 1.21 with `lto = true, codegen-units = 1`
(the strongest Rust config) and a default `cargo --release` build. C++: GCC 15, `-O3` (CMake `Release`), no LTO.
Formatters write to a `std::string` / `String` (the C++ fast formatter: see below). Lower is better.

| Benchmark (20 x 1.38 MB, ~330 K instructions) | Rust (LTO) | Rust (default) | C++ (GCC 15) |
|---|---:|---:|---:|
| Decode | 0.064 s | 0.066 s | 0.062 s |
| Decode + fast formatter (C++: `char` buffer; Rust: `String`) | 0.146 s | 0.135 s | 0.114 s |
| Decode + fast formatter (C++: `std::string` wrapper) | | | 0.132 s |
| Decode + fast formatter with hard coded options (`SpecializedFormatter`) | | | 0.101 s |
| Decode + gas formatter | 0.254 s | 0.305 s | 0.242 s |
| Decode + intel formatter | 0.256 s | 0.302 s | 0.243 s |
| Decode + masm formatter | 0.266 s | 0.327 s | 0.262 s |
| Decode + nasm formatter | 0.253 s | 0.313 s | 0.250 s |

(best of 3 alternating runs; decoding runs at ~450 MB/s, ~110 M instructions/s. Clang 21 builds are within a few %.
The C++ fast formatter writes to a caller provided `char` buffer (`format(instruction, char* output, std::size_t size)`);
the `std::string` overload is a convenience wrapper that formats to a buffer and appends it to the string. With hard coded
options (`SpecializedFormatter<TraitOptions>`, `verify_output_has_enough_bytes_left()` = `false`): 0.100 s.
The benchmarks are in `bench/`, eg. `build/bench/iced_x86_bench_formatter [elf_file] [loops]`.)

The encoder (re-encode every decoded instruction) runs at the same speed as Rust (~47 M instructions/s both).

### Embedded use

The library was written for devices with a C++ runtime but a small stack:

- **No exceptions, no RTTI**: errors are returned with `iced_x86::Result<T>`, the library compiles with
  `-fno-exceptions -fno-rtti` (use the same flags for the whole program, see the CMake options).
- **Small stack frames**: no recursion (except `std::sort` in the block encoder), no big local arrays. Every function's
  stack frame is < 1 KB in all builds, including `-O0` (enforced with `-Wframe-larger-than=1024`). Worst case call
  chains (static analysis with GCC's `-fcallgraph-info`, `-O3`, incl. rarely used error/symbol paths, excl. the
  C/C++ runtime and user callbacks):

  | API | Worst case stack usage |
  |-----|-----------------------:|
  | `Decoder::decode*()` | < 0.3 KB |
  | `FastFormatter::format(instr, char* output, size)` (`size > MAX_FMT_INSTR_LEN`, no symbol resolver) | 0.3 KB |
  | `FastFormatter::format(instr, char* output, size)` (`size <= MAX_FMT_INSTR_LEN`, no symbol resolver) | 0.7 KB |
  | `FastFormatter::format(instr, char* output, size)` (with a symbol resolver) | 1 KB |
  | `InstructionInfoFactory::info()` | 0.7 KB |
  | `Encoder::encode()` | 0.75 KB |
  | `GasFormatter/IntelFormatter/MasmFormatter/NasmFormatter::format()` | 1.4 - 1.7 KB |
  | `BlockEncoder::encode()` | 2.5 KB |

  Observed (measured at runtime with `tools/stack_usage/run.sh`: bytes from the API call to the deepest written stack
  byte, incl. the C/C++ runtime functions that are called, while decoding/formatting/encoding all instructions in
  `libstdc++.so`'s `.text` + 1 MB of random bytes as 16/32/64-bit code). The Rust column is the same measurement with
  the Rust crate (fat LTO); "first use" is higher in Rust since it creates its tables (on the heap) the first time:

  | API | GCC `-O3` | Clang `-O3` | GCC `-O0` | Clang `-O0` | Rust (first use / after that) |
  |-----|----------:|------------:|----------:|------------:|------------------------------:|
  | `Decoder::decode*()` | 96 | 88 | 1.8 K | 2.3 K | 3.5 K / 95 |
  | `InstructionInfoFactory::info()` | 672 | 624 | 1.8 K | 2.3 K | 3.5 K / 831 |
  | `Encoder::encode()` (incl. invalid instructions: error messages) | 1.1 K | 808 | 1.8 K | 2.3 K | 3.5 K / 871 |
  | `FastFormatter::format()` to a `char` buffer | 264 | 272 | 1.8 K | 2.3 K | 3.7 K / 3.7 K (`String`) |
  | `FastFormatter::format()` with a symbol resolver | 1.0 K | 936 | 2.4 K | 3.0 K | 3.7 K / 3.7 K |
  | gas/intel/masm/nasm `format()` to a `std::string` | 1.1 - 1.2 K | 0.9 - 1.1 K | 1.8 - 2.0 K | 3.4 - 4.0 K | 9.6 K / 1.0 - 1.2 K |
  | gas/intel/masm/nasm `format()` with a symbol resolver | 1.3 - 1.5 K | 1.0 - 1.5 K | 2.2 - 2.3 K | 3.5 - 4.5 K | 9.6 K / 1.1 - 1.6 K |
  | `BlockEncoder::encode()` (~50 K instructions) | 2.0 K | 1.8 K | 2.9 K | 4.8 K | 3.4 K / 2.3 K |
  | `CodeAssembler` instructions + `assemble()` | 2.5 K | 2.3 K | 3.5 K | 4.5 K | 2.4 K / 2.4 K |

  (The `-O0` numbers are upper bounds: the deepest byte may have been written by library code called outside the
  measured API call, eg. `Decoder` construction. A dynamically linked program's first call to a shared library
  function can use ~2.5 KB more stack in the dynamic linker (lazy binding saves the AVX-512 registers); that's not
  counted (`LD_BIND_NOW=1`), statically linked programs don't have it.)

- **No startup code, no RAM tables**: there are no global constructors, no lazily created tables (no locks) and no
  `.bss`/`.data` tables. All tables are `const` data (in flash/.rodata) and are constant initialized, eg. the
  decoder's ~7900 op code handlers (the Rust crate creates its decoder and formatter tables on the heap the first time
  they're used). Heap usage the first time a component is used:

  | Component | One time heap usage |
  |-----------|--------------------:|
  | Decoder | 0 |
  | Encoder, op code info (`OpCodeInfo`) | 0 |
  | Instruction info | 0 |
  | Fast formatter | 0 (it never allocates memory) |
  | gas/intel/masm/nasm formatters | 0 (+ ~100 bytes per formatter instance, + a 256 byte buffer if `format(instr, std::string&)` is used) |

  The decoder never allocates when decoding. The gas/intel/masm/nasm formatters only append to the output string.
  The fast formatter (`FastFormatter`, `SpecializedFormatter<TraitOptions>`) formats to a caller provided buffer (eg.
  a static or stack buffer of `FastFormatter::MAX_FMT_INSTR_LEN + 1` bytes: 316 bytes) and never allocates memory,
  it has no RAM tables (`.bss`/`.data`) and it doesn't use `std::string` (the `format(instr, std::string&)` overload is
  a header-only convenience wrapper).
- **Only what you use is linked**: it's a static library and each component is in its own object files (+
  `-ffunction-sections -fdata-sections`, link with `-Wl,--gc-sections`). Code + read-only data (incl. the constant
  tables) of statically linked x86-64 test programs (`-Os -fno-exceptions -fno-rtti -no-pie`,
  `ICED_X86_FORMATTER_STRING_SPECIALIZATION=OFF`, incl. the parts of libstdc++ they use):

  | Program | Code + read-only data |
  |---------|----------------------:|
  | Decoder | 378 KB (329 KB of it is the decoder's constant tables) |
  | Decoder + fast formatter | 455 KB |
  | Decoder + instruction info | 461 KB |
  | Decoder + nasm formatter | 528 KB |
  | Decoder + gas formatter | 538 KB |
  | Encoder | 177 KB |
  | Decoder + encoder | 554 KB |
  | Block encoder | 209 KB |
  | Code assembler (a few instructions) | 215 KB |

  The Rust crate (and the C++ port before the tables were made constant) creates the decoder tables on the heap
  (~390 KB of RAM on x86-64): the C++ port trades RAM for flash/ROM. The tables mostly contain pointers so they're
  smaller on 32-bit targets.
- **PIC**: the CMake target doesn't force PIC (it follows `CMAKE_POSITION_INDEPENDENT_CODE`, `OFF` by default for
  static libraries, but note that some toolchains, eg. Ubuntu's GCC, default to `-fPIE`). Use position dependent code
  on embedded targets if possible: with `-fPIC`/`-fPIE`, the constant tables (they contain pointers) are
  placed in `.data.rel.ro` instead of `.rodata` (some linker scripts put `.data.rel.ro` in RAM), and PIE
  executables need a relocation for every pointer in the decoder tables: 780 KB of `.rela.dyn` (x86-64), or ~6 KB
  if linked with `-Wl,-z,pack-relative-relocs` (`DT_RELR`, glibc 2.36+). Link with `-no-pie` or
  `-Wl,-z,pack-relative-relocs` if the file size matters.

### Differences from the Rust crate

- There are no Cargo features. All components are always compiled, the linker removes what you don't use.
  MVEX (Knights Corner) is always supported (enable it with `DecoderOptions::KNC`); Rust needs the `mvex` feature.
  VEX/EVEX/XOP/3DNow! can't be removed (Rust's `no_vex` etc. features).
- Methods of Rust enums are free functions in a namespace, eg. `code_ext::mnemonic(code)`, `register_ext::size(reg)`,
  `memory_size_ext::info(ms)`. `Debug` output of enums: `to_string(value)`. `Display` of an instruction: `to_string(instr)`.
- `Result<T, IcedError>` is `iced_x86::Result<T>`, `Option<T>` is `std::optional<T>`, `&[T]` is a pointer + size (+ convenience overloads).
- The code assembler's instruction methods return `CodeAssembler&` (so they can be chained) and errors are sticky:
  the first error is saved and returned by `assemble()` (`has_error()`, `error()`).
- C++ keywords get a `_` suffix, eg. `a.and_(eax, ecx)`, `UsedRegister::register_()`.
- `encode_slice()` (block encoder) returns the results sorted by block `rip` (same as Rust).
- The fast formatter formats to a `char` buffer (`format(instr, output, output_size)` returns the length, `snprintf()`
  semantics, `MAX_FMT_INSTR_LEN` is public) instead of appending to a `String` (a `std::string&` overload also exists).
  It doesn't allocate memory: if a far branch operand's offset has a symbol, the symbol resolver is called again for the
  offset after the selector has been resolved (Rust copies the first result to the heap).
- Generated code: all `*.hpp`/`*.cpp` files that start with `// ⚠️This file was generated by GENERATOR!🦹‍♂️` are generated
  (`cd src/csharp/Intel/Generator && dotnet run -c Release -- -l cpp`). See [PORTING.md](PORTING.md) for the design
  and the conventions used by the C++ code.

## Examples

- [Disassemble (decode and format instructions)](#disassemble-decode-and-format-instructions)
- [Assemble instructions](#assemble-instructions)
- [Disassemble with a symbol resolver](#disassemble-with-a-symbol-resolver)
- [Disassemble with colorized text](#disassemble-with-colorized-text)
- [Move code in memory (eg. hook a function)](#move-code-in-memory-eg-hook-a-function)
- [Get instruction info, eg. read/written regs/mem, control flow info, etc](#get-instruction-info-eg-readwritten-regsmem-control-flow-info-etc)
- [Get the virtual address of a memory operand](#get-the-virtual-address-of-a-memory-operand)
- [Disassemble old/deprecated CPU instructions](#disassemble-olddeprecated-cpu-instructions)
- [Disassemble as fast as possible](#disassemble-as-fast-as-possible)
- [Create and encode instructions](#create-and-encode-instructions)

Methods that can fail return an `iced_x86::Result<T>` (similar to Rust's `Result<T, IcedError>`): check it with
`is_ok()`/`is_err()` (or `if (result)`, `has_value()`) and then get the value with `value()`, `*result` or `result->member`
or the `IcedError` with `error()` (accessing the value of an error result aborts).
The library never throws exceptions.

### Disassemble (decode and format instructions)

This example uses a `Decoder` and one of the `Formatter`s to decode and format the code,
eg. `GasFormatter`, `IntelFormatter`, `MasmFormatter`, `NasmFormatter`, `SpecializedFormatter<TraitOptions>` (or `FastFormatter`).

File: [`examples/disassemble.cpp`](examples/disassemble.cpp)

<!-- example: disassemble.cpp -->
```cpp
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

static constexpr std::size_t HEXBYTES_COLUMN_BYTE_LENGTH = 10;
static constexpr std::uint32_t EXAMPLE_CODE_BITNESS = 64;
static constexpr std::uint64_t EXAMPLE_CODE_RIP = 0x0000'7FFA'C46A'CDA4;
static const std::uint8_t EXAMPLE_CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D,
	0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05,
	0x18, 0x57, 0x0A, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x85, 0xF0, 0x00, 0x00, 0x00, 0x4C, 0x8B,
	0x05, 0x2F, 0x24, 0x0A, 0x00, 0x48, 0x8D, 0x05, 0x78, 0x7C, 0x04, 0x00, 0x33, 0xFF,
};

/*
This function produces the following output:
00007FFAC46ACDA4 48895C2410           mov       [rsp+10h],rbx
00007FFAC46ACDA9 4889742418           mov       [rsp+18h],rsi
00007FFAC46ACDAE 55                   push      rbp
00007FFAC46ACDAF 57                   push      rdi
00007FFAC46ACDB0 4156                 push      r14
00007FFAC46ACDB2 488DAC2400FFFFFF     lea       rbp,[rsp-100h]
00007FFAC46ACDBA 4881EC00020000       sub       rsp,200h
00007FFAC46ACDC1 488B0518570A00       mov       rax,[rel 7FFA`C475`24E0h]
00007FFAC46ACDC8 4833C4               xor       rax,rsp
00007FFAC46ACDCB 488985F0000000       mov       [rbp+0F0h],rax
00007FFAC46ACDD2 4C8B052F240A00       mov       r8,[rel 7FFA`C474`F208h]
00007FFAC46ACDD9 488D05787C0400       lea       rax,[rel 7FFA`C46F`4A58h]
00007FFAC46ACDE0 33FF                 xor       edi,edi
*/
static void how_to_disassemble() {
	const auto& bytes = EXAMPLE_CODE;
	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, bytes, EXAMPLE_CODE_RIP, DecoderOptions::NONE);

	// Formatters: Masm*, Nasm*, Gas* (AT&T) and Intel* (XED).
	// For fastest code, see `SpecializedFormatter` which is ~3.3x faster. Use it if formatting
	// speed is more important than being able to re-assemble formatted instructions.
	NasmFormatter formatter;

	// Change some options, there are many more
	formatter.options_mut().set_digit_separator("`");
	formatter.options_mut().set_first_operand_char_index(10);

	// format() appends to a std::string (or you can pass in your own FormatterOutput)
	std::string output;

	// Initialize this outside the loop because decode_out() writes to every field
	Instruction instruction;

	// The decoder also has begin()/end() so you could use a range-for loop:
	//      for (const Instruction& instruction : decoder) { /* ... */ }
	// but can_decode()/decode_out() is a little faster:
	while (decoder.can_decode()) {
		// There's also a decode() method that returns an instruction but that also
		// means it copies an instruction (40 bytes):
		//     instruction = decoder.decode();
		decoder.decode_out(instruction);

		// Format the instruction ("disassemble" it)
		output.clear();
		formatter.format(instruction, output);

		// Eg. "00007FFAC46ACDB2 488DAC2400FFFFFF     lea       rbp,[rsp-100h]"
		std::printf("%016" PRIX64 " ", instruction.ip());
		const std::size_t start_index = static_cast<std::size_t>(instruction.ip() - EXAMPLE_CODE_RIP);
		const std::size_t instr_len = instruction.len();
		for (std::size_t i = 0; i < instr_len; i++)
			std::printf("%02X", bytes[start_index + i]);
		for (std::size_t i = instr_len; i < HEXBYTES_COLUMN_BYTE_LENGTH; i++)
			std::printf("  ");
		std::printf(" %s\n", output.c_str());
	}
}

int main() {
	how_to_disassemble();
	return 0;
}
```
<!-- example-end -->

### Assemble instructions

This allows you to easily create instructions (eg. `a.xor_(eax, ecx)`) without having to use the more verbose `Instruction::with*()` functions.

The code assembler isn't included by `"iced_x86/iced_x86.hpp"` (it has thousands of methods), include `"iced_x86/code_asm.hpp"`.

File: [`examples/code_assembler.cpp`](examples/code_assembler.cpp)

<!-- example: code_assembler.cpp -->
```cpp
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "iced_x86/code_asm.hpp"

using namespace iced_x86;
using namespace iced_x86::code_asm;

// Like Rust's assert!(): also checked in release builds (unlike assert())
static void check(bool condition, const char* message) {
	if (!condition) {
		std::fprintf(stderr, "Check failed: %s\n", message);
		std::abort();
	}
}

static Result<void> how_to_use_code_assembler() {
	// You can also call CodeAssembler::create(64) which returns a Result<CodeAssembler>
	// instead of aborting if the bitness is invalid
	CodeAssembler a(64);

	// Anytime you add something to a register (or subtract from it), you create a
	// memory operand. You can also call word_ptr(), dword_bcst() etc to create memory
	// operands.
	static_cast<void>(rax);                 // register
	static_cast<void>(rax + 0);             // memory with no size hint
	static_cast<void>(ptr(rax));            // memory with no size hint
	static_cast<void>(rax + rcx * 4 - 123); // memory with no size hint
	// To create a memory operand with only a displacement or only a base register,
	// you can call one of the memory fns:
	static_cast<void>(qword_ptr(123));  // memory with a qword size hint
	static_cast<void>(dword_bcst(rcx)); // memory (broadcast) with a dword size hint
	// To add a segment override, call the segment methods:
	static_cast<void>(ptr(rax).fs()); // fs:[rax]

	// Each mnemonic is a method. Mnemonics that are C++ keywords get a `_` suffix, eg. xor_(), and_(), int_()
	a.push(rcx);
	// There are a few exceptions where you must append `_<opcount>` to the mnemonic to
	// get the instruction you need:
	a.ret();
	a.ret_1(123);
	// Use byte_ptr(), word_bcst(), etc to force the arg to a memory operand and to add a
	// size hint
	a.xor_(byte_ptr(rdx + r14 * 4 + 123), 0x10);
	// Prefixes are also methods
	a.rep().stosd();
	// Immediates can be any integer type (eg. `int`, `unsigned`, `long long`, `std::uint64_t`):
	a.mov(rax, 0x1234'5678'9ABC'DEF0ULL);

	// Errors are sticky: instead of checking the result of each call, the first error is
	// saved, the following calls are ignored and assemble() returns the error. You can
	// also check it with has_error() and error().

	// Create labels that can be referenced by code
	CodeLabel loop_lbl1 = a.create_label();
	CodeLabel after_loop1 = a.create_label();
	a.mov(ecx, 10);
	a.set_label(loop_lbl1);
	// If needed, a zero-bytes instruction can be used as a label but this is optional
	a.zero_bytes();
	a.dec(ecx);
	a.jp(after_loop1);
	a.jne(loop_lbl1);
	a.set_label(after_loop1);

	// It's possible to reference labels with RIP-relative addressing
	CodeLabel skip_data = a.create_label();
	CodeLabel data = a.create_label();
	a.jmp(skip_data);
	a.set_label(data);
	a.db({0x90, 0xCC, 0xF1, 0x90});
	a.set_label(skip_data);
	a.lea(rax, ptr(data));

	// AVX512 opmasks, {z}, {sae}, {er} and broadcasting are also supported:
	a.vsqrtps(zmm16.k2().z(), dword_bcst(rcx));
	a.vsqrtps(zmm1.k2().z(), zmm23.rd_sae());
	// Sometimes, the encoder doesn't know if you want VEX or EVEX encoding.
	// You can force EVEX globally like so:
	a.set_prefer_vex(false);
	a.vucomiss(xmm31, xmm15.sae());
	a.vucomiss(xmm31, ptr(rcx));
	// or call vex()/evex() to override the encoding option:
	a.evex().vucomiss(xmm31, xmm15.sae());
	a.vex().vucomiss(xmm15, xmm14);

	// Encode all added instructions.
	// Use `assemble_options()` if you must get the address of a label
	auto bytes = a.assemble(0x1234'5678);
	if (!bytes)
		return bytes.error();
	check(bytes->size() == 82, "bytes.size() == 82");
	// If you don't want to encode them, you can get all instructions by calling
	// one of these methods:
	const std::vector<Instruction>& instrs = a.instructions(); // Get a reference to the internal vector
	check(instrs.size() == 20, "instrs.size() == 20");
	std::vector<Instruction> instrs2 = a.take_instructions(); // Take ownership of the vector with all instructions
	check(instrs2.size() == 20, "instrs2.size() == 20");
	check(a.instructions().empty(), "a.instructions().empty()");

	return {};
}

int main() {
	auto result = how_to_use_code_assembler();
	if (!result) {
		std::fprintf(stderr, "Error: %s\n", result.error().message());
		return 1;
	}
	return 0;
}
```
<!-- example-end -->

### Disassemble with a symbol resolver

Creates a custom `SymbolResolver` that is called by a `Formatter`.

File: [`examples/symbol_resolver.cpp`](examples/symbol_resolver.cpp)

<!-- example: symbol_resolver.cpp -->
```cpp
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

class MySymbolResolver final : public SymbolResolver {
public:
	explicit MySymbolResolver(std::unordered_map<std::uint64_t, std::string> map) : map_(std::move(map)) {}

	std::optional<SymbolResult> symbol(const Instruction& /*instruction*/, std::uint32_t /*operand*/,
									   std::optional<std::uint32_t> /*instruction_operand*/, std::uint64_t address,
									   std::uint32_t /*address_size*/) override {
		auto it = map_.find(address);
		if (it == map_.end())
			return std::nullopt;
		// The 'address' arg is the address of the symbol and doesn't have to be identical
		// to the 'address' arg passed to symbol(). If it's different from the input
		// address, the formatter will add +N or -N, eg. '[rax+symbol+123]'
		return SymbolResult::with_str(address, it->second);
	}

private:
	std::unordered_map<std::uint64_t, std::string> map_;
};

static void how_to_resolve_symbols() {
	static const std::uint8_t bytes[] = {0x48, 0x8B, 0x8A, 0xA5, 0x5A, 0xA5, 0x5A};
	Decoder decoder(64, bytes, DecoderOptions::NONE);
	Instruction instr = decoder.decode();

	std::unordered_map<std::uint64_t, std::string> sym_map;
	sym_map.emplace(0x5AA5'5AA5, "my_data");

	std::string output;
	auto resolver = std::make_unique<MySymbolResolver>(std::move(sym_map));
	// Create a formatter that uses our symbol resolver
	MasmFormatter formatter(std::move(resolver), nullptr);

	// This will call the symbol resolver for each immediate / displacement
	// it finds in the instruction.
	formatter.format(instr, output);

	// Prints: mov rcx,[rdx+my_data]
	std::printf("%s\n", output.c_str());
}

int main() {
	how_to_resolve_symbols();
	return 0;
}
```
<!-- example-end -->

### Disassemble with colorized text

Creates a custom `FormatterOutput` that is called by a `Formatter`. It uses ANSI escape sequences to colorize the text.

File: [`examples/colorized_text.cpp`](examples/colorized_text.cpp)

<!-- example: colorized_text.cpp -->
```cpp
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

static constexpr std::uint32_t EXAMPLE_CODE_BITNESS = 64;
static constexpr std::uint64_t EXAMPLE_CODE_RIP = 0x0000'7FFA'C46A'CDA4;
static const std::uint8_t EXAMPLE_CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D,
	0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05,
	0x18, 0x57, 0x0A, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x85, 0xF0, 0x00, 0x00, 0x00, 0x4C, 0x8B,
	0x05, 0x2F, 0x24, 0x0A, 0x00, 0x48, 0x8D, 0x05, 0x78, 0x7C, 0x04, 0x00, 0x33, 0xFF,
};

// Custom formatter output that stores the output in a vector.
class MyFormatterOutput final : public FormatterOutput {
public:
	std::vector<std::pair<std::string, FormatterTextKind>> vec;

	void write(std::string_view text, FormatterTextKind kind) override {
		// This allocates a string. If that's a problem, just call printf() here
		// instead of storing the result in a vector.
		vec.emplace_back(std::string(text), kind);
	}
};

// Returns the ANSI escape sequence of the color to use (same colors as the Rust example
// which uses the `colored` crate)
static const char* get_color(FormatterTextKind kind) {
	switch (kind) {
	case FormatterTextKind::Directive:
	case FormatterTextKind::Keyword:
		return "\x1B[93m"; // bright yellow
	case FormatterTextKind::Prefix:
	case FormatterTextKind::Mnemonic:
		return "\x1B[91m"; // bright red
	case FormatterTextKind::Register:
		return "\x1B[94m"; // bright blue
	case FormatterTextKind::Number:
		return "\x1B[96m"; // bright cyan
	default:
		return "\x1B[37m"; // white
	}
}

static constexpr const char* RESET_COLOR = "\x1B[0m";

static void how_to_colorize_text() {
	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, EXAMPLE_CODE, EXAMPLE_CODE_RIP, DecoderOptions::NONE);

	IntelFormatter formatter;
	formatter.options_mut().set_first_operand_char_index(8);
	MyFormatterOutput output;
	for (const Instruction& instruction : decoder) {
		output.vec.clear();
		// The formatter calls output.write() which will update vec with text/colors
		formatter.format(instruction, output);
		for (const auto& [text, kind] : output.vec)
			std::printf("%s%s%s", get_color(kind), text.c_str(), RESET_COLOR);
		std::printf("\n");
	}
}

int main() {
	how_to_colorize_text();
	return 0;
}
```
<!-- example-end -->

### Move code in memory (eg. hook a function)

Uses instruction info API and the encoder to patch a function to jump to the programmer's function.

File: [`examples/move_code.cpp`](examples/move_code.cpp)

<!-- example: move_code.cpp -->
```cpp
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

// Decodes instructions from some address, then encodes them starting at some
// other address. This can be used to hook a function. You decode enough instructions
// until you have enough bytes to add a JMP instruction that jumps to your code.
// Your code will then conditionally jump to the original code that you re-encoded.
//
// This code uses the BlockEncoder which will help with some things, eg. converting
// short branches to longer branches if the target is too far away.
//
// 64-bit mode also supports RIP relative addressing, but the encoder can't rewrite
// those to use a longer displacement. If any of the moved instructions have RIP
// relative addressing and it tries to access data too far away, the encoder will fail.
// The easiest solution is to use OS alloc functions that allocate memory close to the
// original code (+/-2GB).

static constexpr std::uint32_t EXAMPLE_CODE_BITNESS = 64;
static constexpr std::uint64_t EXAMPLE_CODE_RIP = 0x0000'7FFA'C46A'CDA4;
static const std::uint8_t EXAMPLE_CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D,
	0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05,
	0x18, 0x57, 0x0A, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x85, 0xF0, 0x00, 0x00, 0x00, 0x4C, 0x8B,
	0x05, 0x2F, 0x24, 0x0A, 0x00, 0x48, 0x8D, 0x05, 0x78, 0x7C, 0x04, 0x00, 0x33, 0xFF,
};

static void disassemble(const std::vector<std::uint8_t>& data, std::uint64_t ip) {
	NasmFormatter formatter;
	std::string output;
	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, data, ip, DecoderOptions::NONE);
	for (const Instruction& instruction : decoder) {
		output.clear();
		formatter.format(instruction, output);
		std::printf("%016" PRIX64 " %s\n", instruction.ip(), output.c_str());
	}
	std::printf("\n");
}

/*
This function produces the following output:
Original code:
00007FFAC46ACDA4 mov [rsp+10h],rbx
00007FFAC46ACDA9 mov [rsp+18h],rsi
00007FFAC46ACDAE push rbp
00007FFAC46ACDAF push rdi
00007FFAC46ACDB0 push r14
00007FFAC46ACDB2 lea rbp,[rsp-100h]
00007FFAC46ACDBA sub rsp,200h
00007FFAC46ACDC1 mov rax,[rel 7FFAC47524E0h]
00007FFAC46ACDC8 xor rax,rsp
00007FFAC46ACDCB mov [rbp+0F0h],rax
00007FFAC46ACDD2 mov r8,[rel 7FFAC474F208h]
00007FFAC46ACDD9 lea rax,[rel 7FFAC46F4A58h]
00007FFAC46ACDE0 xor edi,edi

Original + patched code:
00007FFAC46ACDA4 mov rax,123456789ABCDEF0h
00007FFAC46ACDAE jmp rax
00007FFAC46ACDB0 push r14
00007FFAC46ACDB2 lea rbp,[rsp-100h]
00007FFAC46ACDBA sub rsp,200h
00007FFAC46ACDC1 mov rax,[rel 7FFAC47524E0h]
00007FFAC46ACDC8 xor rax,rsp
00007FFAC46ACDCB mov [rbp+0F0h],rax
00007FFAC46ACDD2 mov r8,[rel 7FFAC474F208h]
00007FFAC46ACDD9 lea rax,[rel 7FFAC46F4A58h]
00007FFAC46ACDE0 xor edi,edi

Moved code:
00007FFAC48ACDA4 mov [rsp+10h],rbx
00007FFAC48ACDA9 mov [rsp+18h],rsi
00007FFAC48ACDAE push rbp
00007FFAC48ACDAF push rdi
00007FFAC48ACDB0 jmp 00007FFAC46ACDB0h
*/
static Result<void> how_to_move_code() {
	std::vector<std::uint8_t> example_code(EXAMPLE_CODE, EXAMPLE_CODE + sizeof(EXAMPLE_CODE));
	std::printf("Original code:\n");
	disassemble(example_code, EXAMPLE_CODE_RIP);

	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, example_code, EXAMPLE_CODE_RIP, DecoderOptions::NONE);

	// In 64-bit mode, we need 12 bytes to jump to any address:
	//      mov rax,imm64   // 10
	//      jmp rax         // 2
	// We overwrite rax because it's probably not used by the called function.
	// In 32-bit mode, a normal JMP is just 5 bytes
	const std::uint32_t required_bytes = 10 + 2;
	std::uint32_t total_bytes = 0;
	std::vector<Instruction> orig_instructions;
	for (const Instruction& instr : decoder) {
		orig_instructions.push_back(instr);
		total_bytes += static_cast<std::uint32_t>(instr.len());
		if (instr.is_invalid())
			return IcedError("Found garbage");
		if (total_bytes >= required_bytes)
			break;

		switch (instr.flow_control()) {
		case FlowControl::Next:
			break;

		case FlowControl::UnconditionalBranch:
			if (instr.op0_kind() == OpKind::NearBranch64) {
				[[maybe_unused]] const std::uint64_t target = instr.near_branch_target();
				// You could check if it's just jumping forward a few bytes and follow it
				// but this is a simple example so we'll fail.
			}
			return IcedError("Not supported by this simple example");

		case FlowControl::IndirectBranch:
		case FlowControl::ConditionalBranch:
		case FlowControl::Return:
		case FlowControl::Call:
		case FlowControl::IndirectCall:
		case FlowControl::Interrupt:
		case FlowControl::XbeginXabortXend:
		case FlowControl::Exception:
		default:
			return IcedError("Not supported by this simple example");
		}
	}
	if (total_bytes < required_bytes)
		return IcedError("Not enough bytes!");
	if (orig_instructions.empty())
		return IcedError("No instructions");
	// Create a JMP instruction that branches to the original code, except those instructions
	// that we'll re-encode. We don't need to do it if it already ends in 'ret'
	const Instruction& last_instr = orig_instructions.back();
	const std::uint64_t jmp_back_addr = last_instr.next_ip();
	if (last_instr.flow_control() != FlowControl::Return) {
		auto jmp = Instruction::with_branch(Code::Jmp_rel32_64, jmp_back_addr);
		if (!jmp)
			return jmp.error();
		orig_instructions.push_back(*jmp);
	}

	// Relocate the code to some new location. It can fix short/near branches and
	// convert them to short/near/long forms if needed. This also works even if it's a
	// jrcxz/loop/loopcc instruction which only have short forms.
	//
	// It can currently only fix RIP relative operands if the new location is within 2GB
	// of the target data location.
	//
	// Note that a block is not the same thing as a basic block. A block can contain any
	// number of instructions, including any number of branch instructions. One block
	// should be enough unless you must relocate different blocks to different locations.
	const std::uint64_t relocated_base_address = EXAMPLE_CODE_RIP + 0x20'0000;
	InstructionBlock block(orig_instructions, relocated_base_address);
	// This method can also encode more than one block but that's rarely needed, see above comment.
	auto result = BlockEncoder::encode(decoder.bitness(), block, BlockEncoderOptions::NONE);
	if (!result)
		return result.error();
	const std::vector<std::uint8_t>& new_code = result->code_buffer;

	// Patch the original code. Pretend that we use some OS API to write to memory...
	// We could use the BlockEncoder/Encoder for this but it's easy to do yourself too.
	// This is 'mov rax,imm64; jmp rax'
	constexpr std::uint64_t YOUR_FUNC = 0x1234'5678'9ABC'DEF0; // Address of your code
	example_code[0] = 0x48;                                     // \ 'MOV RAX,imm64'
	example_code[1] = 0xB8;                                     // /
	std::uint64_t v = YOUR_FUNC;
	for (std::size_t i = 2; i < 10; i++) {
		example_code[i] = static_cast<std::uint8_t>(v);
		v >>= 8;
	}
	example_code[10] = 0xFF; // \ JMP RAX
	example_code[11] = 0xE0; // /

	// Disassemble it
	std::printf("Original + patched code:\n");
	disassemble(example_code, EXAMPLE_CODE_RIP);

	// Disassemble the moved code
	std::printf("Moved code:\n");
	disassemble(new_code, relocated_base_address);

	return {};
}

int main() {
	auto result = how_to_move_code();
	if (!result) {
		std::fprintf(stderr, "Error: %s\n", result.error().message());
		return 1;
	}
	return 0;
}
```
<!-- example-end -->

### Get instruction info, eg. read/written regs/mem, control flow info, etc

Shows how to get used registers/memory and other info. It uses `Instruction` methods
and an `InstructionInfoFactory` to get this info.

File: [`examples/instruction_info.cpp`](examples/instruction_info.cpp)

<!-- example: instruction_info.cpp -->
```cpp
#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

static constexpr std::uint32_t EXAMPLE_CODE_BITNESS = 64;
static constexpr std::uint64_t EXAMPLE_CODE_RIP = 0x0000'7FFA'C46A'CDA4;
static const std::uint8_t EXAMPLE_CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x55, 0x57, 0x41, 0x56, 0x48, 0x8D,
	0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05,
	0x18, 0x57, 0x0A, 0x00, 0x48, 0x33, 0xC4, 0x48, 0x89, 0x85, 0xF0, 0x00, 0x00, 0x00, 0x4C, 0x8B,
	0x05, 0x2F, 0x24, 0x0A, 0x00, 0x48, 0x8D, 0x05, 0x78, 0x7C, 0x04, 0x00, 0x33, 0xFF,
};

static std::string flags(std::uint32_t rf) {
	std::string sb;
	auto append = [&sb](const char* s) {
		if (!sb.empty())
			sb += ", ";
		sb += s;
	};

	if ((rf & RflagsBits::OF) != 0)
		append("OF");
	if ((rf & RflagsBits::SF) != 0)
		append("SF");
	if ((rf & RflagsBits::ZF) != 0)
		append("ZF");
	if ((rf & RflagsBits::AF) != 0)
		append("AF");
	if ((rf & RflagsBits::CF) != 0)
		append("CF");
	if ((rf & RflagsBits::PF) != 0)
		append("PF");
	if ((rf & RflagsBits::DF) != 0)
		append("DF");
	if ((rf & RflagsBits::IF) != 0)
		append("IF");
	if ((rf & RflagsBits::AC) != 0)
		append("AC");
	if ((rf & RflagsBits::UIF) != 0)
		append("UIF");
	if (sb.empty())
		sb = "<empty>";
	return sb;
}

/*
This function produces the following output:
00007FFAC46ACDA4 mov [rsp+10h],rbx
    OpCode: o64 89 /r
    Instruction: MOV r/m64, r64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_rm64_r64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 4, size = 1
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_or_mem
    Op1: r64_reg
    Used reg: RSP:Read
    Used reg: RBX:Read
    Used mem: [SS:RSP+0x10;UInt64;Write]
00007FFAC46ACDA9 mov [rsp+18h],rsi
    OpCode: o64 89 /r
    Instruction: MOV r/m64, r64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_rm64_r64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 4, size = 1
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_or_mem
    Op1: r64_reg
    Used reg: RSP:Read
    Used reg: RSI:Read
    Used mem: [SS:RSP+0x18;UInt64;Write]
00007FFAC46ACDAE push rbp
    OpCode: o64 50+ro
    Instruction: PUSH r64
    Encoding: Legacy
    Mnemonic: Push
    Code: Push_r64
    CpuidFeature: X64
    FlowControl: Next
    SP Increment: -8
    Op0Access: Read
    Op0: r64_opcode
    Used reg: RBP:Read
    Used reg: RSP:ReadWrite
    Used mem: [SS:RSP+0xFFFFFFFFFFFFFFF8;UInt64;Write]
00007FFAC46ACDAF push rdi
    OpCode: o64 50+ro
    Instruction: PUSH r64
    Encoding: Legacy
    Mnemonic: Push
    Code: Push_r64
    CpuidFeature: X64
    FlowControl: Next
    SP Increment: -8
    Op0Access: Read
    Op0: r64_opcode
    Used reg: RDI:Read
    Used reg: RSP:ReadWrite
    Used mem: [SS:RSP+0xFFFFFFFFFFFFFFF8;UInt64;Write]
00007FFAC46ACDB0 push r14
    OpCode: o64 50+ro
    Instruction: PUSH r64
    Encoding: Legacy
    Mnemonic: Push
    Code: Push_r64
    CpuidFeature: X64
    FlowControl: Next
    SP Increment: -8
    Op0Access: Read
    Op0: r64_opcode
    Used reg: R14:Read
    Used reg: RSP:ReadWrite
    Used mem: [SS:RSP+0xFFFFFFFFFFFFFFF8;UInt64;Write]
00007FFAC46ACDB2 lea rbp,[rsp-100h]
    OpCode: o64 8D /r
    Instruction: LEA r64, m
    Encoding: Legacy
    Mnemonic: Lea
    Code: Lea_r64_m
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 4, size = 4
    Op0Access: Write
    Op1Access: NoMemAccess
    Op0: r64_reg
    Op1: mem
    Used reg: RBP:Write
    Used reg: RSP:Read
00007FFAC46ACDBA sub rsp,200h
    OpCode: o64 81 /5 id
    Instruction: SUB r/m64, imm32
    Encoding: Legacy
    Mnemonic: Sub
    Code: Sub_rm64_imm32
    CpuidFeature: X64
    FlowControl: Next
    Immediate offset = 3, size = 4
    RFLAGS Written: OF, SF, ZF, AF, CF, PF
    RFLAGS Modified: OF, SF, ZF, AF, CF, PF
    Op0Access: ReadWrite
    Op1Access: Read
    Op0: r64_or_mem
    Op1: imm32sex64
    Used reg: RSP:ReadWrite
00007FFAC46ACDC1 mov rax,[7FFAC47524E0h]
    OpCode: o64 8B /r
    Instruction: MOV r64, r/m64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_r64_rm64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 3, size = 4
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_reg
    Op1: r64_or_mem
    Used reg: RAX:Write
    Used mem: [DS:0x7FFAC47524E0;UInt64;Read]
00007FFAC46ACDC8 xor rax,rsp
    OpCode: o64 33 /r
    Instruction: XOR r64, r/m64
    Encoding: Legacy
    Mnemonic: Xor
    Code: Xor_r64_rm64
    CpuidFeature: X64
    FlowControl: Next
    RFLAGS Written: SF, ZF, PF
    RFLAGS Cleared: OF, CF
    RFLAGS Undefined: AF
    RFLAGS Modified: OF, SF, ZF, AF, CF, PF
    Op0Access: ReadWrite
    Op1Access: Read
    Op0: r64_reg
    Op1: r64_or_mem
    Used reg: RAX:ReadWrite
    Used reg: RSP:Read
00007FFAC46ACDCB mov [rbp+0F0h],rax
    OpCode: o64 89 /r
    Instruction: MOV r/m64, r64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_rm64_r64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 3, size = 4
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_or_mem
    Op1: r64_reg
    Used reg: RBP:Read
    Used reg: RAX:Read
    Used mem: [SS:RBP+0xF0;UInt64;Write]
00007FFAC46ACDD2 mov r8,[7FFAC474F208h]
    OpCode: o64 8B /r
    Instruction: MOV r64, r/m64
    Encoding: Legacy
    Mnemonic: Mov
    Code: Mov_r64_rm64
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 3, size = 4
    Memory size: 8
    Op0Access: Write
    Op1Access: Read
    Op0: r64_reg
    Op1: r64_or_mem
    Used reg: R8:Write
    Used mem: [DS:0x7FFAC474F208;UInt64;Read]
00007FFAC46ACDD9 lea rax,[7FFAC46F4A58h]
    OpCode: o64 8D /r
    Instruction: LEA r64, m
    Encoding: Legacy
    Mnemonic: Lea
    Code: Lea_r64_m
    CpuidFeature: X64
    FlowControl: Next
    Displacement offset = 3, size = 4
    Op0Access: Write
    Op1Access: NoMemAccess
    Op0: r64_reg
    Op1: mem
    Used reg: RAX:Write
00007FFAC46ACDE0 xor edi,edi
    OpCode: o32 33 /r
    Instruction: XOR r32, r/m32
    Encoding: Legacy
    Mnemonic: Xor
    Code: Xor_r32_rm32
    CpuidFeature: INTEL386
    FlowControl: Next
    RFLAGS Cleared: OF, SF, CF
    RFLAGS Set: ZF, PF
    RFLAGS Undefined: AF
    RFLAGS Modified: OF, SF, ZF, AF, CF, PF
    Op0Access: Write
    Op1Access: None
    Op0: r32_reg
    Op1: r32_or_mem
    Used reg: RDI:Write
*/
static void how_to_get_instruction_info() {
	auto decoder = Decoder::with_ip(EXAMPLE_CODE_BITNESS, EXAMPLE_CODE, EXAMPLE_CODE_RIP, DecoderOptions::NONE);

	// Use a factory to create the instruction info if you need register and
	// memory usage. If it's something else, eg. encoding, flags, etc, there
	// are Instruction methods that can be used instead.
	InstructionInfoFactory info_factory;
	Instruction instr;
	while (decoder.can_decode()) {
		decoder.decode_out(instr);

		// Gets offsets in the instruction of the displacement and immediates and their sizes.
		// This can be useful if there are relocations in the binary. The encoder has a similar
		// method. This method must be called after decode() and you must pass in the last
		// instruction decode() returned.
		const ConstantOffsets offsets = decoder.get_constant_offsets(instr);

		// For quick hacks, it's fine to use to_string() to format an instruction,
		// but for real code, use a formatter, eg. MasmFormatter. See other examples.
		std::printf("%016" PRIX64 " %s\n", instr.ip(), to_string(instr).c_str());

		const OpCodeInfo& op_code = instr.op_code();
		// The returned reference is valid until the next info() call
		const InstructionInfo& info = info_factory.info(instr);
		const FpuStackIncrementInfo fpu_info = instr.fpu_stack_increment_info();
		std::printf("    OpCode: %s\n", std::string(op_code.op_code_string()).c_str());
		std::printf("    Instruction: %s\n", std::string(op_code.instruction_string()).c_str());
		std::printf("    Encoding: %s\n", to_string(instr.encoding()));
		std::printf("    Mnemonic: %s\n", to_string(instr.mnemonic()));
		std::printf("    Code: %s\n", to_string(instr.code()));
		std::string cpuid_features;
		for (CpuidFeature cpuid_feature : instr.cpuid_features()) {
			if (!cpuid_features.empty())
				cpuid_features += " and ";
			cpuid_features += to_string(cpuid_feature);
		}
		std::printf("    CpuidFeature: %s\n", cpuid_features.c_str());
		std::printf("    FlowControl: %s\n", to_string(instr.flow_control()));
		if (fpu_info.writes_top()) {
			if (fpu_info.increment() == 0)
				std::printf("    FPU TOP: the instruction overwrites TOP\n");
			else
				std::printf("    FPU TOP inc: %d\n", fpu_info.increment());
			std::printf("    FPU TOP cond write: %s\n", fpu_info.conditional() ? "true" : "false");
		}
		if (offsets.has_displacement())
			std::printf("    Displacement offset = %zu, size = %zu\n", offsets.displacement_offset(), offsets.displacement_size());
		if (offsets.has_immediate())
			std::printf("    Immediate offset = %zu, size = %zu\n", offsets.immediate_offset(), offsets.immediate_size());
		if (offsets.has_immediate2())
			std::printf("    Immediate #2 offset = %zu, size = %zu\n", offsets.immediate_offset2(), offsets.immediate_size2());
		if (instr.is_stack_instruction())
			std::printf("    SP Increment: %d\n", instr.stack_pointer_increment());
		if (instr.condition_code() != ConditionCode::None)
			std::printf("    Condition code: %s\n", to_string(instr.condition_code()));
		if (instr.rflags_read() != RflagsBits::NONE)
			std::printf("    RFLAGS Read: %s\n", flags(instr.rflags_read()).c_str());
		if (instr.rflags_written() != RflagsBits::NONE)
			std::printf("    RFLAGS Written: %s\n", flags(instr.rflags_written()).c_str());
		if (instr.rflags_cleared() != RflagsBits::NONE)
			std::printf("    RFLAGS Cleared: %s\n", flags(instr.rflags_cleared()).c_str());
		if (instr.rflags_set() != RflagsBits::NONE)
			std::printf("    RFLAGS Set: %s\n", flags(instr.rflags_set()).c_str());
		if (instr.rflags_undefined() != RflagsBits::NONE)
			std::printf("    RFLAGS Undefined: %s\n", flags(instr.rflags_undefined()).c_str());
		if (instr.rflags_modified() != RflagsBits::NONE)
			std::printf("    RFLAGS Modified: %s\n", flags(instr.rflags_modified()).c_str());
		const auto op_kinds = instr.op_kinds();
		if (std::any_of(op_kinds.begin(), op_kinds.end(), [](OpKind op_kind) { return op_kind == OpKind::Memory; })) {
			const std::size_t size = memory_size_ext::size(instr.memory_size());
			if (size != 0)
				std::printf("    Memory size: %zu\n", size);
		}
		for (std::uint32_t i = 0; i < instr.op_count(); i++)
			std::printf("    Op%uAccess: %s\n", i, to_string(info.op_access(i)));
		for (std::uint32_t i = 0; i < op_code.op_count(); i++)
			std::printf("    Op%u: %s\n", i, to_string(op_code.op_kind(i)));
		for (const UsedRegister& reg_info : info.used_registers())
			std::printf("    Used reg: %s\n", to_string(reg_info).c_str());
		for (const UsedMemory& mem_info : info.used_memory())
			std::printf("    Used mem: %s\n", to_string(mem_info).c_str());
	}
}

int main() {
	how_to_get_instruction_info();
	return 0;
}
```
<!-- example-end -->

### Get the virtual address of a memory operand

File: [`examples/virtual_address.cpp`](examples/virtual_address.cpp)

<!-- example: virtual_address.cpp -->
```cpp
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

// Like Rust's assert!(): also checked in release builds (unlike assert())
static void check(bool condition, const char* message) {
	if (!condition) {
		std::fprintf(stderr, "Check failed: %s\n", message);
		std::abort();
	}
}

static void how_to_get_virtual_address() {
	// add [rdi+r12*8-5AA5EDCCh],esi
	static const std::uint8_t bytes[] = {0x42, 0x01, 0xB4, 0xE7, 0x34, 0x12, 0x5A, 0xA5};
	Decoder decoder(64, bytes, DecoderOptions::NONE);
	Instruction instr = decoder.decode();

	auto va = instr.virtual_address(
		0, 0, [](Register register_, std::size_t /*element_index*/, std::size_t /*element_size*/) -> std::optional<std::uint64_t> {
			switch (register_) {
			// The base address of ES, CS, SS and DS is always 0 in 64-bit mode
			case Register::ES:
			case Register::CS:
			case Register::SS:
			case Register::DS:
				return 0;
			case Register::RDI:
				return 0x0000'0000'1000'0000;
			case Register::R12:
				return 0x0000'0004'0000'0000;
			default:
				return std::nullopt;
			}
		});
	check(va == 0x0000'001F'B55A'1234, "va == 0x0000'001F'B55A'1234");
}

int main() {
	how_to_get_virtual_address();
	return 0;
}
```
<!-- example-end -->

### Disassemble old/deprecated CPU instructions

File: [`examples/old_instructions.cpp`](examples/old_instructions.cpp)

<!-- example: old_instructions.cpp -->
```cpp
#include <cstdint>
#include <cstdio>
#include <string>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

/*
This function produces the following output:
731E0A03 bndmov bnd1, [eax]
731E0A07 mov tr3, esi
731E0A0A rdshr [eax]
731E0A0D dmint
731E0A0F svdc [eax], cs
731E0A12 cpu_read
731E0A14 pmvzb mm1, [eax]
731E0A17 frinear
731E0A19 altinst
*/
static void how_to_disassemble_old_instrs() {
	// clang-format off
	static const std::uint8_t bytes[] = {
		// bndmov bnd1,[eax]
		0x66, 0x0F, 0x1A, 0x08,
		// mov tr3,esi
		0x0F, 0x26, 0xDE,
		// rdshr [eax]
		0x0F, 0x36, 0x00,
		// dmint
		0x0F, 0x39,
		// svdc [eax],cs
		0x0F, 0x78, 0x08,
		// cpu_read
		0x0F, 0x3D,
		// pmvzb mm1,[eax]
		0x0F, 0x58, 0x08,
		// frinear
		0xDF, 0xFC,
		// altinst
		0x0F, 0x3F,
	};
	// clang-format on

	// Enable decoding of Cyrix/Geode instructions, Centaur ALTINST, MOV to/from TR
	// and MPX instructions.
	// There are other options to enable other instructions such as UMOV, KNC, etc.
	// These are deprecated instructions or only used by old CPUs so they're not
	// enabled by default. Some newer instructions also use the same opcodes as
	// some of these old instructions.
	constexpr std::uint32_t DECODER_OPTIONS =
		DecoderOptions::MPX | DecoderOptions::MOV_TR | DecoderOptions::CYRIX | DecoderOptions::CYRIX_DMI | DecoderOptions::ALTINST;
	auto decoder = Decoder::with_ip(32, bytes, 0x731E'0A03, DECODER_OPTIONS);

	NasmFormatter formatter;
	formatter.options_mut().set_space_after_operand_separator(true);
	std::string output;

	Instruction instruction;
	while (decoder.can_decode()) {
		decoder.decode_out(instruction);

		output.clear();
		formatter.format(instruction, output);

		std::printf("%08X %s\n", instruction.ip32(), output.c_str());
	}
}

int main() {
	how_to_disassemble_old_instrs();
	return 0;
}
```
<!-- example-end -->

### Disassemble as fast as possible

For fastest possible disassembly you should set `ENABLE_DB_DW_DD_DQ` to `false`
and you should also hide `verify_output_has_enough_bytes_left()` and return `false`.
The trait options struct derives from `SpecializedFormatterTraitOptions` and hides the static members it wants to change.

The fast formatters (`SpecializedFormatter<TraitOptions>`, `FastFormatter`) write to a caller provided buffer and never
allocate memory: `std::size_t format(const Instruction&, char* output, std::size_t output_size)` (+ a `char (&)[N]`
overload) writes the NUL terminated formatted instruction and returns its length. If the buffer is too small, the output
is truncated and it returns the length of the whole formatted instruction (same as `snprintf()`). Without a symbol
resolver, a formatted instruction is never longer than `MAX_FMT_INSTR_LEN` chars, so a buffer of
`MAX_FMT_INSTR_LEN + 1` bytes is always big enough (that's also the fastest path). `format(const Instruction&, std::string&)`
is a convenience wrapper that appends to a `std::string`.

File: [`examples/fast_formatter.cpp`](examples/fast_formatter.cpp)

<!-- example: fast_formatter.cpp -->
```cpp
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

// Hide the members of SpecializedFormatterTraitOptions that you want to change
struct MyTraitOptions : SpecializedFormatterTraitOptions {
	// If you never create a db/dw/dd/dq 'instruction', we don't need this feature.
	static constexpr bool ENABLE_DB_DW_DD_DQ = false;
	// For a few percent faster code, you can also hide `verify_output_has_enough_bytes_left()` and return `false`
	// static constexpr bool verify_output_has_enough_bytes_left() noexcept { return false; }
};
using MyFormatter = SpecializedFormatter<MyTraitOptions>;

static void how_to_disassemble_really_fast() {
	// Assume this is a big array and not just one instruction
	static const std::uint8_t bytes[] = {0x62, 0xF2, 0x4F, 0xDD, 0x72, 0x50, 0x01};
	Decoder decoder(64, bytes, DecoderOptions::NONE);

	// The formatter writes to a caller provided buffer (it never allocates memory). If there's no symbol
	// resolver, the formatted instruction is never longer than MAX_FMT_INSTR_LEN chars (+ a NUL char).
	char output[MyFormatter::MAX_FMT_INSTR_LEN + 1];
	Instruction instruction;
	MyFormatter formatter;
	while (decoder.can_decode()) {
		decoder.decode_out(instruction);
		// Returns the length of the formatted instruction, `output` is NUL terminated
		const std::size_t len = formatter.format(instruction, output);
		// do something with 'output' here, eg.:
		std::printf("%s (%zu chars)\n", output, len);
	}

	// If the buffer is too small, the output is truncated (and NUL terminated) and it returns the length of the
	// whole formatted instruction (same as snprintf())
	char small[16];
	const std::size_t len = formatter.format(instruction, small);
	std::printf("%s (%zu chars)\n", small, len);
}

int main() {
	how_to_disassemble_really_fast();
	return 0;
}
```
<!-- example-end -->

Also compile with optimizations (`-DCMAKE_BUILD_TYPE=Release`) and consider enabling LTO (`-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`).

### Embedded: no heap allocations

Decodes and formats instructions without allocating any memory: the decoder, `FastFormatter` (formats to a `char`
buffer) and a gas/intel/masm/nasm formatter that writes to a fixed size buffer (a custom `FormatterOutput`). It counts
all `operator new` calls to verify it.

<!-- example: embedded.cpp -->
```cpp
// Disassembling on an embedded device: no heap allocations, small stack, fixed size buffers.
//
// - The decoder and all tables are constant data: nothing is allocated or initialized at runtime
// - `FastFormatter` formats to a caller provided char buffer and never allocates memory
// - The other formatters (gas/intel/masm/nasm) can write to your own `FormatterOutput`, eg. a fixed size buffer.
//   They allocate ~100 bytes when they're created (their number formatter) but nothing when formatting.
//
// This example counts all calls to `operator new` to show that nothing is allocated while decoding and formatting.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string_view>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

static std::size_t heap_allocations = 0;
void* operator new(std::size_t size) {
	heap_allocations++;
	if (void* p = std::malloc(size != 0 ? size : 1))
		return p;
	std::abort();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

// A `FormatterOutput` that writes to a fixed size char buffer (the text is truncated if it doesn't fit)
class FixedBufferOutput final : public FormatterOutput {
public:
	void clear() noexcept {
		len_ = 0;
		buffer_[0] = '\0';
	}
	const char* c_str() const noexcept { return buffer_; }

	void write(std::string_view text, FormatterTextKind kind) override {
		static_cast<void>(kind);
		const std::size_t n = text.size() < sizeof(buffer_) - 1 - len_ ? text.size() : sizeof(buffer_) - 1 - len_;
		std::memcpy(buffer_ + len_, text.data(), n);
		len_ += n;
		buffer_[len_] = '\0';
	}

private:
	char buffer_[128] = {};
	std::size_t len_ = 0;
};

// The code to disassemble (could also be read from flash, a debug interface, etc.)
static const std::uint8_t CODE[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x48, 0x8D, 0xAC, 0x24, 0x00, 0xFF, 0xFF, 0xFF, 0x48, 0x81,
	0xEC, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05, 0x18, 0x57, 0x0A, 0x00, 0xC5, 0xF8, 0x10, 0x44,
	0x24, 0x20, 0xE8, 0x10, 0x00, 0x00, 0x00, 0x74, 0xF0,
};
static constexpr std::uint64_t CODE_RIP = 0x7FF7'1FF3'2800;

int main() {
	// Everything can be created on the stack or statically: FastFormatter is 32 bytes, Decoder ~300 bytes,
	// MasmFormatter ~400 bytes, and the output buffers are as big as you want them to be. They're static here so
	// they don't use any stack.
	static FastFormatter fast_formatter;
	static MasmFormatter masm_formatter;
	static FixedBufferOutput masm_output;
	static char fast_output[FastFormatter::MAX_FMT_INSTR_LEN + 1];

	// Change some options (they're stored in the formatter, no allocation)
	masm_formatter.options_mut().set_first_operand_char_index(8);
	fast_formatter.options_mut().set_space_after_operand_separator(true);

	const std::size_t allocations_before = heap_allocations;

	Decoder decoder = Decoder::with_ip(64, CODE, CODE_RIP, DecoderOptions::NONE);
	Instruction instruction;
	while (decoder.can_decode()) {
		decoder.decode_out(instruction);

		fast_formatter.format(instruction, fast_output);

		masm_output.clear();
		masm_formatter.format(instruction, masm_output);

		std::printf("%016llX %-40s %s\n", static_cast<unsigned long long>(instruction.ip()), fast_output, masm_output.c_str());
	}

	std::printf("Heap allocations while decoding and formatting: %zu\n", heap_allocations - allocations_before);
	return heap_allocations == allocations_before ? 0 : 1;
}
```
<!-- example-end -->

### Create and encode instructions

NOTE: It's much easier to just use `CodeAssembler`, see the example above.
This example shows how to create instructions without using it.

This example uses a `BlockEncoder` to encode created `Instruction`s.

File: [`examples/encode_instructions.cpp`](examples/encode_instructions.cpp)

<!-- example: encode_instructions.cpp -->
```cpp
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "iced_x86/iced_x86.hpp"

using namespace iced_x86;

/*
This function produces the following output:
00001248FC840000 push    %rbp
00001248FC840001 push    %rdi
00001248FC840002 push    %rsi
00001248FC840003 sub     $0x50,%rsp
00001248FC84000A vzeroupper
00001248FC84000D lea     0x60(%rsp),%rbp
00001248FC840012 mov     %rcx,%rsi
00001248FC840015 lea     -0x38(%rbp),%rdi
00001248FC840019 mov     $0xA,%ecx
00001248FC84001E xor     %eax,%eax
00001248FC840020 rep stos %eax,(%rdi)
00001248FC840022 cmp     $0x12345678,%rsi
00001248FC840029 jne     0x00001248FC84002C
00001248FC84002B nop
00001248FC84002C xor     %r15d,%r15d
00001248FC84002F lea     0x1248FC840037,%r14
00001248FC840036 nop
00001248FC840037 .byte   0x12,0x34,0x56,0x78
*/
static Result<void> how_to_encode_instructions() {
	const std::uint32_t bitness = 64;

	// All created instructions get an IP of 0. The label id is just an IP.
	// The branch instruction's *target* IP should be equal to the IP of the
	// target instruction.
	std::uint64_t label_id = 1;
	auto create_label = [&label_id]() { return label_id++; };
	auto add_label = [](std::uint64_t id, Result<Instruction> instruction) {
		if (instruction)
			instruction->set_ip(id);
		return instruction;
	};

	const std::uint64_t label1 = create_label();

	// Most Instruction::with*() methods return a Result<Instruction> (they fail if the operands are invalid)
	std::vector<Result<Instruction>> results = {
		Instruction::with1(Code::Push_r64, Register::RBP),
		Instruction::with1(Code::Push_r64, Register::RDI),
		Instruction::with1(Code::Push_r64, Register::RSI),
		Instruction::with2(Code::Sub_rm64_imm32, Register::RSP, 0x50),
		Instruction::with(Code::VEX_Vzeroupper),
		Instruction::with2(Code::Lea_r64_m, Register::RBP, MemoryOperand::with_base_displ(Register::RSP, 0x60)),
		Instruction::with2(Code::Mov_r64_rm64, Register::RSI, Register::RCX),
		Instruction::with2(Code::Lea_r64_m, Register::RDI, MemoryOperand::with_base_displ(Register::RBP, -0x38)),
		Instruction::with2(Code::Mov_r32_imm32, Register::ECX, 0x0A),
		Instruction::with2(Code::Xor_r32_rm32, Register::EAX, Register::EAX),
		Instruction::with_rep_stosd(bitness),
		Instruction::with2(Code::Cmp_rm64_imm32, Register::RSI, 0x1234'5678),
		// Create a branch instruction that references label1
		Instruction::with_branch(Code::Jne_rel32_64, label1),
		Instruction::with(Code::Nopd),
		// Add the instruction that is the target of the branch
		add_label(label1, Instruction::with2(Code::Xor_r32_rm32, Register::R15D, Register::R15D)),
	};

	// Create an instruction that accesses some data using an RIP relative memory operand
	const std::uint64_t data1 = create_label();
	results.push_back(Instruction::with2(Code::Lea_r64_m, Register::R14, MemoryOperand::with_base_displ(Register::RIP, static_cast<std::int64_t>(data1))));
	results.push_back(Instruction::with(Code::Nopd));
	static const std::uint8_t raw_data[] = {0x12, 0x34, 0x56, 0x78};
	results.push_back(add_label(data1, Instruction::with_declare_byte(raw_data)));

	std::vector<Instruction> instructions;
	for (const Result<Instruction>& result : results) {
		if (!result)
			return result.error();
		instructions.push_back(*result);
	}

	// Use BlockEncoder to encode a block of instructions. This block can contain any
	// number of branches and any number of instructions. It does support encoding more
	// than one block but it's rarely needed.
	// It uses Encoder to encode all instructions.
	// If the target of a branch is too far away, it can fix it to use a longer branch.
	// This can be disabled by enabling some BlockEncoderOptions flags.
	const std::uint64_t target_rip = 0x0000'1248'FC84'0000;
	InstructionBlock block(instructions, target_rip);
	auto result = BlockEncoder::encode(bitness, block, BlockEncoderOptions::NONE);
	if (!result) {
		std::fprintf(stderr, "Failed to encode it: %s\n", result.error().message());
		return result.error();
	}

	// Now disassemble the encoded instructions. Note that the 'jmp near'
	// instruction was turned into a 'jmp short' instruction because we
	// didn't disable branch optimizations.
	const std::vector<std::uint8_t>& bytes = result->code_buffer;
	std::string output;
	const std::uint8_t* bytes_code = bytes.data();
	const std::size_t bytes_code_len = bytes.size() - sizeof(raw_data);
	const std::uint8_t* bytes_data = bytes.data() + bytes_code_len;
	auto decoder = Decoder::with_ip(bitness, bytes_code, bytes_code_len, target_rip, DecoderOptions::NONE);
	GasFormatter formatter;
	formatter.options_mut().set_first_operand_char_index(8);
	for (const Instruction& instruction : decoder) {
		output.clear();
		formatter.format(instruction, output);
		std::printf("%016" PRIX64 " %s\n", instruction.ip(), output.c_str());
	}
	auto db = Instruction::with_declare_byte(bytes_data, sizeof(raw_data));
	if (!db)
		return db.error();
	output.clear();
	formatter.format(*db, output);
	std::printf("%016" PRIX64 " %s\n", decoder.ip(), output.c_str());
	return {};
}

int main() {
	auto result = how_to_encode_instructions();
	if (!result) {
		std::fprintf(stderr, "Error: %s\n", result.error().message());
		return 1;
	}
	return 0;
}
```
<!-- example-end -->
