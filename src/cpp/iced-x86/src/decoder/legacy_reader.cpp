// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: decoder/table_de/legacy_reader.rs

#include "internal/decoder/handlers.hpp"
#include "internal/decoder/handlers_d3now.hpp"
#include "internal/decoder/handlers_fpu.hpp"
#include "internal/decoder/handlers_legacy.hpp"
#include "internal/decoder/table_de.hpp"

#include <cstdint>

namespace iced_x86::internal {

// The handler creation code is split into several functions (called one after the other, not nested) so the stack frames
// are small enough in unoptimized (Debug) builds.

// Returns nullptr if `kind` is handled by another legacy_create_handlerN() fn
static const OpCodeHandler* legacy_create_handler1(TableDeserializer& deserializer, LegacyOpCodeHandlerKind kind) noexcept {
	Code2 codes2;
	Code3 codes3;
	const OpCodeHandler* handler;
	switch (kind) {
	case LegacyOpCodeHandlerKind::Bitness:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Bitness, {deserializer.read_handler(), deserializer.read_handler()});
		break;
	case LegacyOpCodeHandlerKind::Bitness_DontReadModRM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Bitness_DontReadModRM, {deserializer.read_handler(), deserializer.read_handler()});
		break;
	case LegacyOpCodeHandlerKind::RM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RM, {deserializer.read_handler(), deserializer.read_handler()});
		break;
	case LegacyOpCodeHandlerKind::Options1632_1:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Options1632, {
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_decoder_options()
		});
		break;
	case LegacyOpCodeHandlerKind::Options1632_2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Options1632, {
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_decoder_options(),
			deserializer.read_handler(),
			deserializer.read_decoder_options()
		});
		break;
	case LegacyOpCodeHandlerKind::Options3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Options, {
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_decoder_options()
		});
		break;
	case LegacyOpCodeHandlerKind::Options5:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Options, {
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_decoder_options(),
			deserializer.read_handler(),
			deserializer.read_decoder_options()
		});
		break;
	case LegacyOpCodeHandlerKind::Options_DontReadModRM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Options_DontReadModRM, {
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_decoder_options()
		});
		break;
	case LegacyOpCodeHandlerKind::AnotherTable:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_AnotherTable, {
			deserializer.read_array_reference_no_clone(static_cast<std::uint32_t>(LegacyOpCodeHandlerKind::ArrayReference))
		});
		break;
	case LegacyOpCodeHandlerKind::Group:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Group, {
			deserializer.read_array_reference(static_cast<std::uint32_t>(LegacyOpCodeHandlerKind::ArrayReference))
		});
		break;
	case LegacyOpCodeHandlerKind::Group8x64:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Group8x64, {
			deserializer.read_array_reference(static_cast<std::uint32_t>(LegacyOpCodeHandlerKind::ArrayReference)),
			deserializer.read_array_reference(static_cast<std::uint32_t>(LegacyOpCodeHandlerKind::ArrayReference))
		});
		break;
	case LegacyOpCodeHandlerKind::Group8x8:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Group8x8, {
			deserializer.read_array_reference(static_cast<std::uint32_t>(LegacyOpCodeHandlerKind::ArrayReference)),
			deserializer.read_array_reference(static_cast<std::uint32_t>(LegacyOpCodeHandlerKind::ArrayReference))
		});
		break;
	case LegacyOpCodeHandlerKind::MandatoryPrefix:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MandatoryPrefix, {
			true,
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler()
		});
		break;
	case LegacyOpCodeHandlerKind::MandatoryPrefix4:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MandatoryPrefix4, {
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_u32()
		});
		break;
	case LegacyOpCodeHandlerKind::MandatoryPrefix_NoModRM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MandatoryPrefix, {
			false,
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler()
		});
		break;
	case LegacyOpCodeHandlerKind::MandatoryPrefix3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MandatoryPrefix3, {
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_handler(),
			deserializer.read_legacy_handler_flags()
		});
		break;
	case LegacyOpCodeHandlerKind::D3NOW:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_D3NOW, {});
		break;
	case LegacyOpCodeHandlerKind::EVEX:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX, {deserializer.read_handler()});
		break;
	case LegacyOpCodeHandlerKind::VEX2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX2, {deserializer.read_handler()});
		break;
	case LegacyOpCodeHandlerKind::VEX3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX3, {deserializer.read_handler()});
		break;
	case LegacyOpCodeHandlerKind::XOP:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_XOP, {deserializer.read_handler()});
		break;
	case LegacyOpCodeHandlerKind::AL_DX:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_AL_DX, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Ap:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ap, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::B_BM:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_B_BM, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::B_Ev:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_B_Ev, {codes2.code1, codes2.code2, deserializer.read_boolean()});
		break;
	case LegacyOpCodeHandlerKind::B_MIB:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_B_MIB, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::BM_B:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_BM_B, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::BranchIw:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_BranchIw, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::BranchSimple:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_BranchSimple, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::C_R_3a:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_C_R, {codes2.code1, codes2.code2, deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::C_R_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_C_R, {deserializer.read_code(), Code::INVALID, deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::DX_AL:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_DX_AL, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::DX_eAX:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_DX_eAX, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::eAX_DX:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_eAX_DX, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Eb_1:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Eb, {deserializer.read_code(), 0});
		break;
	default:
		return nullptr;
	}
	return handler;
}

// Returns nullptr if `kind` is handled by another legacy_create_handlerN() fn
static const OpCodeHandler* legacy_create_handler2(TableDeserializer& deserializer, LegacyOpCodeHandlerKind kind) noexcept {
	Code code;
	Code2 codes2;
	Code3 codes3;
	const OpCodeHandler* handler;
	switch (kind) {
	case LegacyOpCodeHandlerKind::Eb_2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Eb, {deserializer.read_code(), deserializer.read_handler_flags()});
		break;
	case LegacyOpCodeHandlerKind::Eb_CL:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Eb_CL, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Eb_Gb_1:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Eb_Gb, {deserializer.read_code(), 0});
		break;
	case LegacyOpCodeHandlerKind::Eb_Gb_2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Eb_Gb, {deserializer.read_code(), deserializer.read_handler_flags()});
		break;
	case LegacyOpCodeHandlerKind::Eb_Ib_1:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Eb_Ib, {deserializer.read_code(), 0});
		break;
	case LegacyOpCodeHandlerKind::Eb_Ib_2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Eb_Ib, {deserializer.read_code(), deserializer.read_handler_flags()});
		break;
	case LegacyOpCodeHandlerKind::Eb1:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Eb_1, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Ed_V_Ib:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ed_V_Ib, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Ep:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ep, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Ev_3a:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev, {codes3.code1, codes3.code2, codes3.code3, 0});
		break;
	case LegacyOpCodeHandlerKind::Ev_3b:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev, {codes2.code1, codes2.code2, Code::INVALID, 0});
		break;
	case LegacyOpCodeHandlerKind::Ev_4:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev, {codes3.code1, codes3.code2, codes3.code3, deserializer.read_handler_flags()});
		break;
	case LegacyOpCodeHandlerKind::Ev_CL:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_CL, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Ev_Gv_32_64:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Gv_32_64, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Ev_Gv_3a:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Gv, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Ev_Gv_3b:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Gv, {codes2.code1, codes2.code2, Code::INVALID});
		break;
	case LegacyOpCodeHandlerKind::Ev_Gv_4:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Gv_flags, {
			codes3.code1,
			codes3.code2,
			codes3.code3,
			deserializer.read_handler_flags()
		});
		break;
	case LegacyOpCodeHandlerKind::Ev_Gv_CL:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Gv_CL, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Ev_Gv_Ib:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Gv_Ib, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Ev_Gv_REX:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Gv_REX, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Ev_Ib_3:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Ib, {codes3.code1, codes3.code2, codes3.code3, 0});
		break;
	case LegacyOpCodeHandlerKind::Ev_Ib_4:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Ib, {codes3.code1, codes3.code2, codes3.code3, deserializer.read_handler_flags()});
		break;
	case LegacyOpCodeHandlerKind::Ev_Ib2_3:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Ib2, {codes3.code1, codes3.code2, codes3.code3, 0});
		break;
	case LegacyOpCodeHandlerKind::Ev_Ib2_4:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Ib2, {codes3.code1, codes3.code2, codes3.code3, deserializer.read_handler_flags()});
		break;
	case LegacyOpCodeHandlerKind::Ev_Iz_3:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Iz, {codes3.code1, codes3.code2, codes3.code3, 0});
		break;
	case LegacyOpCodeHandlerKind::Ev_Iz_4:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Iz, {codes3.code1, codes3.code2, codes3.code3, deserializer.read_handler_flags()});
		break;
	case LegacyOpCodeHandlerKind::Ev_P:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_P, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Ev_REXW_1a:
		code = deserializer.read_code();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_REXW, {code, Code::INVALID, deserializer.read_u32()});
		break;
	case LegacyOpCodeHandlerKind::Ev_REXW:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_REXW, {codes2.code1, codes2.code2, deserializer.read_u32()});
		break;
	case LegacyOpCodeHandlerKind::Ev_Sw:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_Sw, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Ev_VX:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_VX, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Ev1:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ev_1, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Evj:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Evj, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Evw:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Evw, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Ew:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ew, {codes3.code1, codes3.code2, codes3.code3});
		break;
	default:
		return nullptr;
	}
	return handler;
}

// Returns nullptr if `kind` is handled by another legacy_create_handlerN() fn
static const OpCodeHandler* legacy_create_handler3(TableDeserializer& deserializer, LegacyOpCodeHandlerKind kind) noexcept {
	Code2 codes2;
	Code3 codes3;
	const OpCodeHandler* handler;
	switch (kind) {
	case LegacyOpCodeHandlerKind::Gb_Eb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gb_Eb, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Gdq_Ev:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gdq_Ev, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_Eb:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Eb, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_Eb_REX:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Eb_REX, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ev_32_64:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ev_32_64, {
			codes2.code1,
			codes2.code2,
			deserializer.read_boolean(),
			deserializer.read_boolean()
		});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ev_3a:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ev, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ev_3b:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ev, {codes2.code1, codes2.code2, Code::INVALID});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ev_Ib:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ev_Ib, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ev_Ib_REX:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ev_Ib_REX, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ev_Iz:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ev_Iz, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ev_REX:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ev_REX, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ev2:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ev2, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ev3:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ev3, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ew:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ew, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_M:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_M, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_M_as:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_M_as, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_Ma:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Ma, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Gv_Mp_2:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Mp, {codes2.code1, codes2.code2, Code::INVALID});
		break;
	case LegacyOpCodeHandlerKind::Gv_Mp_3:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Mp, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_Mv:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_Mv, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Gv_N:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_N, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Gv_N_Ib_REX:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_N_Ib_REX, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Gv_RX:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_RX, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Gv_W:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gv_W, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::GvM_VX_Ib:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_GvM_VX_Ib, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Ib:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ib, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Ib3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ib3, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::IbReg:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_IbReg, {deserializer.read_code(), deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::IbReg2:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_IbReg2, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Iw_Ib:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Iw_Ib, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Jb:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Jb, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Jb2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Jb2, {
			deserializer.read_code(),
			deserializer.read_code(),
			deserializer.read_code(),
			deserializer.read_code(),
			deserializer.read_code(),
			deserializer.read_code(),
			deserializer.read_code()
		});
		break;
	case LegacyOpCodeHandlerKind::Jdisp:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Jdisp, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Jx:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Jx, {codes2.code1, codes2.code2, deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Jz:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Jz, {codes3.code1, codes3.code2, codes3.code3});
		break;
	default:
		return nullptr;
	}
	return handler;
}

// Returns nullptr if `kind` is handled by another legacy_create_handlerN() fn
static const OpCodeHandler* legacy_create_handler4(TableDeserializer& deserializer, LegacyOpCodeHandlerKind kind) noexcept {
	Code2 codes2;
	Code3 codes3;
	std::uint32_t index;
	const OpCodeHandler* handler;
	switch (kind) {
	case LegacyOpCodeHandlerKind::M_1:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_M, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::M_2:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_M, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::M_REXW_2:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_M_REXW, {codes2.code1, codes2.code2, 0, 0});
		break;
	case LegacyOpCodeHandlerKind::M_REXW_4:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_M_REXW, {
			codes2.code1,
			codes2.code2,
			deserializer.read_handler_flags(),
			deserializer.read_handler_flags()
		});
		break;
	case LegacyOpCodeHandlerKind::MemBx:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MemBx, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Mf_1:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Mf, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Mf_2a:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Mf, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Mf_2b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Mf, {deserializer.read_code(), deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::MIB_B:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MIB_B, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::MP:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MP, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Ms:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ms, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::MV:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MV, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Mv_Gv:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Mv_Gv, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Mv_Gv_REXW:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Mv_Gv_REXW, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::NIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_NIb, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Ob_Reg:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ob_Reg, {deserializer.read_code(), deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::Ov_Reg:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Ov_Reg, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::P_Ev:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_P_Ev, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::P_Ev_Ib:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_P_Ev_Ib, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::P_Q:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_P_Q, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::P_Q_Ib:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_P_Q_Ib, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::P_R:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_P_R, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::P_W:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_P_W, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::PushEv:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PushEv, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::PushIb2:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PushIb2, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::PushIz:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PushIz, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::PushOpSizeReg_4a:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PushOpSizeReg, {
			codes3.code1,
			codes3.code2,
			codes3.code3,
			deserializer.read_register()
		});
		break;
	case LegacyOpCodeHandlerKind::PushOpSizeReg_4b:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PushOpSizeReg, {
			codes2.code1,
			codes2.code2,
			Code::INVALID,
			deserializer.read_register()
		});
		break;
	case LegacyOpCodeHandlerKind::PushSimple2:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PushSimple2, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::PushSimpleReg:
		index = deserializer.read_u32();
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PushSimpleReg, {index, codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Q_P:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Q_P, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::R_C_3a:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_R_C, {codes2.code1, codes2.code2, deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::R_C_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_R_C, {deserializer.read_code(), Code::INVALID, deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::rDI_P_N:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_rDI_P_N, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::rDI_VX_RX:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_rDI_VX_RX, {deserializer.read_code()});
		break;
	default:
		return nullptr;
	}
	return handler;
}

// Returns nullptr if `kind` is handled by another legacy_create_handlerN() fn
static const OpCodeHandler* legacy_create_handler5(TableDeserializer& deserializer, LegacyOpCodeHandlerKind kind) noexcept {
	Code code;
	Code code2;
	Code2 codes2;
	Code3 codes3;
	const OpCodeHandler* handler;
	switch (kind) {
	case LegacyOpCodeHandlerKind::Reg:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg, {deserializer.read_code(), deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::Reg_Ib2:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg_Ib2, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Reg_Iz:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg_Iz, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Reg_Ob:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg_Ob, {deserializer.read_code(), deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::Reg_Ov:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg_Ov, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Reg_Xb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg_Xb, {deserializer.read_code(), deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::Reg_Xv:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg_Xv, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Reg_Xv2:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg_Xv2, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Reg_Yb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg_Yb, {deserializer.read_code(), deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::Reg_Yv:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reg_Yv, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::RegIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RegIb, {deserializer.read_code(), deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::RegIb3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RegIb3, {deserializer.read_u32()});
		break;
	case LegacyOpCodeHandlerKind::RegIz2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RegIz2, {deserializer.read_u32()});
		break;
	case LegacyOpCodeHandlerKind::Reservednop:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Reservednop, {deserializer.read_handler(), deserializer.read_handler()});
		break;
	case LegacyOpCodeHandlerKind::RIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RIb, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::RIbIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RIbIb, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Rv:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Rv, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Rv_32_64:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Rv_32_64, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::RvMw_Gw:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RvMw_Gw, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Simple:
		code = deserializer.read_code();
		if (code == Code::Int3)
			handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Int3, {});
		else
			handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple, {false, code});
		break;
	case LegacyOpCodeHandlerKind::Simple_ModRM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple, {true, deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Simple2_3a:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple2, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Simple2_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple2, {
			deserializer.read_code(),
			deserializer.read_code(),
			deserializer.read_code()
		});
		break;
	case LegacyOpCodeHandlerKind::Simple2Iw:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple2Iw, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Simple3:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple3, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Simple4:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple4, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Simple4b:
		code = deserializer.read_code();
		code2 = deserializer.read_code();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple4, {code, code2});
		break;
	case LegacyOpCodeHandlerKind::Simple5:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple5, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Simple5_a32:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple5_a32, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Simple5_ModRM_as:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Simple5_ModRM_as, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::SimpleReg:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_SimpleReg, {deserializer.read_code(), deserializer.read_u32()});
		break;
	case LegacyOpCodeHandlerKind::ST_STi:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_ST_STi, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::STi:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_STi, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::STi_ST:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_STi_ST, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Sw_Ev:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Sw_Ev, {codes3.code1, codes3.code2, codes3.code3});
		break;
	default:
		return nullptr;
	}
	return handler;
}

// Returns nullptr if `kind` is handled by another legacy_create_handlerN() fn
static const OpCodeHandler* legacy_create_handler6(TableDeserializer& deserializer, LegacyOpCodeHandlerKind kind) noexcept {
	Code code;
	Code2 codes2;
	Code3 codes3;
	const OpCodeHandler* handler;
	switch (kind) {
	case LegacyOpCodeHandlerKind::V_Ev:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_V_Ev, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::VM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VM, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::VN:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VN, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::VQ:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VQ, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::VRIbIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VRIbIb, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::VW_2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VW, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::VW_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VW, {deserializer.read_code(), deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::VWIb_2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VWIb, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::VWIb_3:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VWIb, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::VX_E_Ib:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VX_E_Ib, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::VX_Ev:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VX_Ev, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Wbinvd:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Wbinvd, {});
		break;
	case LegacyOpCodeHandlerKind::WV:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_WV, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Xb_Yb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Xb_Yb, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Xchg_Reg_rAX:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Xchg_Reg_rAX, {deserializer.read_u32()});
		break;
	case LegacyOpCodeHandlerKind::Xv_Yv:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Xv_Yv, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Yb_Reg:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Yb_Reg, {deserializer.read_code(), deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::Yb_Xb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Yb_Xb, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Yv_Reg:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Yv_Reg, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::Yv_Reg2:
		codes2 = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Yv_Reg2, {codes2.code1, codes2.code2});
		break;
	case LegacyOpCodeHandlerKind::Yv_Xv:
		codes3 = deserializer.read_code3();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Yv_Xv, {codes3.code1, codes3.code2, codes3.code3});
		break;
	case LegacyOpCodeHandlerKind::M_Sw:
		code = deserializer.read_code();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_M_Sw, {code});
		break;
	case LegacyOpCodeHandlerKind::Sw_M:
		code = deserializer.read_code();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Sw_M, {code});
		break;
	case LegacyOpCodeHandlerKind::Rq:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Rq, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::Gd_Rd:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Gd_Rd, {deserializer.read_code()});
		break;
	case LegacyOpCodeHandlerKind::PrefixEsCsSsDs:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PrefixEsCsSsDs, {deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::PrefixFsGs:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PrefixFsGs, {deserializer.read_register()});
		break;
	case LegacyOpCodeHandlerKind::Prefix66:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Prefix66, {});
		break;
	case LegacyOpCodeHandlerKind::Prefix67:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Prefix67, {});
		break;
	case LegacyOpCodeHandlerKind::PrefixF0:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PrefixF0, {});
		break;
	case LegacyOpCodeHandlerKind::PrefixF2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PrefixF2, {});
		break;
	case LegacyOpCodeHandlerKind::PrefixF3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PrefixF3, {});
		break;
	case LegacyOpCodeHandlerKind::PrefixREX:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_PrefixREX, {deserializer.read_handler(), deserializer.read_u32()});
		break;
	default:
		return nullptr;
	}
	return handler;
}

void legacy_read_handlers(TableDeserializer& deserializer, HandlerVec& result) noexcept {
	LegacyOpCodeHandlerKind kind = deserializer.read_legacy_op_code_handler_kind();
	switch (kind) {
	case LegacyOpCodeHandlerKind::Invalid:
		result.push_back(get_invalid_handler());
		return;
	case LegacyOpCodeHandlerKind::Invalid_NoModRM:
		result.push_back(get_invalid_no_modrm_handler());
		return;
	case LegacyOpCodeHandlerKind::Invalid2:
		result.push_back(get_invalid_handler());
		result.push_back(get_invalid_handler());
		return;
	case LegacyOpCodeHandlerKind::Dup: {
		std::uint32_t count = deserializer.read_u32();
		const OpCodeHandler* dup_handler = deserializer.read_handler_or_null_instance();
		for (std::uint32_t i = 0; i < count; i++)
			result.push_back(dup_handler);
		return;
	}
	case LegacyOpCodeHandlerKind::Null:
		result.push_back(get_null_handler());
		return;
	case LegacyOpCodeHandlerKind::HandlerReference:
		result.push_back(deserializer.read_handler_reference());
		return;
	case LegacyOpCodeHandlerKind::ArrayReference:
		ICED_UNREACHABLE();
	default:
		break;
	}
	const OpCodeHandler* handler = legacy_create_handler1(deserializer, kind);
	if (handler == nullptr)
		handler = legacy_create_handler2(deserializer, kind);
	if (handler == nullptr)
		handler = legacy_create_handler3(deserializer, kind);
	if (handler == nullptr)
		handler = legacy_create_handler4(deserializer, kind);
	if (handler == nullptr)
		handler = legacy_create_handler5(deserializer, kind);
	if (handler == nullptr)
		handler = legacy_create_handler6(deserializer, kind);
	if (handler == nullptr)
		ICED_UNREACHABLE();
	result.push_back(handler);
}

} // namespace iced_x86::internal
