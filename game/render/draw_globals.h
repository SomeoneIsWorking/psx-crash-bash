// game/render/draw_globals.h — the guest globals every native packet body reads besides its arguments.
#pragma once

#include <cstdint>

class Core;

namespace crashbash::render {

struct DrawGlobals {
  std::uint32_t hasEnvironment = 0; // 0x8005B698 points at a display environment
  std::int32_t screenScale = 0;     // its s16 at +4: the display width the 640-wide layout maps to
  std::uint32_t fade = 0;           // 0x800569AC, 0..0x1000: how far colours are faded to black
  std::uint32_t otBase = 0;         // 0x800569D8: bucket 0 of the table slice packets link into
  std::int32_t zBias = 0;           // 0x800569DC (s16): added to a packet's depth before the bucket
  std::int32_t zLimit = 0;          // 0x800569DE (s16): buckets from here up are not linked
  std::int32_t originX = 0;         // 0x800569C0, 0x800569C4: the 2D origin the draw functions add
  std::int32_t originY = 0;
};

DrawGlobals readDrawGlobals(Core &core);

} // namespace crashbash::render
