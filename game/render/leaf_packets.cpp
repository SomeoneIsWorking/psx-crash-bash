#include "leaf_packets.h"

#include "core.h"
#include "crashbash_guest.h"
#include <lucent/log.h>

#include <cstdlib>

namespace crashbash::render {
namespace {

constexpr std::uint32_t kWordBytes = 4u;
constexpr std::uint32_t kLinkMask = 0xFFFFFFu;
constexpr std::int32_t kLayoutWidth = 0x280;
constexpr std::uint32_t kFadeOne = 0x1000u;
constexpr std::uint32_t kSemiTransparent = 0x21u;
constexpr std::uint32_t kTexturePageKeep = 0xFF9Fu;
constexpr std::uint32_t kTexturePageBits = 0x60u;
constexpr std::uint32_t kTexturePageShift = 0x13u;
constexpr std::uint32_t kStackArguments = 16u;

// A texture record's offsets.
constexpr std::uint32_t kTextureWidth = 8u;
constexpr std::uint32_t kTextureHeight = 10u;
constexpr std::uint32_t kTextureAdvance = 0x10u;
constexpr std::uint32_t kTexturePage = 0x24u;
constexpr std::uint32_t kTextureClut = 0x26u;
constexpr std::uint32_t kTextureUv = 0x28u;

// A shaded quad's corner colours.
constexpr std::uint32_t kShadedColours = 0x20u;
constexpr std::uint32_t kShadedColourStride = 4u;
constexpr std::uint32_t kShadedDrawMode = 0xE1000200u;
constexpr std::uint32_t kShadedLength = 10u;
constexpr std::uint32_t kShadedWords = 11u;
constexpr std::uint32_t kShadedCommand = 0x38u;
constexpr std::uint32_t kShadedSemiTransparent = 0x32000000u;
constexpr std::uint32_t kShadedDrawModeBits = 0x60u;

// A colour with the screen fade applied, as the guest packs it: blue, green, red.
std::uint32_t fadedColour(const DrawGlobals &globals, std::uint32_t colour) {
  std::uint32_t red = colour & 0xFFu;
  std::uint32_t green = (colour >> 8) & 0xFFu;
  std::uint32_t blue = (colour >> 16) & 0xFFu;
  if (globals.fade != 0u) {
    const auto keep = static_cast<std::int32_t>(kFadeOne - globals.fade);
    red = static_cast<std::uint32_t>(static_cast<std::int32_t>(red) * keep >> 12) & 0xFFu;
    green = static_cast<std::uint32_t>(static_cast<std::int32_t>(green) * keep >> 12) & 0xFFu;
    blue = static_cast<std::uint32_t>(static_cast<std::int32_t>(blue) * keep >> 12) & 0xFFu;
  }
  return blue << 16 | green << 8 | red;
}

// The sprite bodies' corners: the position word's x scaled to the display, its y halved, then the sprite's
// size added.
struct Corners {
  std::uint32_t topLeft = 0; // y high, x low
  std::uint32_t topRight = 0;
  std::uint32_t bottomLeft = 0;
  std::uint32_t bottomRight = 0;
};

Corners spriteCorners(const DrawGlobals &globals, const TextureRecord &texture, std::uint32_t position) {
  const std::int32_t high = static_cast<std::int32_t>(position & 0xFFFF0000u);
  const std::uint32_t halfY = static_cast<std::uint32_t>(((high >> 16) - (high >> 31)) >> 1) << 16;
  Corners corners;
  corners.topLeft = (static_cast<std::uint32_t>(
                         layoutDivide(static_cast<std::int16_t>(position) * globals.screenScale, kLayoutWidth)) &
                     0xFFFFu) |
                    halfY;
  std::uint32_t right = (texture.width + (corners.topLeft - 1u)) & 0xFFFFu;
  corners.topRight = halfY | right;
  const std::uint32_t bottom = (static_cast<std::uint32_t>(texture.height) +
                                static_cast<std::uint32_t>(static_cast<std::int32_t>(corners.topRight) >> 16) - 1u)
                               << 16;
  right |= bottom;
  corners.bottomRight = right;
  corners.bottomLeft = bottom | ((right + 1u - texture.width) & 0xFFFFu);
  return corners;
}

std::uint32_t texturePage(const TextureRecord &texture, std::uint32_t colour) {
  return (texture.tpage & kTexturePageKeep) | ((colour >> kTexturePageShift) & kTexturePageBits);
}

LeafResult buildSprite(const psx::present::EmitMemory &memory, std::uint32_t packet, const LeafCall &call) {
  const DrawGlobals &globals = call.globals;
  const TextureRecord &texture = call.texture;
  const std::uint32_t colour = call.colours[0];
  const Corners corners = spriteCorners(globals, texture, call.position);
  memory.mem_w32(packet + 4, fadedColour(globals, colour));
  memory.mem_w8(packet + 3, 9);
  memory.mem_w8(packet + 7, 0x2C);
  memory.mem_w8(packet + 7, static_cast<std::int32_t>(colour) < 0 ? 0x2E : 0x2C);
  memory.mem_w16(packet + 0xC, texture.uv[0]);
  memory.mem_w16(packet + 0x14, texture.uv[1]);
  memory.mem_w16(packet + 0x1C, texture.uv[2]);
  memory.mem_w16(packet + 0x24, texture.uv[3]);
  memory.mem_w16(packet + 0x16, texturePage(texture, colour));
  memory.mem_w32(packet + 8, corners.topLeft);
  memory.mem_w16(packet + 0xE, texture.clut);
  memory.mem_w32(packet + 0x10, corners.topRight);
  memory.mem_w32(packet + 0x20, corners.bottomRight);
  memory.mem_w32(packet + 0x18, corners.bottomLeft);
  LeafResult result;
  result.allocated = 10;
  result.length = 9;
  result.bucket = call.bucket;
  result.built = true;
  result.value = static_cast<std::uint32_t>(
      layoutDivide(static_cast<std::int32_t>(texture.advance) * kLayoutWidth, globals.screenScale));
  return result;
}

LeafResult buildQuad(const psx::present::EmitMemory &memory, std::uint32_t packet, const LeafCall &call) {
  const DrawGlobals &globals = call.globals;
  const TextureRecord &texture = call.texture;
  const std::uint32_t colour = call.colours[0];
  const Corners corners = spriteCorners(globals, texture, call.position);
  memory.mem_w32(packet + 4, fadedColour(globals, call.colours[0]));
  memory.mem_w32(packet + 0x10, fadedColour(globals, call.colours[1]));
  memory.mem_w32(packet + 0x1C, fadedColour(globals, call.colours[2]));
  memory.mem_w32(packet + 0x28, fadedColour(globals, call.colours[3]));
  memory.mem_w8(packet + 3, 0xC);
  memory.mem_w8(packet + 7, 0x3C);
  memory.mem_w8(packet + 7, static_cast<std::int32_t>(colour) < 0 ? 0x3E : 0x3C);
  memory.mem_w16(packet + 0xC, texture.uv[0]);
  memory.mem_w16(packet + 0x18, texture.uv[1]);
  memory.mem_w16(packet + 0x24, texture.uv[2]);
  memory.mem_w16(packet + 0x30, texture.uv[3]);
  memory.mem_w16(packet + 0x1A, texturePage(texture, colour));
  memory.mem_w32(packet + 8, corners.topLeft);
  memory.mem_w16(packet + 0xE, texture.clut);
  memory.mem_w32(packet + 0x14, corners.topRight);
  memory.mem_w32(packet + 0x2C, corners.bottomRight);
  memory.mem_w32(packet + 0x20, corners.bottomLeft);
  LeafResult result;
  result.allocated = 13;
  result.length = 12;
  result.bucket = call.bucket;
  result.built = true;
  result.value = static_cast<std::uint32_t>(
      layoutDivide(static_cast<std::int32_t>(texture.advance) * kLayoutWidth, globals.screenScale));
  return result;
}

std::uint32_t vertexWord(const LeafCall &call, std::uint32_t offset) {
  std::uint32_t word = 0;
  for (std::uint32_t byte = 0; byte < 4u; ++byte) {
    word |= static_cast<std::uint32_t>(call.vertices[offset + byte]) << (byte * 8u);
  }
  return word;
}

std::int32_t vertexHalf(const LeafCall &call, std::uint32_t offset) {
  return static_cast<std::int16_t>(vertexWord(call, offset) & 0xFFFFu);
}

std::uint32_t shadedColour(const LeafCall &call, std::uint32_t index) {
  const std::uint32_t at = kShadedColours + index * kShadedColourStride;
  return fadedColour(call.globals,
                     static_cast<std::uint32_t>(call.vertices[at]) |
                         static_cast<std::uint32_t>(call.vertices[at + 1]) << 8 |
                         static_cast<std::uint32_t>(call.vertices[at + 2]) << 16);
}

// A flat-layout corner: the origin and the guest's coordinate scaled from the 640-column layout.
std::uint32_t flatX(const LeafCall &call, std::uint32_t offset) {
  const std::int32_t value = (call.globals.originX + vertexHalf(call, offset)) * call.globals.screenScale;
  return static_cast<std::uint32_t>(layoutDivide(value, 640)) & 0xFFFFu;
}

std::uint32_t flatY(const LeafCall &call, std::uint32_t offset) {
  return static_cast<std::uint32_t>((call.globals.originY + vertexHalf(call, offset)) / 2) & 0xFFFFu;
}

LeafResult buildShaded(const psx::present::EmitMemory &memory, std::uint32_t packet, const LeafCall &call) {
  LeafResult result;
  result.allocated = kShadedWords;
  if ((call.attributes & kDrawn) == 0u) {
    return result;
  }
  const std::uint32_t attributes = call.attributes;
  memory.mem_w32(packet + 0xC, shadedColour(call, 0));
  memory.mem_w32(packet + 0x14, shadedColour(call, 1));
  memory.mem_w32(packet + 0x1C, shadedColour(call, 2));
  memory.mem_w32(packet + 0x24, shadedColour(call, 3));
  memory.mem_w8(packet + 3, kShadedLength);
  memory.mem_w8(packet + 0xF, kShadedCommand);
  memory.mem_w32(packet + 4, kShadedDrawMode);
  memory.mem_w32(packet + 8, 0);
  if ((attributes & kSemiTransparent) != 0u) {
    memory.mem_w32(packet + 4, (attributes & kShadedDrawModeBits) | kShadedDrawMode);
    memory.mem_w32(packet + 0xC, memory.mem_r32(packet + 0xC) | kShadedSemiTransparent);
  }
  std::int32_t depth = 0;
  if ((attributes & kFlatLayout) == 0u) {
    gte_write_data(psx::gte::kVxy0, vertexWord(call, 0));
    gte_write_data(psx::gte::kVz0, vertexWord(call, 4));
    gte_write_data(psx::gte::kVxy1, vertexWord(call, 8));
    gte_write_data(psx::gte::kVz1, vertexWord(call, 12));
    gte_write_data(psx::gte::kVxy2, vertexWord(call, 16));
    gte_write_data(psx::gte::kVz2, vertexWord(call, 20));
    gte_op(&memory.core(), psx::gte::kRtpt);
    gte_op(&memory.core(), psx::gte::kAvsz3);
    depth = static_cast<std::int32_t>(gte_read_data(psx::gte::kOtz));
    memory.storeGteXy(packet + 0x10, psx::gte::kSxy0);
    memory.storeGteXy(packet + 0x18, psx::gte::kSxy1);
    memory.storeGteXy(packet + 0x20, psx::gte::kSxy2);
    gte_write_data(psx::gte::kVxy0, vertexWord(call, 24));
    gte_write_data(psx::gte::kVz0, vertexWord(call, 28));
    gte_op(&memory.core(), psx::gte::kRtps);
    memory.storeGteXy(packet + 0x28, psx::gte::kSxy2);
  } else {
    constexpr std::uint32_t kCorners[4] = {0u, 8u, 16u, 24u};
    constexpr std::uint32_t kPoints[4] = {0x10u, 0x18u, 0x20u, 0x28u};
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      memory.mem_w16(packet + kPoints[corner], flatX(call, kCorners[corner]));
      memory.mem_w16(packet + kPoints[corner] + 2u, flatY(call, kCorners[corner] + 2u));
    }
  }
  const std::uint32_t bucket = static_cast<std::uint32_t>(depth + call.globals.zBias) >> 1;
  result.length = kShadedLength;
  result.built = true;
  if (bucket < static_cast<std::uint32_t>(call.globals.zLimit)) {
    result.bucket = bucket;
  }
  return result;
}

// Links `packet` at the head of the bucket; returns its header as linked.
std::uint32_t link(Core &core, std::uint32_t packet, std::uint32_t otBase, std::uint32_t bucket) {
  const std::uint32_t slot = otBase + bucket * kWordBytes;
  const std::uint32_t header = (core.mem_r32(packet) & ~kLinkMask) | (core.mem_r32(slot) & kLinkMask);
  core.mem_w32(packet, header);
  core.mem_w32(slot, (core.mem_r32(slot) & ~kLinkMask) | (packet & kLinkMask));
  return header;
}

TextureRecord readTexture(Core &core, std::uint32_t address) {
  TextureRecord texture;
  texture.width = core.mem_r16(address + kTextureWidth);
  texture.height = core.mem_r16(address + kTextureHeight);
  texture.tpage = core.mem_r16(address + kTexturePage);
  texture.clut = core.mem_r16(address + kTextureClut);
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    texture.uv[corner] = core.mem_r16(address + kTextureUv + corner * 2u);
  }
  texture.advance = core.mem_r8(address + kTextureAdvance);
  return texture;
}

} // namespace

std::int32_t layoutDivide(std::int32_t value, std::int32_t divisor) {
  if (divisor == 0) {
    lucent::error("leaf-packets", "a layout division by a display scale of 0");
    std::abort();
  }
  return value / divisor;
}

void completeLeafCall(Core &core, LeafCall &call) {
  call.globals = readDrawGlobals(core);
  if (const auto slot = core.otTables.slotOf(call.globals.otBase)) {
    call.table = slot->table;
    call.baseBucket = slot->index;
  } else {
    call.table = ~0u;
  }
  if (call.kind == LeafKind::Shaded) {
    if ((call.attributes & kDrawn) != 0u && (call.attributes & kFlatLayout) == 0u) {
      call.hasControl = 1u;
      call.control = psx::present::readGteControl();
    }
  } else if (call.globals.hasEnvironment != 0u) {
    call.texture = readTexture(core, call.textureAddress);
  }
}

LeafCall readLeafCall(Core &core, LeafKind kind) {
  LeafCall call;
  call.kind = kind;
  if (kind == LeafKind::Shaded) {
    const std::uint32_t vertices = core.r[4];
    call.attributes = core.r[5];
    for (std::uint32_t byte = 0; byte < kShadedVertexBytes; ++byte) {
      call.vertices[byte] = core.mem_r8(vertices + byte);
    }
  } else {
    call.textureAddress = core.r[4];
    call.position = core.r[5];
    call.bucket = core.r[6];
    call.colours[0] = core.r[7];
    if (kind == LeafKind::Quad) {
      for (std::uint32_t colour = 1; colour < 4u; ++colour) {
        call.colours[colour] = core.mem_r32(core.r[29] + kStackArguments + (colour - 1u) * kWordBytes);
      }
    }
  }
  completeLeafCall(core, call);
  return call;
}

LeafResult buildLeaf(const psx::present::EmitMemory &memory, std::uint32_t packet, const LeafCall &call) {
  switch (call.kind) {
  case LeafKind::Sprite:
    return buildSprite(memory, packet, call);
  case LeafKind::Quad:
    return buildQuad(memory, packet, call);
  case LeafKind::Shaded:
    break;
  }
  return buildShaded(memory, packet, call);
}

LeafExecution executeLeaf(Core &core, const LeafCall &call) {
  if (call.kind != LeafKind::Shaded && call.globals.hasEnvironment == 0u) {
    return {};
  }
  const std::uint32_t table = core.mem_r32(guest::kOrderingTablePointer);
  const std::uint32_t cursorAt = table + guest::kOrderingTablePoolCursor;
  const std::uint32_t packet = core.mem_r32(cursorAt);
  const std::uint32_t words = call.kind == LeafKind::Sprite ? 10u : call.kind == LeafKind::Quad ? 13u : kShadedWords;
  core.mem_w32(cursorAt, packet + words * kWordBytes);
  const LeafResult result = buildLeaf(psx::present::EmitMemory(core), packet, call);
  const std::uint32_t header = result.bucket ? link(core, packet, call.globals.otBase, *result.bucket) : 0u;
  LeafExecution done;
  done.setsSecond = true;
  done.packet = result.bucket ? packet : 0u;
  if (call.kind == LeafKind::Shaded) {
    done.value = result.built ? packet : 0u;
    done.second = result.built ? header : table;
  } else {
    // The sprite bodies end on the display scale they divided by.
    done.value = result.value;
    done.second = static_cast<std::uint32_t>(call.globals.screenScale);
  }
  return done;
}

} // namespace crashbash::render
