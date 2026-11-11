// draw_section.h — the gameplay process present 0x80010278 as a replay section: the root scene's present, which
// walks the scene machine's presents to every draw, without the DisplayFrame that follows it.
#pragma once

#include "replay_frame.h"

#include <cstdint>
#include <optional>

class Core;

namespace crashbash {

class DrawSection final : public psx::present::ReplaySection {
public:
  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core, psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;
};

} // namespace crashbash
