// game/render/model_state.h — a mesh draw (FUN_800193A8) as a producer state, and its render.
//
// The state is what the body draws from: the GTE transform, the face list, the vertices the guest animated into
// its scratch buffer, the depth bias and limit, and the packets of the faces it linked (their colours,
// texture coordinates and pages, which the body does not write). The render runs `projectFaces` again over
// the transform and vertices `t` of the way between two states and puts each face the body links at that
// state, and the state kept, into the bucket the body gives it.
#pragma once

#include "gp0_primitive_decode.h"
#include "gte_control.h"
#include "render/draw_globals.h"
#include "render/face_projection.h"
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
  psx::present::GteControl control{};
  DrawGlobals globals;
  std::uint32_t table = 0;
  std::uint32_t baseBucket = 0;
  bool named = false; // `globals.otBase` is in a named table
  std::uint32_t packets = 0;
  std::vector<std::uint8_t> list;
  std::vector<std::uint32_t> vertices;
  std::vector<FaceEmit> linked;
};

// The call's inputs into `call`, from the registers and memory at entry; `call.linked` is emptied.
void readFaceCall(Core &core, FaceCall &call);

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

struct SavedFace {
  std::uint32_t face = 0;
  std::uint32_t layout = 0;
  std::uint32_t bucket = 0;
  FaceWords words{};
};
static_assert(std::is_trivially_copyable_v<SavedFace>);

struct FaceStateHeader {
  psx::present::GteControl control{};
  std::int32_t zBias = 0;
  std::int32_t zLimit = 0;
  std::uint32_t table = 0;
  std::uint32_t baseBucket = 0;
  std::uint32_t listBytes = 0;
  std::uint32_t vertexWords = 0;
  std::uint32_t faces = 0;
};
static_assert(std::is_trivially_copyable_v<FaceStateHeader>);

struct FaceState {
  FaceStateHeader header;
  std::vector<std::uint8_t> list;
  std::vector<std::uint32_t> vertices;
  std::vector<SavedFace> faces;

  // Reads a saved face state into this one, keeping its capacity.
  void read(std::span<const std::byte> bytes);
};

// A face a render links: the saved packet with the points the GTE projected.
struct LinkedFace {
  std::uint32_t bucket = 0;
  psx::gpu::PacketBuffer<kFacePacketWords> words;
};

// What face saves and renders work in, kept by the session so they allocate none.
struct FaceScratch {
  FaceState to;
  FaceState from;
  std::vector<std::uint32_t> vertices;
  std::vector<std::size_t> byFace;
  std::vector<LinkedFace> linked;
  std::vector<SavedFace> saved;

  void clear() {
    vertices.clear();
    byFace.clear();
    linked.clear();
    saved.clear();
  }
};

} // namespace crashbash::render
