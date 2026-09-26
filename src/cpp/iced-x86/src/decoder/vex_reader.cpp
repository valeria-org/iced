// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: decoder/table_de/vex_reader.rs (used by the VEX and XOP tables)

#include "internal/decoder/handlers.hpp"
#include "internal/decoder/handlers_vex.hpp"
#include "internal/decoder/table_de.hpp"
#include "internal/decoder/vex_op_code_handler_kind.hpp"

namespace iced_x86::internal {

// The rest of vex_read_handlers(). It's split into two functions so the stack frame is small enough in unoptimized (Debug) builds.
static const OpCodeHandler* vex_create_handler2(TableDeserializer& deserializer, VexOpCodeHandlerKind kind) noexcept {
	Register reg;
	Code code;
	Code2 codes;
	const OpCodeHandler* handler;
	switch (kind) {
	case VexOpCodeHandlerKind::VHEv:
		reg = deserializer.read_register();
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHEv, {reg, codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::VHEvIb:
		reg = deserializer.read_register();
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHEvIb, {reg, codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::VHIs4W:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHIs4W, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VHIs5W:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHIs5W, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VHM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHM, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VHW_2:
		reg = deserializer.read_register();
		code = deserializer.read_code();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHW, {reg, reg, reg, code, code});
		break;

	case VexOpCodeHandlerKind::VHW_3:
		reg = deserializer.read_register();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHW, {reg, reg, reg, deserializer.read_code(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VHW_4:
		// Rust: OpCodeHandler_VEX_VHW::new1()
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHW,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VHWIb_2:
		reg = deserializer.read_register();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHWIb, {reg, reg, reg, deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VHWIb_4:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHWIb,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VHWIs4:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHWIs4, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VHWIs5:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VHWIs5, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VK_HK_RK:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VK_HK_RK, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VK_R:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VK_R, {deserializer.read_code(), deserializer.read_register()});
		break;

	case VexOpCodeHandlerKind::VK_RK:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VK_RK, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VK_RK_Ib:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VK_RK_Ib, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VK_WK:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VK_WK, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VM, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VW_2:
		reg = deserializer.read_register();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VW, {reg, reg, deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VW_3:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VW, {deserializer.read_register(), deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VWH:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VWH, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VWIb_2:
		reg = deserializer.read_register();
		code = deserializer.read_code();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VWIb, {reg, reg, code, code});
		break;

	case VexOpCodeHandlerKind::VWIb_3:
		reg = deserializer.read_register();
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VWIb, {reg, reg, codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::VX_Ev:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VX_Ev, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::VX_VSIB_HX:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VX_VSIB_HX,
								   {deserializer.read_register(), deserializer.read_register(), deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::WHV:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_WHV, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::WV:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_WV, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::WVIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_WVIb, {deserializer.read_register(), deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VT_SIBMEM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VT_SIBMEM, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::SIBMEM_VT:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_SIBMEM_VT, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VT:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VT, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VT_RT_HT:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VT_RT_HT, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::Options_DontReadModRM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Options_DontReadModRM,
								   {deserializer.read_handler(), deserializer.read_handler(), deserializer.read_decoder_options()});
		break;

	case VexOpCodeHandlerKind::Gq_HK_RK:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Gq_HK_RK, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::VK_R_Ib:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_VK_R_Ib, {deserializer.read_code(), deserializer.read_register()});
		break;

	case VexOpCodeHandlerKind::Gv_Ev:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Gv_Ev, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Ev:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Ev, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::K_Jb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_K_Jb, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::K_Jz:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_K_Jz, {deserializer.read_code()});
		break;

	default:
		ICED_UNREACHABLE();
	}
	return handler;
}

void vex_read_handlers(TableDeserializer& deserializer, HandlerVec& result) noexcept {
	Register reg;
	Code2 codes;
	const OpCodeHandler* handler;
	VexOpCodeHandlerKind kind = deserializer.read_vex_op_code_handler_kind();
	switch (kind) {
	case VexOpCodeHandlerKind::Invalid:
		result.push_back(get_invalid_handler());
		return;

	case VexOpCodeHandlerKind::Invalid2:
		result.push_back(get_invalid_handler());
		result.push_back(get_invalid_handler());
		return;

	case VexOpCodeHandlerKind::Dup: {
		std::uint32_t count = deserializer.read_u32();
		const OpCodeHandler* dup_handler = deserializer.read_handler_or_null_instance();
		for (std::uint32_t i = 0; i < count; i++)
			result.push_back(dup_handler);
		return;
	}

	case VexOpCodeHandlerKind::Null:
		result.push_back(get_null_handler());
		return;

	case VexOpCodeHandlerKind::Invalid_NoModRM:
		result.push_back(get_invalid_no_modrm_handler());
		return;

	case VexOpCodeHandlerKind::Bitness:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Bitness, {deserializer.read_handler(), deserializer.read_handler()});
		break;

	case VexOpCodeHandlerKind::Bitness_DontReadModRM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Bitness_DontReadModRM, {deserializer.read_handler(), deserializer.read_handler()});
		break;

	case VexOpCodeHandlerKind::HandlerReference:
		result.push_back(deserializer.read_handler_reference());
		return;

	case VexOpCodeHandlerKind::ArrayReference:
		ICED_UNREACHABLE();

	case VexOpCodeHandlerKind::RM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RM, {deserializer.read_handler(), deserializer.read_handler()});
		break;

	case VexOpCodeHandlerKind::Group:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Group,
								   {deserializer.read_array_reference(static_cast<std::uint32_t>(VexOpCodeHandlerKind::ArrayReference))});
		break;

	case VexOpCodeHandlerKind::Group8x64:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Group8x64,
								   {deserializer.read_array_reference(static_cast<std::uint32_t>(VexOpCodeHandlerKind::ArrayReference)),
									deserializer.read_array_reference(static_cast<std::uint32_t>(VexOpCodeHandlerKind::ArrayReference))});
		break;

	case VexOpCodeHandlerKind::W:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_W, {deserializer.read_handler(), deserializer.read_handler()});
		break;

	case VexOpCodeHandlerKind::MandatoryPrefix2_1:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MandatoryPrefix2,
								   {true, deserializer.read_handler(), get_invalid_handler(), get_invalid_handler(), get_invalid_handler()});
		break;

	case VexOpCodeHandlerKind::MandatoryPrefix2_4:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MandatoryPrefix2,
								   {true, deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler()});
		break;

	case VexOpCodeHandlerKind::MandatoryPrefix2_NoModRM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MandatoryPrefix2,
								   {false, deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler()});
		break;

	case VexOpCodeHandlerKind::VectorLength_NoModRM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VectorLength_VEX, {false, deserializer.read_handler(), deserializer.read_handler()});
		break;

	case VexOpCodeHandlerKind::VectorLength:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VectorLength_VEX, {true, deserializer.read_handler(), deserializer.read_handler()});
		break;

	case VexOpCodeHandlerKind::Ed_V_Ib:
		reg = deserializer.read_register();
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Ed_V_Ib, {reg, codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Ev_VX:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Ev_VX, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::G_VK:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_G_VK, {deserializer.read_code(), deserializer.read_register()});
		break;

	case VexOpCodeHandlerKind::Gv_Ev_Gv:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Gv_Ev_Gv, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Ev_Gv_Gv:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Ev_Gv_Gv, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Gv_Ev_Ib:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Gv_Ev_Ib, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Gv_Ev_Id:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Gv_Ev_Id, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Gv_GPR_Ib:
		reg = deserializer.read_register();
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Gv_GPR_Ib, {reg, codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Gv_Gv_Ev:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Gv_Gv_Ev, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Gv_RX:
		reg = deserializer.read_register();
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Gv_RX, {reg, codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Gv_W:
		reg = deserializer.read_register();
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Gv_W, {reg, codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::GvM_VX_Ib:
		reg = deserializer.read_register();
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_GvM_VX_Ib, {reg, codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::HRIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_HRIb, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::Hv_Ed_Id:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Hv_Ed_Id, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Hv_Ev:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Hv_Ev, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::M:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_M, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::MHV:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_MHV, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::M_VK:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_M_VK, {deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::MV:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_MV, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::rDI_VX_RX:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_rDI_VX_RX, {deserializer.read_register(), deserializer.read_code()});
		break;

	case VexOpCodeHandlerKind::RdRq:
		codes = deserializer.read_code2();
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_RdRq, {codes.code1, codes.code2});
		break;

	case VexOpCodeHandlerKind::Simple:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_VEX_Simple, {deserializer.read_code()});
		break;

	default:
		handler = vex_create_handler2(deserializer, kind);
		break;
	}
	result.push_back(handler);
}

} // namespace iced_x86::internal
