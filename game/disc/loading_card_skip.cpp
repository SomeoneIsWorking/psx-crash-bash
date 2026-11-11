#include "disc/loading_card_skip.h"

#include "core.h"

#include <cstdint>

namespace crashbash {
namespace {

// BOOT.BIN call sites of the card draw `jal 0x80018B08` (0x80094240, 0x80094180), keyed on r[31].
constexpr std::uint32_t kCardDrawReturnAddress = 0x80094248u;    // BOOT FUN_8009421C, screen 0x8009F998
constexpr std::uint32_t kLevelScreenReturnAddress = 0x80094188u; // BOOT FUN_8009414C, screen 0x8009FA00

} // namespace

bool isLoadingCardDraw(const Core &core) {
  // Only the LOADING panel is retired; the handoff presents its level as before.
  return core.r[31] == kCardDrawReturnAddress || core.r[31] == kLevelScreenReturnAddress;
}

} // namespace crashbash
