#include "boot_logo_skip.h"

#include "core.h"
#include "crashbash_guest.h"
#include "game.h"
#include "guest_execution.h"
#include "measured_guest_call.h"

#include <cstdint>

#include <lucent/log.h>

namespace crashbash {
namespace {

constexpr std::uint16_t kStartButton = 0x0008u;
constexpr std::uint16_t kCrossButton = 0x4000u;
constexpr std::uint16_t kSkipButtons = kStartButton | kCrossButton;
constexpr std::uint32_t kScenePendingState = guest::kSceneTransition + 4u;

void bootLogoUpdateOwned(Core *core) {
  // Same target/flag pair the logo controller writes on completion, requested through 0x8001E588 so the next
  // dispatch runs the exit callback without disturbing the controller's own state.
  if (core->game->pad.pressedButton(kSkipButtons) && core->mem_r32(kScenePendingState) == 0u) {
    measuredGuestCall(*core,
                      guest::kSceneTransitionRequest,
                      0x80092B28u,
                      3u,
                      guest::kSceneTransition,
                      guest::kBootLogoHandoffState,
                      guest::kBootLogoHandoffFlags);
    lucent::info("boot", "Start or Cross requested the BOOT logo handoff through the scene lifecycle");
    return;
  }

  runtime::callOriginal(*core, runtime::GuestImage::Boot, guest::kBootLogoUpdate);
}

} // namespace

void registerBootLogoSkipOverride(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Boot, guest::kBootLogoUpdate, "CrashBash::BootLogoSkip", bootLogoUpdateOwned);
}

} // namespace crashbash
