// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: decoder/table_de/evex_reader.rs

#include "internal/decoder/handlers.hpp"
#include "internal/decoder/handlers_evex.hpp"
#include "internal/decoder/table_de.hpp"

namespace iced_x86::internal {

// The rest of evex_read_handlers(). It's split into two functions so the stack frame is small enough in unoptimized (Debug) builds.
static const OpCodeHandler* evex_create_handler2(TableDeserializer& deserializer, EvexOpCodeHandlerKind kind) noexcept {
	const OpCodeHandler* handler;
	switch (kind) {
	case EvexOpCodeHandlerKind::VkHW_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHW,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::VkHW_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHW,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::VkHW_5:
		// Rust: OpCodeHandler_EVEX_VkHW::new1()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHW,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_register(), deserializer.read_code(),
									deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::VkHW_er_4:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHW_er,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), deserializer.read_boolean(), false});
		break;

	case EvexOpCodeHandlerKind::VkHW_er_4b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHW_er,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), deserializer.read_boolean(), true});
		break;

	case EvexOpCodeHandlerKind::VkHW_er_ur_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHW_er_ur,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::VkHW_er_ur_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHW_er_ur,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::VkHWIb_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::VkHWIb_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::VkHWIb_5:
		// Rust: OpCodeHandler_EVEX_VkHWIb::new1()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHWIb,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_register(), deserializer.read_code(),
									deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::VkHWIb_er_4:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHWIb_er,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::VkHWIb_er_4b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHWIb_er,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::VkM:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkM, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VkW_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkW,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::VkW_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkW,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::VkW_4:
		// Rust: OpCodeHandler_EVEX_VkW::new1()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkW,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::VkW_4b:
		// Rust: OpCodeHandler_EVEX_VkW::new1()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkW,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::VkW_er_4:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkW_er,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), deserializer.read_boolean()});
		break;

	case EvexOpCodeHandlerKind::VkW_er_5:
		// Rust: OpCodeHandler_EVEX_VkW_er::new1()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkW_er,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(),
									deserializer.read_boolean()});
		break;

	case EvexOpCodeHandlerKind::VkW_er_6:
		// Rust: OpCodeHandler_EVEX_VkW_er::new2()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkW_er,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(),
									deserializer.read_boolean(), deserializer.read_boolean()});
		break;

	case EvexOpCodeHandlerKind::VkWIb_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::VkWIb_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::VkWIb_er:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkWIb_er,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VM:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VM, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VSIB_k1:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VSIB_k1,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VSIB_k1_VX:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VSIB_k1_VX,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VW:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VW, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VW_er:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VW_er, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VX_Ev: {
		Code2 codes = deserializer.read_code2();
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VX_Ev, {codes.code1, codes.code2, deserializer.read_tuple_type(), deserializer.read_tuple_type()});
		break;
	}

	case EvexOpCodeHandlerKind::WkHV:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_WkHV, {deserializer.read_register(), deserializer.read_code()});
		break;

	case EvexOpCodeHandlerKind::WkV_3:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_WkV, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::WkV_4a:
		// Rust: OpCodeHandler_EVEX_WkV::new2()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_WkV,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::WkV_4b:
		// Rust: OpCodeHandler_EVEX_WkV::new1()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_WkV,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), deserializer.read_boolean()});
		break;

	case EvexOpCodeHandlerKind::WkVIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_WkVIb,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::WkVIb_er:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_WkVIb_er,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::WV:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_WV, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	default:
		ICED_UNREACHABLE();
	}
	return handler;
}

void evex_read_handlers(TableDeserializer& deserializer, HandlerVec& result) noexcept {
	const OpCodeHandler* handler;
	EvexOpCodeHandlerKind kind = deserializer.read_evex_op_code_handler_kind();
	switch (kind) {
	case EvexOpCodeHandlerKind::Invalid:
		result.push_back(get_invalid_handler());
		return;

	case EvexOpCodeHandlerKind::Invalid2:
		result.push_back(get_invalid_handler());
		result.push_back(get_invalid_handler());
		return;

	case EvexOpCodeHandlerKind::Dup: {
		std::uint32_t count = deserializer.read_u32();
		const OpCodeHandler* dup_handler = deserializer.read_handler();
		for (std::uint32_t i = 0; i < count; i++)
			result.push_back(dup_handler);
		return;
	}

	case EvexOpCodeHandlerKind::HandlerReference:
		result.push_back(deserializer.read_handler_reference());
		return;

	case EvexOpCodeHandlerKind::ArrayReference:
		ICED_UNREACHABLE();

	case EvexOpCodeHandlerKind::RM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RM, {deserializer.read_handler(), deserializer.read_handler()});
		break;

	case EvexOpCodeHandlerKind::Group:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Group,
								   {deserializer.read_array_reference(static_cast<std::uint32_t>(EvexOpCodeHandlerKind::ArrayReference))});
		break;

	case EvexOpCodeHandlerKind::W:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_W, {deserializer.read_handler(), deserializer.read_handler()});
		break;

	case EvexOpCodeHandlerKind::MandatoryPrefix2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MandatoryPrefix2,
								   {true, deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler()});
		break;

	case EvexOpCodeHandlerKind::VectorLength:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VectorLength_EVEX,
								   {deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler()});
		break;

	case EvexOpCodeHandlerKind::VectorLength_er:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VectorLength_EVEX_er,
								   {deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler()});
		break;

	case EvexOpCodeHandlerKind::Ed_V_Ib: {
		Register reg = deserializer.read_register();
		Code2 codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_Ed_V_Ib,
								   {reg, codes.code1, codes.code2, deserializer.read_tuple_type(), deserializer.read_tuple_type()});
		break;
	}

	case EvexOpCodeHandlerKind::Ev_VX: {
		Code2 codes = deserializer.read_code2();
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_Ev_VX, {codes.code1, codes.code2, deserializer.read_tuple_type(), deserializer.read_tuple_type()});
		break;
	}

	case EvexOpCodeHandlerKind::Ev_VX_Ib: {
		Register reg = deserializer.read_register();
		Code2 codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_Ev_VX_Ib, {reg, codes.code1, codes.code2});
		break;
	}

	case EvexOpCodeHandlerKind::Gv_W_er: {
		Register reg = deserializer.read_register();
		Code2 codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_Gv_W_er,
								   {reg, codes.code1, codes.code2, deserializer.read_tuple_type(), deserializer.read_boolean()});
		break;
	}

	case EvexOpCodeHandlerKind::GvM_VX_Ib: {
		Register reg = deserializer.read_register();
		Code2 codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_GvM_VX_Ib,
								   {reg, codes.code1, codes.code2, deserializer.read_tuple_type(), deserializer.read_tuple_type()});
		break;
	}

	case EvexOpCodeHandlerKind::HkWIb_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_HkWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::HkWIb_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_HkWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::HWIb:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_HWIb, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::KkHW_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KkHW,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::KkHW_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KkHW,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::KkHWIb_sae_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KkHWIb_sae,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::KkHWIb_sae_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KkHWIb_sae,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::KkHWIb_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KkHWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::KkHWIb_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KkHWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::KkWIb_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KkWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), false});
		break;

	case EvexOpCodeHandlerKind::KkWIb_3b:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KkWIb,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type(), true});
		break;

	case EvexOpCodeHandlerKind::KP1HW:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KP1HW, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::KR:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_KR, {deserializer.read_register(), deserializer.read_code()});
		break;

	case EvexOpCodeHandlerKind::MV:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_MV, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::V_H_Ev_er: {
		Register reg = deserializer.read_register();
		Code2 codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_V_H_Ev_er,
								   {reg, codes.code1, codes.code2, deserializer.read_tuple_type(), deserializer.read_tuple_type()});
		break;
	}

	case EvexOpCodeHandlerKind::V_H_Ev_Ib: {
		Register reg = deserializer.read_register();
		Code2 codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_V_H_Ev_Ib,
								   {reg, codes.code1, codes.code2, deserializer.read_tuple_type(), deserializer.read_tuple_type()});
		break;
	}

	case EvexOpCodeHandlerKind::VHM:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VHM, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VHW_3:
		// Rust: OpCodeHandler_EVEX_VHW::new2()
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VHW, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VHW_4:
		// Rust: OpCodeHandler_EVEX_VHW::new()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VHW,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VHWIb:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VHWIb, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VK:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VK, {deserializer.read_register(), deserializer.read_code()});
		break;

	case EvexOpCodeHandlerKind::Vk_VSIB:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_Vk_VSIB,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	case EvexOpCodeHandlerKind::VkEv_REXW_2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkEv_REXW, {deserializer.read_register(), deserializer.read_code(), Code::INVALID});
		break;

	case EvexOpCodeHandlerKind::VkEv_REXW_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkEv_REXW,
								   {deserializer.read_register(), deserializer.read_code(), deserializer.read_code()});
		break;

	case EvexOpCodeHandlerKind::VkHM:
		handler =
			ICED_NEW_HANDLER(deserializer, OpCodeHandler_EVEX_VkHM, {deserializer.read_register(), deserializer.read_code(), deserializer.read_tuple_type()});
		break;

	default:
		handler = evex_create_handler2(deserializer, kind);
		break;
	}
	result.push_back(handler);
}

} // namespace iced_x86::internal
