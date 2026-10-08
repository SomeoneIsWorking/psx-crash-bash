#pragma once

#include <cstdint>
#include <cstdio>

class Core;
class Pad;

namespace crashbash::debug {

// Control-channel `arena list` / `arena <id> [mode]` (battle|tournament). The command only arms a request;
// the frame driver advances it at frame boundaries through the guest's own menu flow. SELECT BATTLE TYPE's
// update (`FUN_800b7458`) reads cursor `DAT_8005a64a/b` to index the arena table at `0x800BA324`.
class DevArena {
public:
  // Returns true when `cmd` was this title's command and the reply has been written to `out`.
  bool handle(Core &core, const char *cmd, const char *line, std::FILE *out);

  // Called at every frame boundary; `pad` delivers the real Start press that leaves the attract demo.
  void applyArmed(Core &core, Pad &pad, std::uint32_t frame);

private:
  enum class ArenaMode : std::uint32_t {
    Battle = 0,
    Tournament = 1,
  };

  enum class Stage : std::uint32_t {
    // The attract demo owns the nested overlay slot; leave it with a Start press.
    EnterMenu = 0,
    // The mode's flow table has not been requested yet.
    SelectMode = 1,
    // Waiting for SELECT BATTLE TYPE to become the current menu screen.
    AwaitArenaScreen = 2,
    // The cursor names the requested entry; waiting for the guest's arena selection to match.
    AwaitCommit = 3,
  };

  struct Request {
    bool armed = false;
    std::uint32_t arena = 0;
    ArenaMode mode = ArenaMode::Battle;
    std::uint32_t page = 0;
    std::uint32_t index = 0;
    Stage stage = Stage::EnterMenu;
    std::uint32_t firstFrame = 0;
    // The pad reports `repl_on` for its own forced drive too, so ownership is only decidable before the first tap.
    bool tapped = false;
    std::uint32_t lastScreen = 0;
  };

  // Bounded so a request that never reaches its screen cannot hold the frame driver forever.
  static inline constexpr std::uint32_t kFrameBudget = 2400u;
  // Report cadence while waiting.
  static inline constexpr std::uint32_t kWaitReportStride = 120u;
  // Frames between Start presses while the menu has not come up.
  static inline constexpr std::uint32_t kStartRetryStride = 90u;
  // Active-low on the pad mask, as the control channel's `tap start` / `tap x`.
  static inline constexpr std::uint16_t kStartPressed = 0xFFFF & ~0x0008u;
  static inline constexpr std::uint16_t kConfirmPressed = 0xFFFF & ~0x4000u;
  static inline constexpr int kPressFrames = 4;
  // Frames between Cross presses while the flow walks its screens.
  static inline constexpr std::uint32_t kFlowPressStride = 30u;
  Request request_{};
};

} // namespace crashbash::debug