// game/render/leaf_packets.h — the 2D draw leaves that build packets, as bodies over their arguments.
//
// FUN_80029D28 (a textured sprite), FUN_8002992C (a Gouraud textured quad) and FUN_8001A0D8 (a Gouraud
// untextured quad, projected through the GTE or laid out in 2D) take their position, colours and texture
// record as arguments, build one packet and link it into a bucket. `LeafCall` is a call's arguments and
// everything else its body reads; `buildLeaf` is the body over a packet writer. The guest override runs it over
// a packet bump-allocated from the pool, a render over host words with the position moved.
#pragma once

#include "draw_globals.h"
#include "emit_memory.h"
#include "gte_control.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace crashbash::render {

enum class LeafKind : std::uint32_t { Sprite, Quad, Shaded };

// The fields of a 0x38-byte texture record the sprite bodies read.
struct TextureRecord {
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::uint16_t tpage = 0;
  std::uint16_t clut = 0;
  std::array<std::uint16_t, 4> uv{};
  std::uint32_t advance = 0; // the byte at +0x10, in 640-column units
};

inline constexpr std::uint32_t kShadedVertexBytes = 0x30u;
// Set in a shaded quad's attributes when its corners are screen coordinates and not GTE vertices.
inline constexpr std::uint32_t kFlatLayout = 0x10000000u;
inline constexpr std::uint32_t kDrawn = 0x8000u;

struct LeafCall {
  LeafKind kind = LeafKind::Sprite;
  DrawGlobals globals;
  std::uint32_t table = 0;      // the named table `globals.otBase` is in
  std::uint32_t baseBucket = 0; // and its bucket there
  std::uint32_t textureAddress = 0;
  TextureRecord texture;
  std::uint32_t position = 0; // sprite and quad: y in the high half, x in the low
  std::uint32_t bucket = 0;   // sprite and quad: the bucket linked into
  std::array<std::uint32_t, 4> colours{};
  std::uint32_t attributes = 0;                            // shaded
  std::array<std::uint8_t, kShadedVertexBytes> vertices{}; // shaded: four corners and their colours
  std::uint32_t hasControl = 0;                            // shaded in the GTE layout: the transform
  psx::present::GteControl control{};
};

// What a body built: the words after the header it filled (the bucket to link into, when it links).
struct LeafResult {
  std::uint32_t value = 0;     // the function's return value
  std::uint32_t allocated = 0; // pool words the guest takes, header included
  std::uint32_t length = 0;    // command words in the packet
  std::optional<std::uint32_t> bucket;
  bool built = false;
};

// The arguments of the guest call being made (registers and stack at entry), and the memory it reads.
LeafCall readLeafCall(Core &core, LeafKind kind);

// Runs the body for `call`, writing the packet at `packet` in `memory`.
LeafResult buildLeaf(const psx::present::EmitMemory &memory, std::uint32_t packet, const LeafCall &call);

struct LeafExecution {
  std::uint32_t value = 0;  // the function's return value (v0)
  std::uint32_t second = 0; // what it leaves in v1
  bool setsSecond = false;  // false when it returned before touching v1
  std::uint32_t packet = 0; // the pool packet it linked; 0 when it linked none
};

// The guest function: takes its packet from the pool, builds it there and links it. The packet is written to
// guest memory.
LeafExecution executeLeaf(Core &core, const LeafCall &call);

} // namespace crashbash::render
