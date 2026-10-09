// game/render/leaf_state.h — the 2D draw leaves one object called, as a producer state, and its render.
//
// The state is each leaf call's arguments and what its body reads besides them (display scale, fade, depth
// bias, the texture record, a shaded quad's corners and GTE transform). The render runs `buildLeaf` over a host
// packet per call with the position, corners and transform `t` of the way between two states, and puts the
// packet where the body links it.
#pragma once

#include "frame_record.h"
#include "leaf_packets.h"
#include "state_producer.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class Core;

namespace crashbash::render {

inline constexpr std::uint32_t kLeafStateTag = 2u;

// A leaf call and the pool packet it linked.
struct LeafNote {
  LeafCall call;
  std::uint32_t packet = 0;
};

// Saves the calls whose packets are still on the walk of the table in use. Saves nothing when none are.
void saveLeafState(Core &core, const psx::present::RecordKey &owner, std::span<const LeafNote> notes);

void renderLeafState(Core &core,
                     std::span<const std::byte> from,
                     std::span<const std::byte> to,
                     float t,
                     psx::present::PrimitiveSink &sink);

} // namespace crashbash::render
