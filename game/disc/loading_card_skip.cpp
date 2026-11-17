#include "disc/loading_card_skip.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "frame/measured_guest_call.h"
#include "title/guest.h"

#include <cstdint>

namespace crashbash {
namespace {

// BOOT RenderLoadingScreen: gfx_set_viewport(&LoadingViewport, &LoadingCamera), then the card's eng_render.
constexpr std::uint32_t kLoadingViewport = 0x8009F8BCu;
constexpr std::uint32_t kLoadingCamera = 0x800A016Cu;
constexpr std::uint32_t kViewportReturnAddress = 0x80094248u;
constexpr std::uint32_t kViewportCallTicks = 9u;

void renderLoadingScreenOwned(Core *core) {
  // The viewport still clears the back buffer to black (LoadingViewport isbg); only the card model is retired.
  if (static_cast<std::int32_t>(core->mem_r32(guest::kLevelLoadStage)) < 2) {
    return;
  }
  measuredGuestCall(*core,
                    guest::kSelectDrawContext,
                    kViewportReturnAddress,
                    kViewportCallTicks,
                    kLoadingViewport,
                    kLoadingCamera);
  core->mem_w32(guest::kRenderStateModelDepthCue, core->mem_r32(guest::kSceneTransitionClock));
}

// UpdateLevelLoad stage 1 fades the screen in for the card, which stage 7's GotoProcess then fades out again.
constexpr std::uint32_t kCardFadeInReturnAddress = 0x80093DFCu;

void setFadeOwned(Core *core) {
  // With no card the fade-in and its fade-out show only black; the screen stays at the previous fade.
  if (core->r[31] == kCardFadeInReturnAddress) {
    return;
  }
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kSceneTransitionRequest);
}

} // namespace

void registerLoadingCardSkipOverride(Core &core) {
  runtime::registerNativeOverride(core,
                                  runtime::GuestImage::Boot,
                                  guest::kRenderLoadingScreen,
                                  "CrashBash::LoadingCardSkip",
                                  renderLoadingScreenOwned);
  runtime::registerNativeOverride(core,
                                  runtime::GuestImage::Resident,
                                  guest::kSceneTransitionRequest,
                                  "CrashBash::LoadingCardFadeSkip",
                                  setFadeOwned);
}

} // namespace crashbash
