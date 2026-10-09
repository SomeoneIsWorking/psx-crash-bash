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

class GuestPacket final : public PacketWriter {
public:
  GuestPacket(Core &core, std::uint32_t base) : core_(core), base_(base) {}
  void w8(std::uint32_t offset, std::uint32_t value) override {
    core_.mem_w8(base_ + offset, static_cast<std::uint8_t>(value));
  }
  void w16(std::uint32_t offset, std::uint32_t value) override {
    core_.mem_w16(base_ + offset, static_cast<std::uint16_t>(value));
  }
  void w32(std::uint32_t offset, std::uint32_t value) override {
    core_.mem_w32(base_ + offset, value);
  }
  void storeXy(std::uint32_t offset, std::uint32_t reg) override {
    gte_store_xy(&core_, base_ + offset, static_cast<int>(reg));
  }
  std::uint32_t r32(std::uint32_t offset) override {
    return core_.mem_r32(base_ + offset);
  }

private:
  Core &core_;
  std::uint32_t base_;
};

std::int32_t divide(std::int32_t value, std::int32_t divisor) {
  if (divisor == 0) {
    lucent::error("leaf-packets", "a layout division by a display scale of 0");
    std::abort();
  }
  return value / divisor;
}

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
  corners.topLeft =
      (static_cast<std::uint32_t>(divide(static_cast<std::int16_t>(position) * globals.screenScale, kLayoutWidth)) &
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

LeafResult buildSprite(Core &, PacketWriter &packet, const LeafCall &call) {
  const DrawGlobals &globals = call.globals;
  const TextureRecord &texture = call.texture;
  const std::uint32_t colour = call.colours[0];
  const Corners corners = spriteCorners(globals, texture, call.position);
  packet.w32(4, fadedColour(globals, colour));
  packet.w8(3, 9);
  packet.w8(7, 0x2C);
  packet.w8(7, static_cast<std::int32_t>(colour) < 0 ? 0x2E : 0x2C);
  packet.w16(0xC, texture.uv[0]);
  packet.w16(0x14, texture.uv[1]);
  packet.w16(0x1C, texture.uv[2]);
  packet.w16(0x24, texture.uv[3]);
  packet.w16(0x16, texturePage(texture, colour));
  packet.w32(8, corners.topLeft);
  packet.w16(0xE, texture.clut);
  packet.w32(0x10, corners.topRight);
  packet.w32(0x20, corners.bottomRight);
  packet.w32(0x18, corners.bottomLeft);
  LeafResult result;
  result.allocated = 10;
  result.length = 9;
  result.bucket = call.bucket;
  result.built = true;
  result.value = static_cast<std::uint32_t>(
      divide(static_cast<std::int32_t>(texture.advance) * kLayoutWidth, globals.screenScale));
  return result;
}

LeafResult buildQuad(Core &, PacketWriter &packet, const LeafCall &call) {
  const DrawGlobals &globals = call.globals;
  const TextureRecord &texture = call.texture;
  const std::uint32_t colour = call.colours[0];
  const Corners corners = spriteCorners(globals, texture, call.position);
  packet.w32(4, fadedColour(globals, call.colours[0]));
  packet.w32(0x10, fadedColour(globals, call.colours[1]));
  packet.w32(0x1C, fadedColour(globals, call.colours[2]));
  packet.w32(0x28, fadedColour(globals, call.colours[3]));
  packet.w8(3, 0xC);
  packet.w8(7, 0x3C);
  packet.w8(7, static_cast<std::int32_t>(colour) < 0 ? 0x3E : 0x3C);
  packet.w16(0xC, texture.uv[0]);
  packet.w16(0x18, texture.uv[1]);
  packet.w16(0x24, texture.uv[2]);
  packet.w16(0x30, texture.uv[3]);
  packet.w16(0x1A, texturePage(texture, colour));
  packet.w32(8, corners.topLeft);
  packet.w16(0xE, texture.clut);
  packet.w32(0x14, corners.topRight);
  packet.w32(0x2C, corners.bottomRight);
  packet.w32(0x20, corners.bottomLeft);
  LeafResult result;
  result.allocated = 13;
  result.length = 12;
  result.bucket = call.bucket;
  result.built = true;
  result.value = static_cast<std::uint32_t>(
      divide(static_cast<std::int32_t>(texture.advance) * kLayoutWidth, globals.screenScale));
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
  return static_cast<std::uint32_t>(divide(value, 640)) & 0xFFFFu;
}

std::uint32_t flatY(const LeafCall &call, std::uint32_t offset) {
  return static_cast<std::uint32_t>((call.globals.originY + vertexHalf(call, offset)) / 2) & 0xFFFFu;
}

LeafResult buildShaded(Core &core, PacketWriter &packet, const LeafCall &call) {
  LeafResult result;
  result.allocated = kShadedWords;
  if ((call.attributes & kDrawn) == 0u) {
    return result;
  }
  const std::uint32_t attributes = call.attributes;
  packet.w32(0xC, shadedColour(call, 0));
  packet.w32(0x14, shadedColour(call, 1));
  packet.w32(0x1C, shadedColour(call, 2));
  packet.w32(0x24, shadedColour(call, 3));
  packet.w8(3, kShadedLength);
  packet.w8(0xF, kShadedCommand);
  packet.w32(4, kShadedDrawMode);
  packet.w32(8, 0);
  if ((attributes & kSemiTransparent) != 0u) {
    packet.w32(4, (attributes & kShadedDrawModeBits) | kShadedDrawMode);
    packet.w32(0xC, packet.r32(0xC) | kShadedSemiTransparent);
  }
  std::int32_t depth = 0;
  if ((attributes & kFlatLayout) == 0u) {
    gte_write_data(gte::kVxy0, vertexWord(call, 0));
    gte_write_data(gte::kVz0, vertexWord(call, 4));
    gte_write_data(gte::kVxy1, vertexWord(call, 8));
    gte_write_data(gte::kVz1, vertexWord(call, 12));
    gte_write_data(gte::kVxy2, vertexWord(call, 16));
    gte_write_data(gte::kVz2, vertexWord(call, 20));
    gte_op(&core, gte::kRtpt);
    gte_op(&core, gte::kAvsz3);
    depth = static_cast<std::int32_t>(gte_read_data(gte::kOtz));
    packet.storeXy(0x10, gte::kSxy0);
    packet.storeXy(0x18, gte::kSxy1);
    packet.storeXy(0x20, gte::kSxy2);
    gte_write_data(gte::kVxy0, vertexWord(call, 24));
    gte_write_data(gte::kVz0, vertexWord(call, 28));
    gte_op(&core, gte::kRtps);
    packet.storeXy(0x28, gte::kSxy2);
  } else {
    constexpr std::uint32_t kCorners[4] = {0u, 8u, 16u, 24u};
    constexpr std::uint32_t kPoints[4] = {0x10u, 0x18u, 0x20u, 0x28u};
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      packet.w16(kPoints[corner], flatX(call, kCorners[corner]));
      packet.w16(kPoints[corner] + 2u, flatY(call, kCorners[corner] + 2u));
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

LeafCall readLeafCall(Core &core, LeafKind kind) {
  LeafCall call;
  call.kind = kind;
  call.globals = readDrawGlobals(core);
  if (const auto slot = core.otTables.slotOf(call.globals.otBase)) {
    call.table = slot->table;
    call.baseBucket = slot->index;
  } else {
    call.table = ~0u;
  }
  if (kind == LeafKind::Shaded) {
    const std::uint32_t vertices = core.r[4];
    call.attributes = core.r[5];
    for (std::uint32_t byte = 0; byte < kShadedVertexBytes; ++byte) {
      call.vertices[byte] = core.mem_r8(vertices + byte);
    }
    if ((call.attributes & kDrawn) != 0u && (call.attributes & kFlatLayout) == 0u) {
      call.hasControl = 1u;
      call.control = gte::readControl();
    }
    return call;
  }
  call.textureAddress = core.r[4];
  call.position = core.r[5];
  call.bucket = core.r[6];
  call.colours[0] = core.r[7];
  if (kind == LeafKind::Quad) {
    for (std::uint32_t colour = 1; colour < 4u; ++colour) {
      call.colours[colour] = core.mem_r32(core.r[29] + kStackArguments + (colour - 1u) * kWordBytes);
    }
  }
  if (call.globals.hasEnvironment != 0u) {
    call.texture = readTexture(core, call.textureAddress);
  }
  return call;
}

LeafResult buildLeaf(Core &core, PacketWriter &writer, const LeafCall &call) {
  switch (call.kind) {
  case LeafKind::Sprite:
    return buildSprite(core, writer, call);
  case LeafKind::Quad:
    return buildQuad(core, writer, call);
  case LeafKind::Shaded:
    break;
  }
  return buildShaded(core, writer, call);
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
  GuestPacket writer(core, packet);
  const LeafResult result = buildLeaf(core, writer, call);
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
