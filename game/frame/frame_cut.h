// game/frame/frame_cut.h — whether a sealed frame record is a cut, for the 60 fps in-between.
//
// A record is a cut when the scene it shows is not the scene of the record before it: another process
// state (0x8005B648), another scene in the mode-selecting record (0x8009F658) or another menu screen
// in the menu-world record (0x8009F8A4). Each scene enter places its camera and objects afresh.
// Identity is sampled after the frame's present call, so it describes the record being sealed.
#pragma once

#include "crashbash_guest.h"

#include <cstdint>
#include <optional>

class Core;

namespace crashbash {

class FrameCut {
public:
  struct SceneIdentity {
    std::uint32_t processState = 0;
    std::uint32_t scene = 0;
    std::uint32_t menuScreen = 0;
    bool operator==(const SceneIdentity &) const = default;
  };

  static SceneIdentity sample(Core &core);

  // The frame whose record is about to be sealed has run.
  void noteFrameEnded(Core &core);
  void noteFrameEnded(const SceneIdentity &current);
  // Whether the most recently ended frame is a cut from the one before it.
  [[nodiscard]] bool isCut() const {
    return cut_;
  }

private:
  std::optional<SceneIdentity> previous_;
  bool cut_ = true;
};

} // namespace crashbash
