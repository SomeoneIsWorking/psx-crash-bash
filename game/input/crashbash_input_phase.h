#pragma once

#include <cstdint>

class Core;

namespace crashbash {

// Input phase for pad recordings: the guest's outer scene record and menu scene record packed so each
// press is an offset from the entry of the screen it was recorded on (frame counts drift with load timing).
// Both are CURRENT slots from crashbash_guest.h and stay stable while their screen does.
// The Polar Push briefing and its match share one segment, so no recording can be cut at match start.
// Each half keeps its low 24 bits (drops the 0x80 KSEG prefix), which makes psx::input::kUnkeyedPhase
// (both halves all-ones) unreachable.
// Stateless: every answer is read from the Core.
class InputPhase {
public:
  // Outer scene in the high half, menu screen in the low half.
  std::uint64_t of(Core &core) const;

  // The packed form, exposed for the test.
  static constexpr std::uint64_t pack(std::uint32_t scene, std::uint32_t menuScene) {
    return (static_cast<std::uint64_t>(scene & 0xFFFFFFu) << 24) | (menuScene & 0xFFFFFFu);
  }
};

} // namespace crashbash
