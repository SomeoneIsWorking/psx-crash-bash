// game/render/leaf_state.h — the 2D draws one object made, as a producer state, and its render.
//
// An object's 2D draws are leaf calls the guest made directly (a screen quad) and component bodies (a string, a
// digit set, a panel, a border) that made leaf calls of their own. The state holds each leaf call's arguments and
// what its body reads besides them (display scale, fade, depth bias, the texture record, a shaded quad's corners
// and GTE transform), and each component body's inputs (component_body.h) with what its leaves shared. The render
// draws every leaf call, and runs every component body over inputs `t` of the way between two states, building
// a host packet per leaf and putting it where the leaf body links it.
#pragma once

#include "component_body.h"
#include "frame_record.h"
#include "leaf_packets.h"
#include "state_producer.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

class Core;

namespace crashbash::render {

inline constexpr std::uint32_t kLeafStateTag = 2u;

// A draw an object made: a body with the leaves it called, or one leaf call the guest made itself.
struct UiNote {
  std::optional<ComponentBody> body;
  std::vector<LeafNote> leaves;
};

// Saves the draws whose packets are all still on the walk of the table in use. Saves nothing when none are.
void saveLeafState(Core &core, const psx::present::RecordKey &owner, std::span<const UiNote> notes);

void renderLeafState(Core &core,
                     std::span<const std::byte> from,
                     std::span<const std::byte> to,
                     float t,
                     psx::present::PrimitiveSink &sink);

} // namespace crashbash::render
