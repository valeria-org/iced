#!/bin/sh
# Measures the observed (runtime) stack usage of the main APIs: each workload runs on a thread whose stack is
# painted with a pattern; the result is the number of bytes from the API call boundary to the deepest written byte
# (library code + the C/C++ runtime functions it calls). Workloads: decode/format/encode/... all instructions of
# libstdc++.so's .text + 1 MB of random bytes (16/32/64-bit).
#
# Usage: run.sh [--rust]    (Linux only; env CXX_LIST="g++ clang++" to select compilers)
set -e
tool_dir=$(cd "$(dirname "$0")" && pwd)
cpp_dir=$(cd "$tool_dir/../.." && pwd)
work_dir=${WORK_DIR:-$tool_dir/work}
mkdir -p "$work_dir"
workloads="decode_one decode instr_info encode_valid encode op_code_info fast fast_string fast_sym gas gas_out gas_sym intel intel_sym masm masm_out masm_sym nasm nasm_sym block_encoder code_asm"
cols=""
for cxx in ${CXX_LIST:-g++ clang++}; do
	for bt in Release Debug; do
		name="$(basename "$cxx")-$bt"
		CXX="$cxx" cmake -S "$cpp_dir" -B "$work_dir/$name" -G Ninja -DCMAKE_BUILD_TYPE=$bt -DICED_X86_BUILD_TESTS=OFF \
			-DICED_X86_BUILD_BENCH=OFF -DICED_X86_BUILD_EXAMPLES=OFF > /dev/null
		cmake --build "$work_dir/$name" > /dev/null
		opt=-O3; [ "$bt" = "Debug" ] && opt=-O0
		"$cxx" -std=c++17 $opt -pthread -I"$cpp_dir/include" "$tool_dir/stack_usage.cpp" "$work_dir/$name/libiced_x86.a" -o "$work_dir/stack-$name"
		cols="$cols $name"
	done
done
printf "%-14s" "bytes"; for c in $cols; do printf " %16s" "$c"; done; echo
for w in $workloads; do
	printf "%-14s" "$w"
	# LD_BIND_NOW: the dynamic linker's lazy symbol binding saves the AVX state on the stack (~2.5 KB), not iced's stack
	for c in $cols; do printf " %16s" "$(LD_BIND_NOW=1 "$work_dir/stack-$c" "$w" | awk '{print $3}')"; done
	echo
done
if [ "$1" = "--rust" ]; then
	(cd "$tool_dir/rust" && CARGO_TARGET_DIR="$work_dir/target" cargo build --release -q)
	echo; echo "Rust (fat LTO): first use (creates the lazy tables) / warm"
	for w in decode instr_info encode fast fast_sym gas gas_sym intel intel_sym masm masm_sym nasm nasm_sym block_encoder code_asm; do
		printf "%-14s %8s %8s\n" "$w" "$("$work_dir/target/release/iced_x86_rust_stack_usage" $w | awk '{print $2}')" \
			"$(WARM=1 "$work_dir/target/release/iced_x86_rust_stack_usage" $w | awk '{print $2}')"
	done
fi
