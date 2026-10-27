#pragma once

#include <array>
#include <cstdint>
#include <string>

class Core;

namespace crashbash::debug {

// The debug menu's `pause open|up|down|left|right|confirm`. Arming only records a request; the frame driver
// applies it at a frame boundary as a real pad press, so BOOT's pause dispatcher (`FUN_8007F314`) opens and walks its
// pages itself; every action but `open` needs the page open and presses its button once. `open` needs a live match: it
// presses Start until the dispatcher sets its pause flag.
class DevPause {
public:
  // The actions `pause` takes, in its parameter's order; `open` first, then the one-press buttons.
  static constexpr std::array<const char *, 6> kActionNames{"open", "up", "down", "left", "right", "confirm"};

  // Arms action `action` (an index of kActionNames); the returned text answers the client.
  std::string arm(Core &core, std::uint32_t action);

  // Called at every frame boundary, before the pad is serviced.
  void applyArmed(Core &core, std::uint32_t frame);

  bool armed() const {
    return request_.action != Action::None;
  }

private:
  enum class Action : std::uint8_t {
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
  // Input turns a press is held.
  static inline constexpr int kPressTurns = 4;

  Request request_{};
};

} // namespace crashbash::debug
