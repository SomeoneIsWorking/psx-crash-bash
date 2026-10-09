// game/render/panel_bodies.h — the panel and border draws, FUN_8001A6D4 and FUN_8001A43C, as bodies over their inputs.
//
// A panel is a record of flags, a rectangle, a colour and a border; its body is one Gouraud quad and, with the
// border flag set, a border draw of its own (four strips). Each strip and the quad is a shaded 2D leaf the body
// lays out from halfwords.
#pragma once

#include "draw_globals.h"
#include "leaf_port.h"
#include "state_bytes.h"

#include <array>
#include <cstdint>

class Core;

namespace crashbash::render {

// The record's words, which the body reads as halfwords: +4 x, +6 y, +8 depth, +10 right edge or width, +12 bottom,
// +0x10 colour, +0x14 border colour, +0x18 and +0x1A border sizes, +0x1C border flags.
struct PanelBody {
  std::array<std::uint32_t, 8> record{};
  std::uint32_t attributes = 0;
  std::uint32_t address = 0; // the record's own, where its border draw reads the rectangle
};

PanelBody readPanelBody(Core &core);
BodyResult runPanel(const PanelBody &body, const DrawGlobals &globals, LeafPort &port);
PanelBody blendPanel(const PanelBody &from, const PanelBody &to, float t);
void writePanel(psx::present::StateWriter &writer, const PanelBody &body);
PanelBody readPanel(psx::present::StateReader &reader);

// The four strips around the rectangle at `edge`: x, y, depth, right edge or width, bottom.
struct BorderBody {
  std::array<std::int16_t, 5> edge{};
  std::int16_t reserved = 0;
  std::uint32_t colour = 0;
  std::uint32_t attributes = 0;
  std::int32_t sideWidth = 0;
  std::int32_t edgeHeight = 0;
};

BorderBody readBorderBody(Core &core);
BodyResult runBorder(const BorderBody &body, const DrawGlobals &globals, LeafPort &port);
BorderBody blendBorder(const BorderBody &from, const BorderBody &to, float t);
void writeBorder(psx::present::StateWriter &writer, const BorderBody &body);
BorderBody readBorder(psx::present::StateReader &reader);

} // namespace crashbash::render
