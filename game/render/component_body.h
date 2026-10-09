// game/render/component_body.h — the four native component bodies as one value a state can hold.
//
// The string, digit, panel and border draws (text_bodies.h, panel_bodies.h) are the guest functions
// 0x800243A0, 0x800248A0, 0x8001A6D4 and 0x8001A43C. An object that called one saves its inputs; the render runs the
// body again over inputs `t` of the way from the earlier state.
#pragma once

#include "draw_globals.h"
#include "leaf_port.h"
#include "panel_bodies.h"
#include "state_bytes.h"
#include "text_bodies.h"

#include <variant>

namespace crashbash::render {

using ComponentBody = std::variant<StringBody, NumberBody, PanelBody, BorderBody>;

BodyResult runBody(const ComponentBody &body, const DrawGlobals &globals, LeafPort &port);
// Whether `from` and `to` are the same kind of body, so the render can move one to the other.
bool sameKind(const ComponentBody &from, const ComponentBody &to);
// `to` with the position and size `t` of the way from `from`'s; the two must pair.
ComponentBody blendBody(const ComponentBody &from, const ComponentBody &to, float t);
void writeBody(psx::present::StateWriter &writer, const ComponentBody &body);
ComponentBody readBody(psx::present::StateReader &reader);

} // namespace crashbash::render
