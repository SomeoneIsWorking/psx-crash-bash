// game/render/ui_producer.h — keys the guest's 2D UI packets by the element that draws them.
//
// Text, panels and screen quads are render components drawn through their +0x54 callbacks; the object
// is the component's incarnation (component_incarnation.h). The 2D leaves take only an image and a
// position, so each element is named at the leaf from the call site it returns to: a glyph by its byte
// in the string, a panel part or a digit place by the site that draws it. BOOT's HUD and menu page draw
// outside any component; there the call site also names the owning record.
#pragma once

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

// The text, panel and quad component draws as producers, and the element namers on the 2D leaves.
void registerUiProducers(Core &core);

} // namespace crashbash::render
