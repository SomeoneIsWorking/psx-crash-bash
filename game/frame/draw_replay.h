// draw_replay.h — Crash Bash's in-betweens: the process present's draw section replayed with the draw contexts
// (cameras) it selected, the render components it drew, the texture scrolls the update linked and the scene fade
// blended.
#pragma once

#include "frame/draw_section.h"
#include "motion_set.h"
#include "render/component_motion.h"
#include "render/context_motion.h"
#include "render/texture_scroll_motion.h"
#include "replay_frame.h"

#include <array>
#include <cstdint>

class Core;

namespace crashbash {

namespace render {
class ComponentIncarnations;
}

class DrawReplay {
public:
  explicit DrawReplay(const render::ComponentIncarnations &incarnations);

  // The process state's present `present` is about to run.
  void beforePresent(Core &core, std::uint32_t present);
  // The next draw pairs with nothing.
  void forget();
  // The real draw selected draw context `context`.
  void noteSelected(std::uint32_t context);
  // The real draw drew render component `component` as `kind`.
  void noteDrawn(std::uint32_t component, render::ComponentMotion::Kind kind);
  // The update scrolled texture strip `scroller` of `owner` through the DR_MOVEs at `moves`.
  void noteScrolled(std::uint32_t scroller, std::uint32_t owner, std::uint32_t moves);

  [[nodiscard]] psx::present::ReplayFrame &frame() {
    return frame_;
  }

private:
  static constexpr std::size_t kTracks = 1;

  DrawSection section_;
  std::array<psx::present::MotionTrack, kTracks> tracks_;
  render::ContextMotion contexts_;
  render::ComponentMotion components_;
  render::TextureScrollMotion scrolls_;
  std::array<psx::present::MotionSource *, 3> sources_{&contexts_, &components_, &scrolls_};
  psx::present::ReplayFrame frame_;
};

} // namespace crashbash
