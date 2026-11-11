// texture_scroll_motion.h — the texture strips FUN_80028F28 scrolled in the update (the Polar Push aurora), sampled
// at each draw section's entry; a replay rewrites the two DR_MOVEs the update linked at the blended scroll.
#pragma once

#include "motion_set.h"

#include <array>
#include <cstdint>
#include <vector>

class Core;

namespace crashbash::render {

class TextureScrollMotion final : public psx::present::MotionSource {
public:
  // FUN_80028F28(scroller, owner) scrolled `scroller` and linked its two DR_MOVEs at `moves` and `moves + 24`.
  void noteScrolled(std::uint32_t scroller, std::uint32_t owner, std::uint32_t moves);
  // Nothing sampled so far pairs with the next sample.
  void forget();

  [[nodiscard]] const char *name() const override {
    return "texture scrolls";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

private:
  struct Note {
    std::uint32_t scroller = 0;
    std::uint32_t owner = 0;
    std::uint32_t moves = 0;
  };
  // A strip of `height` rows at `src` copied rolled by the position's whole rows to `dst`.
  struct Record {
    std::uint32_t scroller = 0;
    std::uint32_t moves = 0;
    std::array<std::uint32_t, 2> links{}; // each move's tag link, which the section keeps
    std::int32_t position = 0;            // 8.8, in [0, height)
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t srcX = 0;
    std::int32_t srcY = 0;
    std::int32_t dstX = 0;
    std::int32_t dstY = 0;

    [[nodiscard]] bool sameStrip(const Record &other) const {
      return scroller == other.scroller && width == other.width && height == other.height && srcX == other.srcX &&
             srcY == other.srcY && dstX == other.dstX && dstY == other.dstY;
    }
  };

  std::vector<Note> noted_;
  std::vector<Record> earlier_;
  std::vector<Record> later_;
};

// Overrides FUN_80028F28 to note each scroll the update makes.
void registerTextureScrollNotes(Core &core);

} // namespace crashbash::render
