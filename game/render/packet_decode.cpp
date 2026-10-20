#include "render/packet_decode.h"

#include "gp0_primitive_decode.h"

#include <algorithm>

namespace crashbash::render {

std::optional<psx::present::DrawPrimitive> decodePacket(std::span<const std::uint32_t> words) {
  psx::present::DrawModeTracker drawMode;
  bool moded = false;
  std::size_t first = 0;
  for (; first < words.size(); ++first) {
    if (drawMode.take(words.subspan(first, 1))) {
      moded = true;
    } else if (words[first] != 0u) {
      break;
    }
  }
  auto primitive = psx::gpu::decodePacketPrimitive(words.subspan(first));
  // Without a mode word the packet keeps the GPU's mode, which this decode does not track.
  if (primitive && moded) {
    drawMode.apply(*primitive);
  }
  return primitive;
}

std::optional<psx::present::DrawPrimitive> decodeLinkedPacket(std::span<const std::uint32_t> words) {
  if (words.empty()) {
    return std::nullopt;
  }
  const std::size_t length = std::min<std::size_t>(words[0] >> 24, words.size() - 1u);
  return decodePacket(words.subspan(1, length));
}

} // namespace crashbash::render
