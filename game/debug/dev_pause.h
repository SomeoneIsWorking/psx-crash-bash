#pragma once

#include <cstdint>
#include <string>

class Core;
class Pad;

namespace crashbash::debug {

// Control-channel and REPL `pause open|up|down|left|right|confirm`. The command only arms a request; the frame driver
// applies it at a frame boundary as a real pad press, so BOOT's pause dispatcher (`FUN_8007F314`) opens and walks its
// pages itself; every action but `open` needs the page open and presses its button once. `open` needs a live match: it
// presses Start until the dispatcher sets its pause flag.
class DevPause {
public:
  // Returns true when `cmd` was this title's command; the reply text is left in `reply`.
  bool handle(Core &core, const char *cmd, const char *line, std::string &reply);

  // Called at every frame boundary, before the pad is serviced.
  void applyArmed(Core &core, Pad &pad, std::uint32_t frame);

  bool armed() const {
    return request_.action != Action::None;
  }

private:
  enum class Action : std::uint32_t {
    None = 0,
    Open = 1,
    // One press of `button` while the page is open.
    Press = 2,
  };

  struct Request {
    Action action = Action::None;
    std::uint16_t button = 0;
    const char *word = "";
    std::uint32_t firstFrame = 0;
    bool started = false;
  };

  // Bounded so a request that never reaches a live match cannot press Start forever.
  static inline constexpr std::uint32_t kFrameBudget = 600u;
  // Frames between Start presses while the pause flag has not come up.
  static inline constexpr std::uint32_t kPressStride = 30u;
  static inline constexpr int kPressFrames = 4;

  Request request_{};
};

} // namespace crashbash::debug
