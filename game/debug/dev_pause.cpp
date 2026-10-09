#include "dev_pause.h"

#include "core.h"
#include "crashbash_guest.h"
#include "pad_input.h"

#include <cstdio>
#include <cstring>
#include <lucent/log.h>

namespace crashbash::debug {
namespace {

// Active-low pad masks, as the control channel's `tap`.
constexpr std::uint16_t kStartPressed = 0xFFFF & ~0x0008u;
constexpr std::uint16_t kUpPressed = 0xFFFF & ~0x0010u;
constexpr std::uint16_t kDownPressed = 0xFFFF & ~0x0040u;
constexpr std::uint16_t kLeftPressed = 0xFFFF & ~0x0080u;
constexpr std::uint16_t kRightPressed = 0xFFFF & ~0x0020u;
constexpr std::uint16_t kConfirmPressed = 0xFFFF & ~0x4000u;

constexpr const char *kArmed = "armed: the frame driver presses the pad at the next frame boundary";
constexpr const char *kUsage = "usage: pause open | up | down | left | right | confirm";

struct Named {
  const char *word;
  std::uint16_t mask;
};

// The one-press actions; the request stores the word's index.
constexpr Named kPresses[] = {
    {"up", kUpPressed},
    {"down", kDownPressed},
    {"left", kLeftPressed},
    {"right", kRightPressed},
    {"confirm", kConfirmPressed},
};

bool pauseIsOpen(Core &core) {
  return (core.mem_r32(guest::kPauseFlags) & guest::kPauseOpenBit) != 0;
}

// The mode scene (`0x8009F720`) runs the dispatcher from its update; any other scene gives Start another meaning.
bool modeSceneIsCurrent(Core &core) {
  return core.mem_r32(guest::kSceneTransition + guest::kSceneCurrentSlot) == guest::kModeScene;
}

} // namespace

bool DevPause::handle(Core &core, const char *cmd, const char *line, std::string &reply) {
  if (std::strcmp(cmd, "pause") != 0) {
    return false;
  }
  char argument[16] = {0};
  if (std::sscanf(line, "%*s %15s", argument) != 1) {
    reply = kUsage;
    return true;
  }
  if (std::strcmp(argument, "open") == 0) {
    if (pauseIsOpen(core)) {
      reply = "refused: the pause page is already open";
      return true;
    }
    request_ = Request{Action::Open, 0, "open", 0, false};
    reply = kArmed;
    return true;
  }
  for (const Named &press : kPresses) {
    if (std::strcmp(argument, press.word) != 0) {
      continue;
    }
    if (!pauseIsOpen(core)) {
      reply = "refused: the pause page is not open, send `pause open` first";
      return true;
    }
    request_ = Request{Action::Press, press.mask, press.word, 0, false};
    reply = kArmed;
    return true;
  }
  reply = kUsage;
  return true;
}

void DevPause::applyArmed(Core &core, Pad &pad, std::uint32_t frame) {
  if (request_.action == Action::None) {
    return;
  }
  if (!request_.started) {
    request_.started = true;
    request_.firstFrame = frame;
  }
  const std::uint32_t waited = frame - request_.firstFrame;

  if (request_.action == Action::Press) {
    pad.driveTap(request_.button, kPressFrames);
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
    pad.driveTap(kStartPressed, kPressFrames);
    lucent::info("crashbash-pause", "pause open: Start pressed at f{}", frame);
  }
}

} // namespace crashbash::debug
