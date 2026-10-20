// game/render/packet_decode.h — a guest packet's command words as the primitive the GPU draws.
#pragma once

#include "frame_record.h"

#include <cstdint>
#include <optional>
#include <span>

namespace crashbash::render {

// `words` are the packet's command words after its header. A packet may lead with draw mode words (and no-op
// pads), applied by psxport's DrawModeTracker.
std::optional<psx::present::DrawPrimitive> decodePacket(std::span<const std::uint32_t> words);

// `words` is a whole packet: its header, whose high byte counts the command words, then those.
std::optional<psx::present::DrawPrimitive> decodeLinkedPacket(std::span<const std::uint32_t> words);

} // namespace crashbash::render
