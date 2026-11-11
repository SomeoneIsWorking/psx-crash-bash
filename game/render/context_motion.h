// context_motion.h — the draw contexts (cameras) the real draw selected, sampled at each draw section's entry and
// written blended into its replays: the inputs FUN_80014540 derives the view from.
#pragma once

#include "motion_set.h"

#include <array>
#include <cstdint>
#include <vector>

class Core;

namespace crashbash::render {

class ContextMotion final : public psx::present::MotionSource {
public:
  // The real draw derived the view of `context` (FUN_80014540's a0); it is sampled from the next section entry on.
  void noteSelected(std::uint32_t context);
  // The real draw begins: what it selects is noted afresh.
  void openDraw();
  // Nothing sampled so far pairs with the next sample.
  void forget();

  [[nodiscard]] const char *name() const override {
    return "draw contexts";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

  // The view's translation (+0x00), the eye objects are placed from (+0x0C), the projection distance H (+0x18) and
  // the view angles RotMatrix turns into +0x74 (+0x54).
  static constexpr std::array<psx::present::MotionField, 9> kFields{{
      {.offset = 0x00},
      {.offset = 0x04},
      {.offset = 0x08},
      {.offset = 0x0C},
      {.offset = 0x10},
      {.offset = 0x14},
      {.offset = 0x18},
      {.offset = 0x54, .motion = psx::present::Motion::Angle12x2},
      {.offset = 0x58, .motion = psx::present::Motion::Angle12Low},
  }};

private:
  struct Record {
    std::uint32_t address = 0;
    std::array<std::uint32_t, kFields.size()> words{};
  };

  std::vector<std::uint32_t> selected_;
  std::vector<Record> earlier_;
  std::vector<Record> later_;
};

// Overrides FUN_80014540 (derive a draw context's view) to note each context the real draw derives a view from.
void registerContextViewNotes(Core &core);

} // namespace crashbash::render
