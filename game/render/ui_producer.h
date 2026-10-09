// game/render/ui_producer.h — keys the guest's 2D UI packets by the element that draws them.
//
// Text, panels and screen quads are render components drawn through their +0x54 callbacks; the object
// is the component's incarnation (component_incarnation.h). The string, digit, panel and border draws they
// call are native bodies (text_bodies.h, panel_bodies.h) that name each glyph, digit place or panel part as
// they draw it. BOOT's HUD and menu page draw outside any component; there the call site names the owning
// record and is a producer of its own, its draws saved as that record's state.
#pragma once

#include "component_incarnation.h"

#include <cstdint>

class Core;

namespace crashbash::render {

// A glyph is its byte's main-RAM offset: the string's own address plus the glyph's index in it.
constexpr std::uint32_t glyphElement(std::uint32_t glyphByte) {
  return glyphByte & 0x1FFFFFu;
}

enum class PanelPart : std::uint32_t { Body, Left, Right, Top, Bottom };

// Digit places, counted from the units.
enum class DigitPlace : std::uint32_t { Units, Tens, Hundreds };

// The object a BOOT call site names: the record, and for a number's digits a place of it. Each is its own object, so
// each saves a state of its own.
constexpr std::uint32_t ownerObject(std::uint32_t record, DigitPlace place = DigitPlace::Units) {
  return (static_cast<std::uint32_t>(place) << kComponentOffsetBits) | (record & kComponentOffsetMask);
}

// The text, panel and quad component draws as producers, and the element namers on the 2D leaves.
void registerUiProducers(Core &core);

} // namespace crashbash::render
