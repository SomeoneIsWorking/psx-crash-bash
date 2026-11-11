#include "frame/draw_replay.h"

#include "core.h"
#include "title/guest.h"

namespace crashbash {
namespace {

using psx::present::Motion;
using psx::present::MotionField;

// The scene clock's value, which BOOT's mode present copies into the screen fade 0x800569AC.
constexpr std::array kClockFields{MotionField{.offset = 0, .motion = Motion::Linear}};

} // namespace

DrawReplay::DrawReplay(const render::ComponentIncarnations &incarnations)
    : tracks_{{
          {.name = "scene fade", .address = guest::kSceneTransitionClock, .fields = kClockFields},
      }},
      components_(incarnations), frame_(section_, tracks_, sources_) {}

void DrawReplay::beforePresent(Core &core, std::uint32_t present) {
  if (present != guest::kInitialStatePresent) {
    forget();
    return;
  }
  frame_.beforeDraw(core);
  components_.openDraw();
  contexts_.openDraw();
}

void DrawReplay::forget() {
  frame_.forget();
  components_.forget();
  contexts_.forget();
  scrolls_.forget();
}

void DrawReplay::noteSelected(std::uint32_t context) {
  if (!frame_.replaying()) {
    contexts_.noteSelected(context);
  }
}

void DrawReplay::noteDrawn(std::uint32_t component, render::ComponentMotion::Kind kind) {
  if (!frame_.replaying()) {
    components_.noteDrawn(component, kind);
  }
}

void DrawReplay::noteScrolled(std::uint32_t scroller, std::uint32_t owner, std::uint32_t moves) {
  if (!frame_.replaying()) {
    scrolls_.noteScrolled(scroller, owner, moves);
  }
}

} // namespace crashbash
