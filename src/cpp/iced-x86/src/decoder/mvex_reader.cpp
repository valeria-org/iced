// SPDX-License-Identifier: MIT
// Copyright (C) 2018-present iced project and contributors

// Rust: decoder/table_de/mvex_reader.rs

#include "internal/decoder/handlers.hpp"
#include "internal/decoder/handlers_mvex.hpp"
#include "internal/decoder/mvex_op_code_handler_kind.hpp"
#include "internal/decoder/table_de.hpp"

namespace iced_x86::internal {

void mvex_read_handlers(TableDeserializer& deserializer, HandlerVec& result) noexcept {
	const OpCodeHandler* handler;
	switch (deserializer.read_mvex_op_code_handler_kind()) {
	case MvexOpCodeHandlerKind::Invalid:
		result.push_back(get_invalid_handler());
		return;

	case MvexOpCodeHandlerKind::Invalid2:
		result.push_back(get_invalid_handler());
		result.push_back(get_invalid_handler());
		return;

	case MvexOpCodeHandlerKind::Dup: {
		std::uint32_t count = deserializer.read_u32();
		const OpCodeHandler* dup_handler = deserializer.read_handler();
		for (std::uint32_t i = 0; i < count; i++)
			result.push_back(dup_handler);
		return;
	}

	case MvexOpCodeHandlerKind::HandlerReference:
		result.push_back(deserializer.read_handler_reference());
		return;

	case MvexOpCodeHandlerKind::ArrayReference:
		ICED_UNREACHABLE();

	case MvexOpCodeHandlerKind::RM:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_RM, {deserializer.read_handler(), deserializer.read_handler()});
		break;

	case MvexOpCodeHandlerKind::Group:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_Group,
								   {deserializer.read_array_reference(static_cast<std::uint32_t>(MvexOpCodeHandlerKind::ArrayReference))});
		break;

	case MvexOpCodeHandlerKind::W:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_W, {deserializer.read_handler(), deserializer.read_handler()});
		break;

	case MvexOpCodeHandlerKind::MandatoryPrefix2:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MandatoryPrefix2,
								   {true, deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler(), deserializer.read_handler()});
		break;

	case MvexOpCodeHandlerKind::EH:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_EH, {deserializer.read_handler(), deserializer.read_handler()});
		break;

	case MvexOpCodeHandlerKind::M:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_M, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::MV:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_MV, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::VW:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_VW, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::HWIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_HWIb, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::VWIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_VWIb, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::VHW:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_VHW, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::VHWIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_VHWIb, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::VKW:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_VKW, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::KHW:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_KHW, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::KHWIb:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_KHWIb, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::VSIB:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_VSIB, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::VSIB_V:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_VSIB_V, {deserializer.read_code()});
		break;

	case MvexOpCodeHandlerKind::V_VSIB:
		handler = ICED_NEW_HANDLER(deserializer, OpCodeHandler_MVEX_V_VSIB, {deserializer.read_code()});
		break;

	default:
		ICED_UNREACHABLE();
	}
	result.push_back(handler);
}

} // namespace iced_x86::internal
