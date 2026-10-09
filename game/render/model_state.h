// game/render/model_state.h — a mesh draw (FUN_800193A8) as a producer state, and its render.
//
// The state is what the body draws from: the GTE transform, the face list, the vertices the guest animated into
// its scratch buffer, the depth bias and limit, and the packets of the faces it linked (their colours,
// texture coordinates and pages, which the body does not write). The render runs `projectFaces` again over
// the transform and vertices `t` of the way between two states and puts each face the body links at that
// state, and the state kept, into the bucket the body gives it.
#pragma once

#include "draw_globals.h"
#include "face_projection.h"
#include "gte_access.h"
#include "state_bytes.h"
#include "state_producer.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

class Core;

namespace crashbash::render {

// A face packet as the guest left it after linking.
using FaceWords = std::array<std::uint32_t, kFacePacketWords>;

// One call of the mesh body: its inputs at entry and the faces it linked.
struct FaceCall {
  gte::Control control{};
  DrawGlobals globals;
  std::uint32_t table = 0;
  std::uint32_t baseBucket = 0;
  bool named = false; // `globals.otBase` is in a named table
  std::uint32_t packets = 0;
  std::vector<std::uint8_t> list;
  std::vector<std::uint32_t> vertices;
  std::vector<FaceEmit> linked;
};

// The call's inputs, from the registers and memory at entry.
FaceCall readFaceCall(Core &core);

// Keeps the faces of `call` that are still on the walk of the table in use when the guest draws it.
// Returns false when the call cannot be saved: its table is not named or a face is not a drawable primitive.
bool saveFaceState(Core &core, const psx::present::RecordKey &owner, const FaceCall &call);

void renderFaceState(Core &core,
                     std::span<const std::byte> from,
                     std::span<const std::byte> to,
                     float t,
                     psx::present::PrimitiveSink &sink);

// Marks a state as a face state; `renderState` reads it back.
inline constexpr std::uint32_t kFaceStateTag = 1u;

} // namespace crashbash::render
