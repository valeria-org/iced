// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

#include "code_asm/code_asm_test_utils.hpp"
#include "iced_x86/block_encoder_options.hpp"
#include "iced_x86/decoder.hpp"

#include <cstring>
#include <unordered_set>
#include <vector>

namespace iced_x86::tests::code_asm_tests {

using namespace iced_x86::code_asm;

using Bytes = std::vector<std::uint8_t>;

static Bytes assemble_ok(CodeAssembler& a, std::uint64_t ip) { return unwrap(a.assemble(ip)); }

TEST_CASE("code_asm/create") {
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		CodeAssembler a = unwrap(CodeAssembler::create(bitness));
		CHECK_EQ(a.bitness(), bitness);
		CHECK_EQ(a.instructions().size(), static_cast<std::size_t>(0));
		CHECK(a.prefer_vex());
		CHECK(a.prefer_short_branch());
		CHECK(!a.has_error());
		CHECK(a.error() == nullptr);
		a.set_prefer_vex(false);
		CHECK(!a.prefer_vex());
		CHECK(a.prefer_short_branch());
		a.set_prefer_short_branch(false);
		CHECK(!a.prefer_vex());
		CHECK(!a.prefer_short_branch());

		CodeAssembler a2(bitness);
		CHECK_EQ(a2.bitness(), bitness);
		CHECK(!a2.has_error());
	}
}

TEST_CASE("code_asm/create_invalid_bitness") {
	CHECK(CodeAssembler::create(1).is_err());
	CodeAssembler a(1);
	CHECK(a.has_error());
	a.nop();
	CHECK(a.instructions().empty());
	CHECK(a.assemble(0).is_err());
}

TEST_CASE("code_asm/assemble_no_instrs") {
	CodeAssembler a(64);
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK(bytes.empty());
}

TEST_CASE("code_asm/assemble_keeps_instrs") {
	CodeAssembler a(64);
	a.int3();
	a.nop();
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0xCC, 0x90}));
	CHECK_EQ(a.instructions().size(), static_cast<std::size_t>(2));
	a.rdtsc();
	bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0xCC, 0x90, 0x0F, 0x31}));
}

TEST_CASE("code_asm/test_prefixes") {
	CodeAssembler a(64);
	a.xacquire().lock().add(byte_ptr(rcx), dl);
	a.xrelease().lock().add(byte_ptr(rcx), dl);
	a.lock().add(byte_ptr(rcx), dl);
	a.rep().stosb();
	a.repe().cmpsb();
	a.repz().cmpsb();
	a.repne().cmpsb();
	a.repnz().cmpsb();
	a.bnd().call(rcx);
	a.notrack().call(rcx);
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0xF0, 0xF2, 0x00, 0x11, 0xF0, 0xF3, 0x00, 0x11, 0xF0, 0x00, 0x11, 0xF3, 0xAA, 0xF3, 0xA6, 0xF3, 0xA6, 0xF2,
						   0xA6, 0xF2, 0xA6, 0xF2, 0xFF, 0xD1, 0x3E, 0xFF, 0xD1}));
}

TEST_CASE("code_asm/test_prefixes_dup") {
	CodeAssembler a(64);
	a.xacquire().xacquire().lock().lock().add(byte_ptr(rcx), dl);
	a.xrelease().xrelease().lock().lock().add(byte_ptr(rcx), dl);
	a.lock().lock().add(byte_ptr(rcx), dl);
	a.rep().rep().stosb();
	a.repe().repe().cmpsb();
	a.repz().repz().cmpsb();
	a.repne().repne().cmpsb();
	a.repnz().repnz().cmpsb();
	a.bnd().bnd().call(rcx);
	a.notrack().notrack().call(rcx);
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0xF0, 0xF2, 0x00, 0x11, 0xF0, 0xF3, 0x00, 0x11, 0xF0, 0x00, 0x11, 0xF3, 0xAA, 0xF3, 0xA6, 0xF3, 0xA6, 0xF2,
						   0xA6, 0xF2, 0xA6, 0xF2, 0xFF, 0xD1, 0x3E, 0xFF, 0xD1}));
}

TEST_CASE("code_asm/prefixes_without_instr_fails_assemble") {
	CodeAssembler& (*const set_prefixes[])(CodeAssembler&) = {
		[](CodeAssembler& a) -> CodeAssembler& { return a.xacquire(); },
		[](CodeAssembler& a) -> CodeAssembler& { return a.xrelease(); },
		[](CodeAssembler& a) -> CodeAssembler& { return a.lock(); },
		[](CodeAssembler& a) -> CodeAssembler& { return a.rep(); },
		[](CodeAssembler& a) -> CodeAssembler& { return a.repe(); },
		[](CodeAssembler& a) -> CodeAssembler& { return a.repz(); },
		[](CodeAssembler& a) -> CodeAssembler& { return a.repne(); },
		[](CodeAssembler& a) -> CodeAssembler& { return a.repnz(); },
		[](CodeAssembler& a) -> CodeAssembler& { return a.bnd(); },
		[](CodeAssembler& a) -> CodeAssembler& { return a.notrack(); },
	};
	for (auto set_prefix : set_prefixes) {
		CodeAssembler a(64);
		set_prefix(a);
		CHECK(a.assemble(0x1234'5678'9ABC'DEF0).is_err());
	}
}

TEST_CASE("code_asm/test_instrs_method") {
	CodeAssembler a(64);
	CHECK(a.instructions().empty());
	a.rdtsc();
	CHECK((a.instructions() == std::vector<Instruction>{Instruction::with(Code::Rdtsc)}));
	a.xor_(ecx, 0x1234'5678);
	CHECK((a.instructions() ==
		   std::vector<Instruction>{Instruction::with(Code::Rdtsc), unwrap(Instruction::with2(Code::Xor_rm32_imm32, Register::ECX, 0x1234'5678))}));
}

TEST_CASE("code_asm/test_take_instrs") {
	CodeAssembler a(64);
	std::vector<Instruction> instrs = a.take_instructions();
	CHECK(instrs.empty());
	a.rdtsc();
	a.xor_(ecx, 0x1234'5678);
	instrs = a.take_instructions();
	CHECK((instrs == std::vector<Instruction>{Instruction::with(Code::Rdtsc), unwrap(Instruction::with2(Code::Xor_rm32_imm32, Register::ECX, 0x1234'5678))}));
	CHECK(a.instructions().empty());
}

TEST_CASE("code_asm/test_take_instrs_calls_reset") {
	CodeAssembler a(64);
	a.lock();
	std::vector<Instruction> instrs = a.take_instructions();
	CHECK(instrs.empty());
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK(bytes.empty());
}

TEST_CASE("code_asm/test_reset_clears_instrs") {
	CodeAssembler a(64);
	a.reset();
	CHECK(a.instructions().empty());
	a.nop();
	a.int3();
	CHECK_EQ(a.instructions().size(), static_cast<std::size_t>(2));
	a.reset();
	CHECK(a.instructions().empty());
}

TEST_CASE("code_asm/test_reset_clears_flags") {
	CodeAssembler a(64);
	a.lock();
	a.reset();
	CHECK(a.instructions().empty());
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK(bytes.empty());
}

TEST_CASE("code_asm/test_reset_clears_error") {
	CodeAssembler a(64);
	a.pop(cs);
	CHECK(a.has_error());
	a.reset();
	CHECK(!a.has_error());
	a.nop();
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0x90}));
}

TEST_CASE("code_asm/test_labels") {
	CodeAssembler a(64);
	CodeLabel unused_label1 = a.create_label();
	CodeLabel label1 = a.create_label();
	CodeLabel label2 = a.create_label();
	CodeLabel label3 = a.create_label();
	CodeLabel unused_label2 = a.create_label();
	(void)unused_label1;
	(void)unused_label2;

	a.set_label(label1);
	a.nop();
	a.set_label(label2);
	a.int3();
	a.je(label1);
	a.jb(label2);
	a.jo(label2);
	a.jne(label3);
	a.rdtsc();
	a.set_label(label3);
	a.nop();

	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0x90, 0xCC, 0x74, 0xFC, 0x72, 0xFB, 0x70, 0xF9, 0x75, 0x02, 0x0F, 0x31, 0x90}));
}

TEST_CASE("code_asm/test_set_label_errors") {
	{
		CodeAssembler a(64);
		CodeLabel default_label;
		a.set_label(default_label);
		CHECK(a.has_error());
	}
	{
		CodeAssembler a(64);
		CodeLabel label1 = a.create_label();
		a.set_label(label1);
		a.int3();
		CHECK(!a.has_error());
		a.set_label(label1);
		CHECK(a.has_error());
	}
	{
		CodeAssembler a(64);
		CodeLabel label1 = a.create_label();
		CodeLabel label2 = a.create_label();
		a.set_label(label1);
		CHECK(!a.has_error());
		a.set_label(label2);
		CHECK(a.has_error());
	}
}

TEST_CASE("code_asm/test_label_at_eof") {
	{
		CodeAssembler a(64);
		CodeLabel label1 = a.create_label();
		a.set_label(label1);
		CHECK(!a.has_error());
		CHECK(a.assemble(0x1234'5678'9ABC'DEF0).is_err());
	}
	{
		CodeAssembler a(64);
		a.int3();
		CodeLabel label1 = a.create_label();
		a.set_label(label1);
		CHECK(!a.has_error());
		CHECK(a.assemble(0x1234'5678'9ABC'DEF0).is_err());
	}
	{
		CodeAssembler a(64);
		a.anonymous_label();
		CHECK(!a.has_error());
		CHECK(a.assemble(0x1234'5678'9ABC'DEF0).is_err());
	}
	{
		CodeAssembler a(64);
		(void)unwrap(a.fwd());
		CHECK(a.assemble(0x1234'5678'9ABC'DEF0).is_err());
	}
}

TEST_CASE("code_asm/test_anon_labels") {
	CodeAssembler a(64);
	a.push(rcx);
	a.anonymous_label();
	a.xor_(rcx, rdx);
	CodeLabel anon = unwrap(a.bwd());
	a.je(anon);
	anon = unwrap(a.fwd());
	a.js(anon);
	a.nop();
	a.anonymous_label();
	a.sub(eax, eax);
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0x51, 0x48, 0x31, 0xD1, 0x74, 0xFB, 0x78, 0x01, 0x90, 0x29, 0xC0}));
}

TEST_CASE("code_asm/test_mult_anon_labels_same_instr_fails") {
	CodeAssembler a(64);
	a.anonymous_label();
	CHECK(!a.has_error());
	a.anonymous_label();
	CHECK(a.has_error());
}

TEST_CASE("code_asm/test_bwd_fails_if_no_anon_label") {
	CodeAssembler a(64);
	CHECK(a.bwd().is_err());
}

TEST_CASE("code_asm/test_normal_and_anon_label_error") {
	CodeAssembler a(64);
	CodeLabel label = a.create_label();
	a.nop();
	CodeLabel anon = unwrap(a.fwd());
	a.je(anon);
	a.set_label(label);
	a.anonymous_label();
	CHECK(!a.has_error());
	a.nop();
	CHECK(a.has_error());
}

TEST_CASE("code_asm/test_sticky_error") {
	CodeAssembler a(64);
	a.nop();
	a.pop(cs);
	REQUIRE(a.has_error());
	const IcedError* error = a.error();
	REQUIRE(error != nullptr);
	CHECK(std::strcmp(error->message(), "pop: invalid operands") == 0);
	// Ignored since there's an error
	a.int3();
	CodeLabel lbl = a.create_label();
	a.set_label(lbl);
	CHECK(!lbl.has_instruction_index());
	a.db({1, 2, 3});
	a.nops_with_size(5);
	CHECK_EQ(a.instructions().size(), static_cast<std::size_t>(1));
	// The first error is kept
	CodeLabel invalid_label;
	a.set_label(invalid_label);
	a.lock().db({0});
	CHECK(std::strcmp(a.error()->message(), "pop: invalid operands") == 0);
	CHECK(a.assemble(0).is_err());
	// Clear the pending prefix
	a.reset();
	a.nop();
	a.pop(cs);
	REQUIRE(a.has_error());
	Result<Bytes> result = a.assemble(0);
	REQUIRE(result.is_err());
	CHECK(std::strcmp(result.error().message(), "pop: invalid operands") == 0);
	a.clear_error();
	CHECK(!a.has_error());
	CHECK(a.error() == nullptr);
	a.int3();
	Bytes bytes = assemble_ok(a, 0);
	CHECK((bytes == Bytes{0x90, 0xCC}));
}

static const std::uint8_t DB_TEST_DATA[] = {0x87, 0xEE, 0x07, 0x18, 0x52, 0xF8, 0x1D, 0x6A, 0xBE, 0x81, 0x17, 0x03, 0x5E, 0x2F, 0x71, 0x73,
	0xBD, 0x70, 0xDB, 0x8C, 0x97, 0xAB, 0x23, 0x32, 0xB2, 0xC0, 0x27, 0xAE, 0xB2, 0x25, 0x31, 0x64};
static constexpr std::size_t DB_TEST_DATA_LEN = sizeof(DB_TEST_DATA);

static Bytes db_test_data(std::size_t len) { return Bytes(DB_TEST_DATA, DB_TEST_DATA + len); }

// Reads the i'th little endian element of DB_TEST_DATA
template <typename T>
static T read_elem(std::size_t index) {
	T value;
	std::memcpy(&value, DB_TEST_DATA + index * sizeof(T), sizeof(T));
	return value;
}

TEST_CASE("code_asm/test_db") {
	for (std::size_t len = 0; len <= DB_TEST_DATA_LEN; len++) {
		CodeAssembler a(64);
		a.db(DB_TEST_DATA, len);
		Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
		CHECK(bytes == db_test_data(len));
	}
}

TEST_CASE("code_asm/test_db_i") {
	std::int8_t data[DB_TEST_DATA_LEN];
	for (std::size_t i = 0; i < DB_TEST_DATA_LEN; i++)
		data[i] = static_cast<std::int8_t>(DB_TEST_DATA[i]);
	for (std::size_t len = 0; len <= DB_TEST_DATA_LEN; len++) {
		CodeAssembler a(64);
		a.db_i(data, len);
		Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
		CHECK(bytes == db_test_data(len));
	}
}

template <typename T, typename F>
static void test_data(F add_data) {
	constexpr std::size_t ELEM_SIZE = sizeof(T);
	T tmp[DB_TEST_DATA_LEN / ELEM_SIZE];
	for (std::size_t len = 0; len <= DB_TEST_DATA_LEN / ELEM_SIZE; len++) {
		CodeAssembler a(64);
		for (std::size_t i = 0; i < len; i++)
			tmp[i] = read_elem<T>(i);
		add_data(a, tmp, len);
		Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
		CHECK(bytes == db_test_data(len * ELEM_SIZE));
	}
}

TEST_CASE("code_asm/test_dw") {
	test_data<std::uint16_t>([](CodeAssembler& a, const std::uint16_t* data, std::size_t len) { a.dw(data, len); });
}

TEST_CASE("code_asm/test_dw_i") {
	test_data<std::int16_t>([](CodeAssembler& a, const std::int16_t* data, std::size_t len) { a.dw_i(data, len); });
}

TEST_CASE("code_asm/test_dd") {
	test_data<std::uint32_t>([](CodeAssembler& a, const std::uint32_t* data, std::size_t len) { a.dd(data, len); });
}

TEST_CASE("code_asm/test_dd_i") {
	test_data<std::int32_t>([](CodeAssembler& a, const std::int32_t* data, std::size_t len) { a.dd_i(data, len); });
}

TEST_CASE("code_asm/test_dd_f32") {
	test_data<float>([](CodeAssembler& a, const float* data, std::size_t len) { a.dd_f32(data, len); });
}

TEST_CASE("code_asm/test_dq") {
	test_data<std::uint64_t>([](CodeAssembler& a, const std::uint64_t* data, std::size_t len) { a.dq(data, len); });
}

TEST_CASE("code_asm/test_dq_i") {
	test_data<std::int64_t>([](CodeAssembler& a, const std::int64_t* data, std::size_t len) { a.dq_i(data, len); });
}

TEST_CASE("code_asm/test_dq_f64") {
	test_data<double>([](CodeAssembler& a, const double* data, std::size_t len) { a.dq_f64(data, len); });
}

TEST_CASE("code_asm/test_db_dw_dd_dq_errors") {
	// Each test adds prefixes and then data. It must fail.
	using AddData = void (*)(CodeAssembler&);
	const AddData tests[] = {
		[](CodeAssembler& a) { a.xrelease().db(nullptr, 0); },
		[](CodeAssembler& a) { a.xacquire().db({0}); },
		[](CodeAssembler& a) { a.lock().db_i(nullptr, 0); },
		[](CodeAssembler& a) { a.rep().db_i({0}); },
		[](CodeAssembler& a) { a.repe().dw(nullptr, 0); },
		[](CodeAssembler& a) { a.repne().dw({0}); },
		[](CodeAssembler& a) { a.bnd().dw_i(nullptr, 0); },
		[](CodeAssembler& a) { a.notrack().dw_i({0}); },
		[](CodeAssembler& a) { a.xacquire().dd(nullptr, 0); },
		[](CodeAssembler& a) { a.lock().dd({0}); },
		[](CodeAssembler& a) { a.rep().dd_i(nullptr, 0); },
		[](CodeAssembler& a) { a.repe().dd_i({0}); },
		[](CodeAssembler& a) { a.repne().dd_f32(nullptr, 0); },
		[](CodeAssembler& a) { a.bnd().dd_f32({0.0f}); },
		[](CodeAssembler& a) { a.notrack().dq(nullptr, 0); },
		[](CodeAssembler& a) { a.repz().dq({0}); },
		[](CodeAssembler& a) { a.repnz().dq_i(nullptr, 0); },
		[](CodeAssembler& a) { a.rep().dq_i({0}); },
		[](CodeAssembler& a) { a.repne().dq_f64(nullptr, 0); },
		[](CodeAssembler& a) { a.notrack().dq_f64({0.0}); },
	};
	for (AddData add_data : tests) {
		CodeAssembler a(64);
		add_data(a);
		CHECK(a.has_error());
		CHECK(a.instructions().empty());
	}
}

TEST_CASE("code_asm/nops") {
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		for (std::size_t size = 0; size <= 128; size++) {
			CodeAssembler a(bitness);
			a.nops_with_size(size);
			Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
			REQUIRE_EQ(bytes.size(), size);
			Decoder decoder = unwrap(Decoder::try_new(bitness, bytes.data(), bytes.size(), DecoderOptions::NONE));
			while (decoder.can_decode()) {
				const Instruction instr = decoder.decode();
				switch (instr.code()) {
				case Code::Nopw:
				case Code::Nopd:
				case Code::Nopq:
				case Code::Nop_rm16:
				case Code::Nop_rm32:
				case Code::Nop_rm64:
					break;
				default:
					FAIL(std::string("Unexpected NOP instruction: ") + to_string(instr.code()));
				}
			}
		}
	}
}

TEST_CASE("code_asm/nops_errors") {
	using AddNops = void (*)(CodeAssembler&);
	const AddNops tests[] = {
		[](CodeAssembler& a) { a.xacquire().nops_with_size(0); },
		[](CodeAssembler& a) { a.xrelease().nops_with_size(1); },
		[](CodeAssembler& a) { a.lock().nops_with_size(2); },
		[](CodeAssembler& a) { a.rep().nops_with_size(10); },
		[](CodeAssembler& a) { a.repe().nops_with_size(20); },
		[](CodeAssembler& a) { a.repne().nops_with_size(30); },
		[](CodeAssembler& a) { a.repz().nops_with_size(20); },
		[](CodeAssembler& a) { a.repnz().nops_with_size(30); },
		[](CodeAssembler& a) { a.bnd().nops_with_size(100); },
		[](CodeAssembler& a) { a.notrack().nops_with_size(2000); },
	};
	for (AddNops add_nops : tests) {
		CodeAssembler a(64);
		add_nops(a);
		CHECK(a.has_error());
		CHECK(a.instructions().empty());
	}
}

TEST_CASE("code_asm/invalid_instr_fails") {
	{
		CodeAssembler a(64);
		a.aaa();
		CHECK(!a.has_error());
		CHECK(a.assemble(0x1234'5678'9ABC'DEF0).is_err());
	}
	{
		CodeAssembler a(64);
		a.pop(cs);
		CHECK(a.has_error());
	}
	{
		CodeAssembler a(64);
		a.push(cs);
		CHECK(!a.has_error());
		CHECK(a.assemble(0x1234'5678'9ABC'DEF0).is_err());
	}
}

TEST_CASE("code_asm/add_instruction") {
	CodeAssembler a(64);
	a.nop();
	CodeLabel lbl = a.create_label();
	a.set_label(lbl);
	a.add_instruction(unwrap(Instruction::with1(Code::Pop_r64, Register::RDX)));
	a.int3();
	a.je(lbl);
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0x90, 0x5A, 0xCC, 0x74, 0xFC}));
}

TEST_CASE("code_asm/test_mem_seg_overrides") {
	CodeAssembler a(64);
	a.mov(ptr(rax), cl);
	a.mov(ptr(rax).es(), cl);
	a.mov(ptr(rax).cs(), cl);
	a.mov(ptr(rax).ss(), cl);
	a.mov(ptr(rax).ds(), cl);
	a.mov(ptr(rax).fs(), cl);
	a.mov(ptr(rax).gs(), cl);
	a.mov(ptr(rax), cl);
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0x88, 0x08, 0x26, 0x88, 0x08, 0x2E, 0x88, 0x08, 0x36, 0x88, 0x08, 0x3E, 0x88, 0x08, 0x64, 0x88, 0x08, 0x65,
						   0x88, 0x08, 0x88, 0x08}));
}

TEST_CASE("code_asm/test_mem_op_masks") {
	CodeAssembler a(64);
	a.set_prefer_vex(false);
	a.vmovups(ptr(rax + 0x10), xmm2);
	a.vmovups(ptr(rax + 0x10).k1(), xmm2);
	a.vmovups(ptr(rax + 0x10).k2(), xmm2);
	a.vmovups(ptr(rax + 0x10).k3(), xmm2);
	a.vmovups(ptr(rax + 0x10).k4(), xmm2);
	a.vmovups(ptr(rax + 0x10).k5(), xmm2);
	a.vmovups(ptr(rax + 0x10).k6(), xmm2);
	a.vmovups(ptr(rax + 0x10).k7(), xmm2);
	a.vmovups(ptr(rax + 0x10), xmm2);
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{
						   0x62, 0xF1, 0x7C, 0x08, 0x11, 0x50, 0x01, //
						   0x62, 0xF1, 0x7C, 0x09, 0x11, 0x50, 0x01, //
						   0x62, 0xF1, 0x7C, 0x0A, 0x11, 0x50, 0x01, //
						   0x62, 0xF1, 0x7C, 0x0B, 0x11, 0x50, 0x01, //
						   0x62, 0xF1, 0x7C, 0x0C, 0x11, 0x50, 0x01, //
						   0x62, 0xF1, 0x7C, 0x0D, 0x11, 0x50, 0x01, //
						   0x62, 0xF1, 0x7C, 0x0E, 0x11, 0x50, 0x01, //
						   0x62, 0xF1, 0x7C, 0x0F, 0x11, 0x50, 0x01, //
						   0x62, 0xF1, 0x7C, 0x08, 0x11, 0x50, 0x01, //
					   }));
}

TEST_CASE("code_asm/test_reg_op_masks") {
	CodeAssembler a(64);
	a.set_prefer_vex(false);
	a.vmovups(xmm2, xmm3);
	a.vmovups(xmm2.k1(), xmm3);
	a.vmovups(xmm2.k2(), xmm3);
	a.vmovups(xmm2.k3(), xmm3);
	a.vmovups(xmm2.k4(), xmm3);
	a.vmovups(xmm2.k5(), xmm3);
	a.vmovups(xmm2.k6(), xmm3);
	a.vmovups(xmm2.k7(), xmm3);
	a.vmovups(xmm2.k7().z(), xmm3);
	a.vmovups(xmm2, xmm3);
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{
						   0x62, 0xF1, 0x7C, 0x08, 0x10, 0xD3, //
						   0x62, 0xF1, 0x7C, 0x09, 0x10, 0xD3, //
						   0x62, 0xF1, 0x7C, 0x0A, 0x10, 0xD3, //
						   0x62, 0xF1, 0x7C, 0x0B, 0x10, 0xD3, //
						   0x62, 0xF1, 0x7C, 0x0C, 0x10, 0xD3, //
						   0x62, 0xF1, 0x7C, 0x0D, 0x10, 0xD3, //
						   0x62, 0xF1, 0x7C, 0x0E, 0x10, 0xD3, //
						   0x62, 0xF1, 0x7C, 0x0F, 0x10, 0xD3, //
						   0x62, 0xF1, 0x7C, 0x8F, 0x10, 0xD3, //
						   0x62, 0xF1, 0x7C, 0x08, 0x10, 0xD3, //
					   }));
}

TEST_CASE("code_asm/test_reg_sae_er") {
	CodeAssembler a(64);
	a.set_prefer_vex(false);
	a.vcvttss2si(edx, xmm11);
	a.vcvttss2si(edx, xmm11.sae());
	a.vucomiss(xmm18, xmm3.sae());
	a.vcvtsi2ss(xmm2, xmm6, ebx);
	a.vcvtsi2ss(xmm2, xmm6, ebx.rn_sae());
	a.vcvtsi2ss(xmm2, xmm6, ebx.rd_sae());
	a.vcvtsi2ss(xmm2, xmm6, ebx.ru_sae());
	a.vcvtsi2ss(xmm2, xmm6, ebx.rz_sae());
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{
						   0x62, 0xD1, 0x7E, 0x08, 0x2C, 0xD3, //
						   0x62, 0xD1, 0x7E, 0x18, 0x2C, 0xD3, //
						   0x62, 0xE1, 0x7C, 0x18, 0x2E, 0xD3, //
						   0x62, 0xF1, 0x4E, 0x08, 0x2A, 0xD3, //
						   0x62, 0xF1, 0x4E, 0x18, 0x2A, 0xD3, //
						   0x62, 0xF1, 0x4E, 0x38, 0x2A, 0xD3, //
						   0x62, 0xF1, 0x4E, 0x58, 0x2A, 0xD3, //
						   0x62, 0xF1, 0x4E, 0x78, 0x2A, 0xD3, //
					   }));
}

TEST_CASE("code_asm/test_mem_ops_16") {
	CodeAssembler a(16);
	a.mov(ptr(0x1234), eax);
	a.mov(ax, ptr(0xFEDC));
	a.mov(ptr(bx), cl);
	a.mov(ptr(bp), cl);
	a.mov(ptr(bx + 1), cl);
	a.mov(ptr(bp - 1), cl);
	a.mov(ptr(bx + 0x1234), cl);
	a.mov(ptr(bp - 0x1234), cl);
	a.mov(ptr(bx + si), cl);
	a.mov(ptr(bx + si + 1), cl);
	a.mov(ptr(bx + si - 1), cl);
	a.mov(ptr(bp + di + 0x1234), cl);
	a.mov(ptr(bp + di - 0x1234), cl);
	a.mov(ptr((bp + di - 0x1234) - 10), cl);
	a.mov(ptr((bp + di - 0x1234) + 10), cl);
	Bytes bytes = assemble_ok(a, 0x1234);
	CHECK((bytes == Bytes{
						   0x66, 0xA3, 0x34, 0x12, //
						   0xA1, 0xDC, 0xFE, //
						   0x88, 0x0F, //
						   0x88, 0x4E, 0x00, //
						   0x88, 0x4F, 0x01, //
						   0x88, 0x4E, 0xFF, //
						   0x88, 0x8F, 0x34, 0x12, //
						   0x88, 0x8E, 0xCC, 0xED, //
						   0x88, 0x08, //
						   0x88, 0x48, 0x01, //
						   0x88, 0x48, 0xFF, //
						   0x88, 0x8B, 0x34, 0x12, //
						   0x88, 0x8B, 0xCC, 0xED, //
						   0x88, 0x8B, 0xC2, 0xED, //
						   0x88, 0x8B, 0xD6, 0xED, //
					   }));
}

TEST_CASE("code_asm/test_mem_ops_32") {
	CodeAssembler a(32);
	a.mov(ptr(0x1234'5678), eax);
	a.mov(ax, ptr(0xFEDC'BA98U));
	a.mov(ptr(ecx), cl);
	a.mov(ptr(ecx + 123), cl);
	a.mov(ptr(ecx - 123), cl);
	a.mov(ptr(ecx * 1), cl);
	a.mov(ptr(ecx * 2), cl);
	a.mov(ptr(ecx * 2 + 123), cl);
	a.mov(ptr(ecx * 2 - 123), cl);
	a.mov(ptr(ecx + edx), cl);
	a.mov(ptr(ecx + edx * 1), cl);
	a.mov(ptr(ecx + edx * 2), cl);
	a.mov(ptr(ecx + edx * 4 + 123), cl);
	a.mov(ptr(ecx + edx * 8 - 123), cl);
	a.mov(ptr((ecx + edx * 8 - 123) - 10), cl);
	a.mov(ptr((ecx + edx * 8 - 123) + 10), cl);
	a.mov(ptr(2 * ecx), cl);
	a.vpgatherdd(xmm2.k1(), ptr(ecx + xmm4 * 4 + 1));
	a.vpgatherdd(xmm2.k2(), ptr(xmm4 * 4 + ecx - 1));
	a.vpgatherdd(xmm2.k3(), ptr(xmm4 + ecx + 1));
	a.vpgatherdd(xmm2.k4(), ptr(ecx + xmm4 - 1));
	Bytes bytes = assemble_ok(a, 0x1234'5678);
	CHECK((bytes == Bytes{
						   0xA3, 0x78, 0x56, 0x34, 0x12, //
						   0x66, 0xA1, 0x98, 0xBA, 0xDC, 0xFE, //
						   0x88, 0x09, //
						   0x88, 0x49, 0x7B, //
						   0x88, 0x49, 0x85, //
						   0x88, 0x0C, 0x0D, 0x00, 0x00, 0x00, 0x00, //
						   0x88, 0x0C, 0x4D, 0x00, 0x00, 0x00, 0x00, //
						   0x88, 0x0C, 0x4D, 0x7B, 0x00, 0x00, 0x00, //
						   0x88, 0x0C, 0x4D, 0x85, 0xFF, 0xFF, 0xFF, //
						   0x88, 0x0C, 0x11, //
						   0x88, 0x0C, 0x11, //
						   0x88, 0x0C, 0x51, //
						   0x88, 0x4C, 0x91, 0x7B, //
						   0x88, 0x4C, 0xD1, 0x85, //
						   0x88, 0x8C, 0xD1, 0x7B, 0xFF, 0xFF, 0xFF, //
						   0x88, 0x4C, 0xD1, 0x8F, //
						   0x88, 0x0C, 0x4D, 0x00, 0x00, 0x00, 0x00, //
						   0x62, 0xF2, 0x7D, 0x09, 0x90, 0x94, 0xA1, 0x01, 0x00, 0x00, 0x00, //
						   0x62, 0xF2, 0x7D, 0x0A, 0x90, 0x94, 0xA1, 0xFF, 0xFF, 0xFF, 0xFF, //
						   0x62, 0xF2, 0x7D, 0x0B, 0x90, 0x94, 0x21, 0x01, 0x00, 0x00, 0x00, //
						   0x62, 0xF2, 0x7D, 0x0C, 0x90, 0x94, 0x21, 0xFF, 0xFF, 0xFF, 0xFF, //
					   }));
}

TEST_CASE("code_asm/test_mem_ops_64") {
	CodeAssembler a(64);
	a.mov(ptr(INT64_C(0x1234'5678'9ABC'DEF0)), rax);
	a.mov(eax, ptr(UINT64_C(0xF234'5678'9ABC'DEF1)));
	a.mov(ptr(rcx), cl);
	a.mov(ptr(rcx + 123), cl);
	a.mov(ptr(rcx - 123), cl);
	a.mov(ptr(rcx * 1), cl);
	a.mov(ptr(rcx * 2), cl);
	a.mov(ptr(rcx * 2 + 123), cl);
	a.mov(ptr(rcx * 2 - 123), cl);
	a.mov(ptr(rcx + rdx), cl);
	a.mov(ptr(rcx + rdx * 1), cl);
	a.mov(ptr(rcx + rdx * 2), cl);
	a.mov(ptr(rcx + rdx * 4 + 123), cl);
	a.mov(ptr(rcx + rdx * 8 - 123), cl);
	a.mov(ptr((rcx + rdx * 8 - 123) - 10), cl);
	a.mov(ptr((rcx + rdx * 8 - 123) + 10), cl);
	a.mov(ptr(2 * rcx), cl);
	a.vpgatherdd(xmm2.k1(), ptr(rcx + xmm4 * 4 + 1));
	a.vpgatherdd(xmm2.k2(), ptr(xmm4 * 4 + rcx - 1));
	a.vpgatherdd(xmm2.k3(), ptr(xmm4 + rcx + 1));
	a.vpgatherdd(xmm2.k4(), ptr(rcx + xmm4 - 1));
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{
						   0x48, 0xA3, 0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, //
						   0xA1, 0xF1, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0xF2, //
						   0x88, 0x09, //
						   0x88, 0x49, 0x7B, //
						   0x88, 0x49, 0x85, //
						   0x88, 0x0C, 0x0D, 0x00, 0x00, 0x00, 0x00, //
						   0x88, 0x0C, 0x4D, 0x00, 0x00, 0x00, 0x00, //
						   0x88, 0x0C, 0x4D, 0x7B, 0x00, 0x00, 0x00, //
						   0x88, 0x0C, 0x4D, 0x85, 0xFF, 0xFF, 0xFF, //
						   0x88, 0x0C, 0x11, //
						   0x88, 0x0C, 0x11, //
						   0x88, 0x0C, 0x51, //
						   0x88, 0x4C, 0x91, 0x7B, //
						   0x88, 0x4C, 0xD1, 0x85, //
						   0x88, 0x8C, 0xD1, 0x7B, 0xFF, 0xFF, 0xFF, //
						   0x88, 0x4C, 0xD1, 0x8F, //
						   0x88, 0x0C, 0x4D, 0x00, 0x00, 0x00, 0x00, //
						   0x62, 0xF2, 0x7D, 0x09, 0x90, 0x94, 0xA1, 0x01, 0x00, 0x00, 0x00, //
						   0x62, 0xF2, 0x7D, 0x0A, 0x90, 0x94, 0xA1, 0xFF, 0xFF, 0xFF, 0xFF, //
						   0x62, 0xF2, 0x7D, 0x0B, 0x90, 0x94, 0x21, 0x01, 0x00, 0x00, 0x00, //
						   0x62, 0xF2, 0x7D, 0x0C, 0x90, 0x94, 0x21, 0xFF, 0xFF, 0xFF, 0xFF, //
					   }));
}

TEST_CASE("code_asm/test_label_mem_ops") {
	CodeAssembler a(64);
	CodeLabel lbl1 = a.create_label();
	CodeLabel lbl2 = a.create_label();
	a.nop();
	a.set_label(lbl1);
	a.int1();
	a.lea(rax, ptr(lbl1));
	a.lea(rax, ptr(lbl2));
	a.int3();
	a.set_label(lbl2);
	a.db({0x12, 0x34, 0x56, 0x78});
	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0x90, 0xF1, 0x48, 0x8D, 0x05, 0xF8, 0xFF, 0xFF, 0xFF, 0x48, 0x8D, 0x05, 0x01, 0x00, 0x00, 0x00, 0xCC, 0x12, 0x34,
						   0x56, 0x78}));
}

TEST_CASE("code_asm/test_call_far") {
	test_instr(16, [](CodeAssembler& a) { a.call_far(0x1234, 0x5678); }, unwrap(Instruction::with_far_branch(Code::Call_ptr1616, 0x1234, 0x5678)),
		TestInstrFlags::NONE, DecoderOptions::NONE);
	test_instr(32, [](CodeAssembler& a) { a.call_far(0x1234, 0x5678'9ABC); },
		unwrap(Instruction::with_far_branch(Code::Call_ptr1632, 0x1234, 0x5678'9ABC)), TestInstrFlags::NONE, DecoderOptions::NONE);
}

TEST_CASE("code_asm/test_jmp_far") {
	test_instr(16, [](CodeAssembler& a) { a.jmp_far(0x1234, 0x5678); }, unwrap(Instruction::with_far_branch(Code::Jmp_ptr1616, 0x1234, 0x5678)),
		TestInstrFlags::NONE, DecoderOptions::NONE);
	test_instr(32, [](CodeAssembler& a) { a.jmp_far(0x1234, 0x5678'9ABC); },
		unwrap(Instruction::with_far_branch(Code::Jmp_ptr1632, 0x1234, 0x5678'9ABC)), TestInstrFlags::NONE, DecoderOptions::NONE);
}

TEST_CASE("code_asm/test_xlatb") {
	test_instr(16, [](CodeAssembler& a) { a.xlatb(); },
		unwrap(Instruction::with1(Code::Xlat_m8, MemoryOperand::with_base_index(Register::BX, Register::AL))), TestInstrFlags::NONE,
		DecoderOptions::NONE);
	test_instr(32, [](CodeAssembler& a) { a.xlatb(); },
		unwrap(Instruction::with1(Code::Xlat_m8, MemoryOperand::with_base_index(Register::EBX, Register::AL))), TestInstrFlags::NONE,
		DecoderOptions::NONE);
	test_instr(64, [](CodeAssembler& a) { a.xlatb(); },
		unwrap(Instruction::with1(Code::Xlat_m8, MemoryOperand::with_base_index(Register::RBX, Register::AL))), TestInstrFlags::NONE,
		DecoderOptions::NONE);
}

TEST_CASE("code_asm/test_xbegin_label") {
	test_instr(16,
		[](CodeAssembler& a) {
			CodeLabel lbl = create_and_emit_label(a);
			a.xbegin(lbl);
		},
		assign_label(unwrap(Instruction::with_xbegin(16, FIRST_LABEL_ID)), FIRST_LABEL_ID), TestInstrFlags::BRANCH, DecoderOptions::NONE);
	test_instr(32,
		[](CodeAssembler& a) {
			CodeLabel lbl = create_and_emit_label(a);
			a.xbegin(lbl);
		},
		assign_label(unwrap(Instruction::with_xbegin(32, FIRST_LABEL_ID)), FIRST_LABEL_ID), TestInstrFlags::BRANCH | TestInstrFlags::IGNORE_CODE,
		DecoderOptions::NONE);
	test_instr(64,
		[](CodeAssembler& a) {
			CodeLabel lbl = create_and_emit_label(a);
			a.xbegin(lbl);
		},
		assign_label(unwrap(Instruction::with_xbegin(64, FIRST_LABEL_ID)), FIRST_LABEL_ID), TestInstrFlags::BRANCH | TestInstrFlags::IGNORE_CODE,
		DecoderOptions::NONE);
}

TEST_CASE("code_asm/test_xbegin_offset") {
	test_instr(16, [](CodeAssembler& a) { a.xbegin(UINT64_C(12752)); }, unwrap(Instruction::with_xbegin(16, 12752)), TestInstrFlags::BRANCH_U64,
		DecoderOptions::NONE);
	test_instr(32, [](CodeAssembler& a) { a.xbegin(UINT64_C(12752)); }, unwrap(Instruction::with_xbegin(32, 12752)),
		TestInstrFlags::BRANCH_U64 | TestInstrFlags::IGNORE_CODE, DecoderOptions::NONE);
	test_instr(64, [](CodeAssembler& a) { a.xbegin(UINT64_C(12752)); }, unwrap(Instruction::with_xbegin(64, 12752)),
		TestInstrFlags::BRANCH_U64 | TestInstrFlags::IGNORE_CODE, DecoderOptions::NONE);
}

TEST_CASE("code_asm/test_vex_evex_prefixes") {
	CodeAssembler a(64);

	a.set_prefer_vex(true);
	CHECK(a.prefer_vex());
	a.vaddpd(xmm1, xmm2, xmm3);
	a.vex().vaddpd(xmm1, xmm2, xmm3);
	a.vaddpd(xmm1, xmm2, xmm3);
	a.evex().vaddpd(xmm1, xmm2, xmm3);
	a.vaddpd(xmm1, xmm2, xmm3);
	CHECK(a.prefer_vex());

	a.set_prefer_vex(false);
	CHECK(!a.prefer_vex());
	a.vaddpd(xmm1, xmm2, xmm3);
	a.vex().vaddpd(xmm1, xmm2, xmm3);
	a.vaddpd(xmm1, xmm2, xmm3);
	a.evex().vaddpd(xmm1, xmm2, xmm3);
	a.vaddpd(xmm1, xmm2, xmm3);
	CHECK(!a.prefer_vex());

	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{
						   0xC5, 0xE9, 0x58, 0xCB, //
						   0xC5, 0xE9, 0x58, 0xCB, //
						   0xC5, 0xE9, 0x58, 0xCB, //
						   0x62, 0xF1, 0xED, 0x08, 0x58, 0xCB, //
						   0xC5, 0xE9, 0x58, 0xCB, //
						   0x62, 0xF1, 0xED, 0x08, 0x58, 0xCB, //
						   0xC5, 0xE9, 0x58, 0xCB, //
						   0x62, 0xF1, 0xED, 0x08, 0x58, 0xCB, //
						   0x62, 0xF1, 0xED, 0x08, 0x58, 0xCB, //
						   0x62, 0xF1, 0xED, 0x08, 0x58, 0xCB, //
					   }));
}

TEST_CASE("code_asm/test_zero_bytes") {
	CodeAssembler a(64);

	CodeLabel lblf = a.create_label();
	CodeLabel lbll = a.create_label();
	CodeLabel lbl1 = a.create_label();
	CodeLabel lbl2 = a.create_label();

	a.set_label(lblf);
	a.zero_bytes();

	a.je(lbl1);
	a.je(lbl2);
	a.set_label(lbl1);
	a.zero_bytes();
	a.set_label(lbl2);
	a.nop();
	a.lock().rep().zero_bytes();

	a.set_label(lbll);
	a.zero_bytes();

	Bytes bytes = assemble_ok(a, 0x1234'5678'9ABC'DEF0);
	CHECK((bytes == Bytes{0x74, 0x02, 0x74, 0x00, 0x90}));
}

TEST_CASE("code_asm/test_label_ip") {
	for (std::uint32_t bitness : {16U, 32U, 64U}) {
		CodeAssembler a(bitness);
		CodeLabel label0 = a.create_label();
		CodeLabel label1 = a.create_label();
		a.nop();
		a.nop();
		a.nop();
		a.set_label(label1);
		a.nop();

		CodeAssemblerResult result = unwrap(a.assemble_options(0x1234'5678'9ABC'DEF0, BlockEncoderOptions::RETURN_NEW_INSTRUCTION_OFFSETS));
		const std::uint64_t label1_ip = unwrap(result.label_ip(label1));
		CHECK_EQ(label1_ip, UINT64_C(0x1234'5678'9ABC'DEF3));
		CHECK(result.label_ip(label0).is_err());
		CHECK(result.label_ip(CodeLabel()).is_err());
	}
}

TEST_CASE("code_asm/test_label_ip_no_offsets") {
	CodeAssembler a(64);
	CodeLabel label1 = a.create_label();
	a.nop();
	a.set_label(label1);
	a.nop();
	CodeAssemblerResult result = unwrap(a.assemble_options(0x1234'5678'9ABC'DEF0, BlockEncoderOptions::NONE));
	CHECK(result.label_ip(label1).is_err());
}

TEST_CASE("code_asm/code_label") {
	CodeAssembler a(64);
	CodeLabel lbl1 = a.create_label();
	CodeLabel lbl2 = a.create_label();
	CodeLabel lbl1_copy = lbl1;
	CHECK(CodeLabel().is_empty());
	CHECK(!lbl1.is_empty());
	CHECK_EQ(lbl1.id(), FIRST_LABEL_ID);
	CHECK_EQ(lbl2.id(), FIRST_LABEL_ID + 1);
	CHECK(lbl1 == lbl1_copy);
	CHECK(lbl1 != lbl2);
	CHECK(!lbl1.has_instruction_index());
	a.set_label(lbl1);
	CHECK(lbl1.has_instruction_index());
	// Only the id is compared
	CHECK(lbl1 == lbl1_copy);
	std::unordered_set<CodeLabel> set;
	set.insert(lbl1);
	set.insert(lbl1_copy);
	set.insert(lbl2);
	CHECK_EQ(set.size(), static_cast<std::size_t>(2));
}

TEST_CASE("code_asm/get_register") {
	CHECK(registers::get_gpr8(Register::AL).value() == al);
	CHECK(!registers::get_gpr8(Register::AX).has_value());
	CHECK(get_gpr16(Register::DX).value() == dx);
	CHECK(get_gpr32(Register::R15D).value() == r15d);
	CHECK(get_gpr64(Register::RSP).value() == rsp);
	CHECK(!get_gpr64(Register::EAX).has_value());
	CHECK(get_segment(Register::GS).value() == gs);
	CHECK(get_st(Register::ST7).value() == st7);
	CHECK(get_cr(Register::CR8).value() == cr8);
	CHECK(get_dr(Register::DR7).value() == dr7);
	CHECK(get_tr(Register::TR7).value() == tr7);
	CHECK(get_bnd(Register::BND3).value() == bnd3);
	CHECK(get_k(Register::K7).value() == k7);
	CHECK(get_mm(Register::MM7).value() == mm7);
	CHECK(get_xmm(Register::XMM31).value() == xmm31);
	CHECK(get_ymm(Register::YMM31).value() == ymm31);
	CHECK(get_zmm(Register::ZMM31).value() == zmm31);
	CHECK(get_tmm(Register::TMM7).value() == tmm7);
	CHECK(!get_tmm(Register::None).has_value());
	CHECK(registers::gpr32::get_gpr32(Register::EAX).value() == registers::gpr32::eax);
}

TEST_CASE("code_asm/registers_are_constexpr") {
	static_assert(eax.register_() == Register::EAX, "");
	static_assert(xmm0.k1().z().state().op_mask() == Register::K1, "");
	static_assert(xmm0.k1().z().state().zeroing_masking(), "");
	static_assert(zmm1.rd_sae().state().rounding_control() == RoundingControl::RoundDown, "");
	static_assert(zmm1.sae().state().suppress_all_exceptions(), "");
	static_assert(xmm0 != xmm0.k1(), "");
	static_assert(static_cast<Register>(r8) == Register::R8, "");
	constexpr AsmMemoryOperand mem = dword_ptr(rax + rcx * 4 - 8).fs().k2();
	static_assert(mem.base() == Register::RAX, "");
	static_assert(mem.index() == Register::RCX, "");
	static_assert(mem.scale() == 4, "");
	static_assert(mem.displacement() == -8, "");
	static_assert(mem.size() == MemoryOperandSize::Dword, "");
	static_assert(mem.segment() == Register::FS, "");
	static_assert(mem.state().op_mask() == Register::K2, "");
	static_assert(!mem.is_broadcast(), "");
	static_assert(qword_bcst(rdx + xmm0 * 8 + 123).is_broadcast(), "");
	static_assert(qword_bcst(rdx + xmm0 * 8 + 123).size() == MemoryOperandSize::Qword, "");
	static_assert(ptr(0x1234).is_displacement_only(), "");
	CHECK(true);
}

TEST_CASE("code_asm/mem_operators") {
	CHECK((rax + 0 == AsmMemoryOperand(Register::RAX, Register::None, 1, 0, CodeAsmOpState())));
	CHECK((ptr(rax) == AsmMemoryOperand(Register::RAX, Register::None, 1, 0, CodeAsmOpState())));
	CHECK((rax - 1 == AsmMemoryOperand(Register::RAX, Register::None, 1, -1, CodeAsmOpState())));
	CHECK((5 + ebx == AsmMemoryOperand(Register::EBX, Register::None, 1, 5, CodeAsmOpState())));
	CHECK((bx + si == AsmMemoryOperand(Register::BX, Register::SI, 1, 0, CodeAsmOpState())));
	CHECK((8 * r9 == AsmMemoryOperand(Register::None, Register::R9, 8, 0, CodeAsmOpState())));
	CHECK((r9 * 8 + rdx == AsmMemoryOperand(Register::RDX, Register::R9, 8, 0, CodeAsmOpState())));
	CHECK((rdx + r9 * 8 == AsmMemoryOperand(Register::RDX, Register::R9, 8, 0, CodeAsmOpState())));
	CHECK((ymm1 + rdx == AsmMemoryOperand(Register::RDX, Register::YMM1, 1, 0, CodeAsmOpState())));
	CHECK((edx + zmm2 == AsmMemoryOperand(Register::EDX, Register::ZMM2, 1, 0, CodeAsmOpState())));
	CHECK(((rax + 1) + (rcx * 2 + 3) == AsmMemoryOperand(Register::RAX, Register::RCX, 2, 4, CodeAsmOpState())));
	CHECK((10 + (rax + 1) - 3 == AsmMemoryOperand(Register::RAX, Register::None, 1, 8, CodeAsmOpState())));
	CHECK((rax + UINT64_C(0xFFFFFFFFFFFFFFFF) == AsmMemoryOperand(Register::RAX, Register::None, 1, -1, CodeAsmOpState())));
	CHECK(ptr(0x1234).fs().segment() == Register::FS);
	CHECK(ptr(rax).es().segment() == Register::ES);
	CHECK(ptr(rax).cs().segment() == Register::CS);
	CHECK(ptr(rax).ss().segment() == Register::SS);
	CHECK(ptr(rax).ds().segment() == Register::DS);
	CHECK(ptr(rax).gs().segment() == Register::GS);
	CHECK(ptr(rax).segment() == Register::None);
	CHECK(byte_ptr(rax).size() == MemoryOperandSize::Byte);
	CHECK(word_ptr(rax).size() == MemoryOperandSize::Word);
	CHECK(dword_ptr(rax).size() == MemoryOperandSize::Dword);
	CHECK(qword_ptr(rax).size() == MemoryOperandSize::Qword);
	CHECK(mmword_ptr(rax).size() == MemoryOperandSize::Qword);
	CHECK(tbyte_ptr(rax).size() == MemoryOperandSize::Tbyte);
	CHECK(tword_ptr(rax).size() == MemoryOperandSize::Tbyte);
	CHECK(fword_ptr(rax).size() == MemoryOperandSize::Fword);
	CHECK(oword_ptr(rax).size() == MemoryOperandSize::Xword);
	CHECK(xmmword_ptr(rax).size() == MemoryOperandSize::Xword);
	CHECK(ymmword_ptr(rax).size() == MemoryOperandSize::Yword);
	CHECK(zmmword_ptr(rax).size() == MemoryOperandSize::Zword);
	CHECK(bcst(rax).is_broadcast());
	CHECK(word_bcst(rax).size() == MemoryOperandSize::Word);
	CHECK(dword_bcst(rax).size() == MemoryOperandSize::Dword);
	// ptr() clears the broadcast flag
	CHECK(!ptr(dword_bcst(rax)).is_broadcast());
	CHECK(ptr(Register::RDX).base() == Register::RDX);
	CHECK(ptr(UINT64_C(0xFFFF'FFFF'FFFF'FFFF)).displacement() == -1);
	CodeAssembler a(64);
	CodeLabel lbl = a.create_label();
	CHECK((ptr(lbl) == AsmMemoryOperand(Register::RIP, Register::None, 1, static_cast<std::int64_t>(lbl.id()), CodeAsmOpState())));
}

TEST_CASE("code_asm/doc_example") {
	CodeAssembler a(64);

	a.push(rcx);
	a.ret();
	a.ret_1(123);
	a.xor_(byte_ptr(rdx + r14 * 4 + 123), 0x10);
	a.rep().stosd();
	a.mov(rax, UINT64_C(0x1234'5678'9ABC'DEF0));

	CodeLabel loop_lbl1 = a.create_label();
	CodeLabel after_loop1 = a.create_label();
	a.mov(ecx, 10);
	a.set_label(loop_lbl1);
	a.dec(ecx);
	a.jp(after_loop1);
	a.jne(loop_lbl1);
	a.set_label(after_loop1);

	CodeLabel skip_data = a.create_label();
	CodeLabel data = a.create_label();
	a.jmp(skip_data);
	a.set_label(data);
	a.db({0x90, 0xCC, 0xF1, 0x90});
	a.set_label(skip_data);
	a.lea(rax, ptr(data));

	a.vsqrtps(zmm16.k2().z(), dword_bcst(rcx));
	a.vsqrtps(zmm1.k2().z(), zmm23.rd_sae());
	a.set_prefer_vex(false);
	a.vucomiss(xmm31, xmm15.sae());
	a.vucomiss(xmm31, ptr(rcx));
	a.evex().vucomiss(xmm31, xmm15.sae());
	a.vex().vucomiss(xmm15, xmm14);
	REQUIRE_MSG(!a.has_error(), a.error()->message());

	Bytes bytes = assemble_ok(a, 0x1234'5678);
	CHECK_EQ(bytes.size(), static_cast<std::size_t>(82));
	CHECK_EQ(a.instructions().size(), static_cast<std::size_t>(19));
	std::vector<Instruction> instrs = a.take_instructions();
	CHECK_EQ(instrs.size(), static_cast<std::size_t>(19));
	CHECK(a.instructions().empty());
}

TEST_CASE("code_asm/doc_examples_data") {
	{
		CodeAssembler a(64);
		a.rep().stosq();
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xF3, 0x48, 0xAB, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.db({0x16, 0x85, 0x10, 0xA0, 0xFA, 0x9E, 0x11, 0xEB, 0x97, 0x34, 0x3B, 0x7E, 0xB7, 0x2B, 0x92, 0x63, 0x16, 0x85});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0x16, 0x85, 0x10, 0xA0, 0xFA, 0x9E, 0x11, 0xEB, 0x97, 0x34, 0x3B, 0x7E, 0xB7, 0x2B,
												   0x92, 0x63, 0x16, 0x85, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.db_i({0x16, -0x7B, 0x10, -0x60, -0x06, -0x62, 0x11, -0x15, -0x69, 0x34, 0x3B, 0x7E, -0x49, 0x2B, -0x6E, 0x63, 0x16, -0x7B});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0x16, 0x85, 0x10, 0xA0, 0xFA, 0x9E, 0x11, 0xEB, 0x97, 0x34, 0x3B, 0x7E, 0xB7, 0x2B,
												   0x92, 0x63, 0x16, 0x85, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.dw({0x4068, 0x7956, 0xFA9F, 0x11EB, 0x9467, 0x77FA, 0x747C, 0xD088, 0x7D7E});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0x68, 0x40, 0x56, 0x79, 0x9F, 0xFA, 0xEB, 0x11, 0x67, 0x94, 0xFA, 0x77, 0x7C, 0x74,
												   0x88, 0xD0, 0x7E, 0x7D, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.dw_i({0x4068, 0x7956, -0x0561, 0x11EB, -0x6B99, 0x77FA, 0x747C, -0x2F78, 0x7D7E});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0x68, 0x40, 0x56, 0x79, 0x9F, 0xFA, 0xEB, 0x11, 0x67, 0x94, 0xFA, 0x77, 0x7C, 0x74,
												   0x88, 0xD0, 0x7E, 0x7D, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.dd({0x40687956, 0xFA9F11EB, 0x946777FA, 0x747CD088, 0x7D7E7C58});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0x56, 0x79, 0x68, 0x40, 0xEB, 0x11, 0x9F, 0xFA, 0xFA, 0x77, 0x67, 0x94, 0x88, 0xD0,
												   0x7C, 0x74, 0x58, 0x7C, 0x7E, 0x7D, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.dd_i({0x40687956, -0x0560EE15, -0x6B988806, 0x747CD088, 0x7D7E7C58});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0x56, 0x79, 0x68, 0x40, 0xEB, 0x11, 0x9F, 0xFA, 0xFA, 0x77, 0x67, 0x94, 0x88, 0xD0,
												   0x7C, 0x74, 0x58, 0x7C, 0x7E, 0x7D, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.dd_f32({3.14f, -1234.5678f, 1e12f, -3.14f, 1234.5678f, -1e12f});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0xC3, 0xF5, 0x48, 0x40, 0x2B, 0x52, 0x9A, 0xC4, 0xA5, 0xD4, 0x68, 0x53, 0xC3, 0xF5,
												   0x48, 0xC0, 0x2B, 0x52, 0x9A, 0x44, 0xA5, 0xD4, 0x68, 0xD3, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.dq({0x40687956FA9F11EB, 0x946777FA747CD088, 0x7D7E7C5814C2BA6E});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0xEB, 0x11, 0x9F, 0xFA, 0x56, 0x79, 0x68, 0x40, 0x88, 0xD0, 0x7C, 0x74, 0xFA, 0x77,
												   0x67, 0x94, 0x6E, 0xBA, 0xC2, 0x14, 0x58, 0x7C, 0x7E, 0x7D, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.dq_i({0x40687956FA9F11EB, -0x6B9888058B832F78, 0x7D7E7C5814C2BA6E});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0xEB, 0x11, 0x9F, 0xFA, 0x56, 0x79, 0x68, 0x40, 0x88, 0xD0, 0x7C, 0x74, 0xFA, 0x77,
												   0x67, 0x94, 0x6E, 0xBA, 0xC2, 0x14, 0x58, 0x7C, 0x7E, 0x7D, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.int3();
		a.dq_f64({3.14, -1234.5678, 1e123});
		a.nop();
		CHECK((assemble_ok(a, 0x1234'5678) == Bytes{0xCC, 0x1F, 0x85, 0xEB, 0x51, 0xB8, 0x1E, 0x09, 0x40, 0xAD, 0xFA, 0x5C, 0x6D, 0x45, 0x4A,
												   0x93, 0xC0, 0xF1, 0x72, 0xF8, 0xA5, 0x25, 0x34, 0x78, 0x59, 0x90}));
	}
	{
		CodeAssembler a(64);
		a.nops_with_size(17);
		CHECK_EQ(assemble_ok(a, 0x1234'5678).size(), static_cast<std::size_t>(17));
	}
}

} // namespace iced_x86::tests::code_asm_tests
