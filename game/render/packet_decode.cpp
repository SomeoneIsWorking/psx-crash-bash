#include "packet_decode.h"

#include "gp0_primitive_decode.h"

#include <algorithm>

namespace crashbash::render {
namespace {

constexpr std::uint32_t kDrawModeCommand = 0xE1u;

} // namespace

std::optional<psx::present::DrawPrimitive> decodePacket(std::span<const std::uint32_t> words) {
  std::size_t first = 0;
  std::optional<std::uint16_t> drawMode;
  while (first < words.size() && (words[first] == 0u || words[first] >> 24 == kDrawModeCommand)) {
    if (words[first] != 0u) {
      drawMode = static_cast<std::uint16_t>(words[first]);
    }
    ++first;
  }
  auto primitive = psx::gpu::decodePacketPrimitive(words.subspan(first));
  if (primitive && drawMode && !primitive->textured) {
    psx::gpu::applyTexPageAttribute(primitive->state, *drawMode);
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
