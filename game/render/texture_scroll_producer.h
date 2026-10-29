// game/render/texture_scroll_producer.h — a texture rect scrolled in VRAM by two copies per frame.
//
// FUN_80028F28(scroller, owner) advances the scroller's 8.8 position (+8) by its speed (+4), wrapped into
// the strip height H at table entry owner+8 -> [scroller+0] * 0x38 (+0x1C width, +0x1E H, +0x20 the
// destination's x,y), and links two DR_MOVEs at the head of the table's last bucket: rows p..H of the
// source (+0xC) to the destination's top, and rows 0..p to the rows below them, p the position's pixels.
// The Polar Push aurora is one.
#pragma once

class Core;

namespace psx::present {
class StateProducerTable;
}

namespace crashbash::render {

// FUN_80028F28 as a producer (object: the scroller) and its render, which moves p between frames.
void registerTextureScrollProducer(Core &core, psx::present::StateProducerTable &renders);

} // namespace crashbash::render
