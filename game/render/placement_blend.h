// game/render/placement_blend.h — how a render moves the coordinates of a component draw between two states.
#pragma once

#include "blend.h"

#include <cstdint>

namespace crashbash::render {

// A coordinate word is plain, or centred (bits 15-14 are 10) with an offset in bits 13-0.
constexpr bool isCentred(std::int16_t coordinate) {
  return (static_cast<std::uint16_t>(coordinate) & 0xC000u) == 0x8000u;
}

constexpr std::uint16_t centreOffset(std::int16_t coordinate) {
  return static_cast<std::uint16_t>(coordinate) & 0x3FFFu;
}

// `t` of the way from `from` to `to`. Two plain coordinates move; two centred ones with offsets move their
// offsets; a plain one against a centred one, or a centred one against a centred one at offset 0, is another
// layout and stays `to`.
inline std::int16_t blendCoordinate(std::int16_t from, std::int16_t to, float t) {
  if (isCentred(from) != isCentred(to)) {
    return to;
  }
  if (!isCentred(to)) {
    return static_cast<std::int16_t>(psx::present::lerpInt(from, to, t));
  }
  if ((centreOffset(from) == 0u) != (centreOffset(to) == 0u)) {
    return to;
  }
  const auto offset = psx::present::lerpInt(centreOffset(from), centreOffset(to), t);
  return static_cast<std::int16_t>(0x8000u | (static_cast<std::uint32_t>(offset) & 0x3FFFu));
}

// A plain size or position halfword.
inline std::int16_t blendHalf(std::int16_t from, std::int16_t to, float t) {
  return static_cast<std::int16_t>(psx::present::lerpInt(from, to, t));
}

} // namespace crashbash::render
