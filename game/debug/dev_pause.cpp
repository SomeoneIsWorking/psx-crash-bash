#include "debug/dev_pause.h"

#include "core.h"
#include "game.h"
#include "host_input.h"
#include "title/guest.h"

#include <lucent/log.h>

namespace crashbash::debug {
namespace {

constexpr const char *kArmed = "armed: the frame driver presses the pad at the next frame boundary";
// The one-press buttons, after `open` in DevPause::kActionNames.
constexpr std::array<std::uint16_t, 5> kPresses{psx::input::kButtonUp,
                                                psx::input::kButtonDown,
                                                psx::input::kButtonLeft,
                                                psx::input::kButtonRight,
                                                psx::input::kButtonCross};
static_assert(kPresses.size() + 1 == DevPause::kActionNames.size());

bool pauseIsOpen(Core &core) {
  return (core.mem_r32(guest::kPauseFlags) & guest::kPauseOpenBit) != 0;
}

// The mode scene (`0x8009F720`) runs the dispatcher from its update; any other scene gives Start another meaning.
bool modeSceneIsCurrent(Core &core) {
  return core.mem_r32(guest::kSceneTransition + guest::kSceneCurrentSlot) == guest::kModeScene;
}

} // namespace

std::string DevPause::arm(Core &core, std::uint32_t action) {
  if (action == 0) {
    if (pauseIsOpen(core)) {
      return "refused: the pause page is already open";
    }
    request_ = Request{Action::Open, 0, kActionNames[0], 0, false};
    return kArmed;
  }
  if (!pauseIsOpen(core)) {
    return "refused: the pause page is not open, send `pause open` first";
  }
  request_ = Request{Action::Press, kPresses[action - 1], kActionNames[action], 0, false};
  return kArmed;
}

void DevPause::applyArmed(Core &core, std::uint32_t frame) {
  if (request_.action == Action::None) {
    return;
  }
  if (!request_.started) {
    request_.started = true;
    request_.firstFrame = frame;
  }
  const std::uint32_t waited = frame - request_.firstFrame;

  if (request_.action == Action::Press) {
    core.game->pad.injectButtons(request_.button, psx::input::InjectedPress::Tap, kPressFrames);
    lucent::info("crashbash-pause", "pause {} pressed at f{}", request_.word, frame);
    request_ = Request{};
    return;
  }

  if (pauseIsOpen(core)) {
    lucent::info("crashbash-pause",
                 "pause open at f{} after {} frame(s): guest flags 0x{:08X}",
                 frame,
                 waited,
                 core.mem_r32(guest::kPauseFlags));
    request_ = Request{};
    return;
  }
  if (waited > kFrameBudget) {
    lucent::info("crashbash-pause",
                 "pause open abandoned at f{} after {} frame(s): scene 0x{:08X}, guest flags 0x{:08X}",
                 frame,
                 kFrameBudget,
                 core.mem_r32(guest::kSceneTransition + guest::kSceneCurrentSlot),
                 core.mem_r32(guest::kPauseFlags));
    request_ = Request{};
    return;
  }
  if (modeSceneIsCurrent(core) && waited % kPressStride == 0) {
    core.game->pad.injectButtons(psx::input::kButtonStart, psx::input::InjectedPress::Tap, kPressFrames);
    lucent::info("crashbash-pause", "pause open: Start pressed at f{}", frame);
  }
}

} // namespace crashbash::debug
