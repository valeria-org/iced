#!/bin/sh
# Differential test: decodes the same (random) bytes with the Rust crate and the C++ library and compares
# everything per instruction: length, Code, decoder error, masm/nasm/gas/intel/fast formatter output,
# used registers/memory, flow control, CPUID features, stack pointer increment, rflags and the re-encoded bytes.
#
# Usage: run.sh [size_in_bytes] [seed]     (requires cargo, cmake, a C++17 compiler, python3)
set -e
size=${1:-2000000}
seed=${2:-1}
tool_dir=$(cd "$(dirname "$0")" && pwd)
cpp_dir=$(cd "$tool_dir/../.." && pwd)
work_dir=${WORK_DIR:-$tool_dir/work}
mkdir -p "$work_dir"

cmake -S "$cpp_dir" -B "$work_dir/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DICED_X86_BUILD_TESTS=OFF \
	-DICED_X86_BUILD_BENCH=OFF -DICED_X86_BUILD_EXAMPLES=OFF > /dev/null
cmake --build "$work_dir/build"
${CXX:-c++} -std=c++17 -O2 -I"$cpp_dir/include" "$tool_dir/diff.cpp" "$work_dir/build/libiced_x86.a" -o "$work_dir/diff_cpp"
(cd "$tool_dir/rust" && CARGO_TARGET_DIR="$work_dir/target" cargo build --release -q)

python3 - "$work_dir" "$size" "$seed" <<'PY'
import random, sys
work, size, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
rnd = random.Random(seed)
open(work + '/rand.bin', 'wb').write(bytes(rnd.getrandbits(8) for _ in range(size)))
# Prefix/escape heavy stream (legacy prefixes, REX, 0F/0F38/0F3A, VEX/EVEX/XOP/3DNow!/FPU)
pre = [0x66, 0x67, 0xF2, 0xF3, 0xF0, 0x2E, 0x26, 0x64, 0x65, 0x40, 0x41, 0x48, 0x4C, 0x4F]
esc = [[0x0F], [0x0F, 0x38], [0x0F, 0x3A], [0xC4], [0xC5], [0x62], [0x8F], [0x0F, 0x0F], [0xD8], [0xDF]]
out = bytearray()
while len(out) < size:
    for _ in range(rnd.randint(0, 3)):
        out.append(rnd.choice(pre))
    if rnd.random() < 0.7:
        out += bytes(rnd.choice(esc))
    out += bytes(rnd.getrandbits(8) for _ in range(rnd.randint(1, 12)))
open(work + '/struct.bin', 'wb').write(out)
PY

status=0
for file in rand.bin struct.bin; do
	for bitness in 16 32 64; do
		# NONE, KNC (MVEX), NO_INVALID_CHECK|AMD
		for options in 0 1000000 3; do
			"$work_dir/diff_cpp" "$work_dir/$file" $bitness $options > "$work_dir/cpp.txt"
			"$work_dir/target/release/iced_x86_rust_diff" "$work_dir/$file" $bitness $options > "$work_dir/rust.txt"
			count=$(wc -l < "$work_dir/rust.txt")
			if cmp -s "$work_dir/cpp.txt" "$work_dir/rust.txt"; then
				echo "OK   $file bitness=$bitness options=0x$options ($count instructions)"
			else
				echo "DIFF $file bitness=$bitness options=0x$options"
				diff "$work_dir/cpp.txt" "$work_dir/rust.txt" | head -10
				status=1
			fi
		done
	done
done
exit $status
