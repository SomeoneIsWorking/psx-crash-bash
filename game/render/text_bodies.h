// game/render/text_bodies.h — the string and digit draws, FUN_800243A0 and FUN_800248A0, as bodies over their inputs.
//
// A body takes the call's arguments and what it reads besides them (the font's glyph rows, the string's bytes, the
// texture table, the tint) and draws through a `LeafPort`: each glyph or digit is one 2D leaf call. The guest
// override runs it over the guest's pool, a render over the same inputs with the position moved.
#pragma once

#include "draw_globals.h"
#include "leaf_port.h"
#include "state_bytes.h"

#include <cstdint>
#include <vector>

class Core;

namespace crashbash::render {

// What the font tables say about one byte of a string.
struct GlyphEntry {
  std::int16_t texture = 0; // -1: the byte draws nothing
  std::uint8_t character = 0;
  std::int8_t advance = 0;
  std::uint8_t flags = 0; // 1 sprite, 2 combining mark, 4 raised
  std::uint8_t reserved = 0;
};

struct StringBody {
  std::int16_t x = 0; // plain, or centred with an offset (placement_blend.h)
  std::int16_t y = 0;
  std::uint32_t colour = 0;
  std::uint32_t colour2 = 0;
  std::uint32_t tinted = 0; // the tint flag: the colours are shifted by `tintScale` before drawing
  std::int32_t tintScale = 0;
  std::uint32_t fontIndex = 0;
  std::int32_t lineHeight = 0; // the advance of byte 0 in the font, the step of a line feed
  std::uint32_t textureBase = 0;
  std::uint32_t address = 0;      // the string's own address: each glyph's element is its byte's
  std::vector<std::uint8_t> text; // up to the terminator
  std::vector<GlyphEntry> glyphs; // one per distinct byte of `text`
};

// A string draw's outcome: the result registers and the colours it leaves in the caller's stack slots.
struct StringOutcome {
  BodyResult result;
  std::uint32_t colour = 0;
  std::uint32_t colour2 = 0;
};

// The call being made (registers and stack at entry) and the memory it reads.
StringBody readStringBody(Core &core);
StringOutcome runString(const StringBody &body, const DrawGlobals &globals, LeafPort &port);
StringBody blendString(const StringBody &from, const StringBody &to, float t);
void writeString(psx::present::StateWriter &writer, const StringBody &body);
StringBody readString(psx::present::StateReader &reader);

struct NumberBody {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint32_t colour = 0;
  std::uint32_t colour2 = 0;
  std::uint32_t textureBase = 0;
  std::uint16_t digits[3]{}; // texture indices of the hundreds, tens and units glyphs
};

NumberBody readNumberBody(Core &core);
BodyResult runNumber(const NumberBody &body, const DrawGlobals &globals, LeafPort &port);
NumberBody blendNumber(const NumberBody &from, const NumberBody &to, float t);
void writeNumber(psx::present::StateWriter &writer, const NumberBody &body);
NumberBody readNumber(psx::present::StateReader &reader);

} // namespace crashbash::render
