#include "loading_card_skip.h"

#include "core.h"
#include "crashbash_guest.h"
#include "guest_execution.h"

#include <cstdint>

namespace crashbash {
namespace {

// BOOT.BIN call sites of the card draw `jal 0x80018B08` (0x80094240, 0x80094180), keyed on r[31].
constexpr std::uint32_t kCardDrawReturnAddress = 0x80094248u;    // BOOT FUN_8009421C, screen 0x8009F998
constexpr std::uint32_t kLevelScreenReturnAddress = 0x80094188u; // BOOT FUN_8009414C, screen 0x8009FA00

void loadingCardDrawOwned(Core *core) {
  // Only the LOADING panel is retired; the handoff presents its level as before.
  if (core->r[31] == kCardDrawReturnAddress || core->r[31] == kLevelScreenReturnAddress) {
    return;
  }
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kLoadingCardDraw);
}

} // namespace

void registerLoadingCardSkipOverride(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kLoadingCardDraw, "CrashBash::LoadingCardSkip", loadingCardDrawOwned);
}

} // namespace crashbash
