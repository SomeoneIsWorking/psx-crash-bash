#include "panel_bodies.h"

#include "core.h"
#include "placement_blend.h"
#include "ui_producer.h"

#include <cstring>

namespace crashbash::render {
namespace {

constexpr std::int32_t kLayoutBase = 0x140;
constexpr std::uint32_t kRecordWords = 8u;
constexpr std::uint32_t kBorderFlags = 7u;
constexpr std::uint32_t kBorderColour = 5u;
constexpr std::uint32_t kBorderEdge = 4u; // the border reads the rectangle from the record's +4

// The vertex array of a shaded quad: four (x, y, depth) words and four colours.
class QuadVertices {
public:
  void point(std::uint32_t corner, std::int32_t x, std::int32_t y, std::int32_t depth) {
    half(corner * 8u, x);
    half(corner * 8u + 2u, y);
    half(corner * 8u + 4u, depth);
  }
  void colour(std::uint32_t colour) {
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      std::memcpy(bytes_.data() + 0x20u + corner * 4u, &colour, sizeof(colour));
    }
  }
  const std::array<std::uint8_t, kShadedVertexBytes> &bytes() const {
    return bytes_;
  }

private:
  void half(std::uint32_t offset, std::int32_t value) {
    const auto stored = static_cast<std::uint16_t>(value);
    std::memcpy(bytes_.data() + offset, &stored, sizeof(stored));
  }

  std::array<std::uint8_t, kShadedVertexBytes> bytes_{};
};

std::int16_t halfOf(const std::array<std::uint32_t, kRecordWords> &record, std::uint32_t offset) {
  std::uint16_t half = 0;
  std::memcpy(&half, reinterpret_cast<const std::uint8_t *>(record.data()) + offset, sizeof(half));
  return static_cast<std::int16_t>(half);
}

void setHalf(std::array<std::uint32_t, kRecordWords> &record, std::uint32_t offset, std::int16_t value) {
  std::memcpy(reinterpret_cast<std::uint8_t *>(record.data()) + offset, &value, sizeof(value));
}

LeafCall shadedCall(const QuadVertices &vertices, std::uint32_t attributes) {
  LeafCall call;
  call.kind = LeafKind::Shaded;
  call.attributes = attributes;
  call.vertices = vertices.bytes();
  return call;
}

std::int32_t halved(std::int32_t value) {
  return (value + static_cast<std::int32_t>(static_cast<std::uint32_t>(value) >> 31)) >> 1;
}

} // namespace

PanelBody readPanelBody(Core &core) {
  PanelBody body;
  const std::uint32_t record = core.r[4];
  for (std::uint32_t word = 0; word < kRecordWords; ++word) {
    body.record[word] = core.mem_r32(record + word * 4u);
  }
  body.attributes = core.r[5];
  body.address = record;
  return body;
}

BodyResult runPanel(const PanelBody &body, const DrawGlobals &globals, LeafPort &port) {
  BodyResult result;
  const auto &record = body.record;
  if ((record[0] & kDrawn) != 0u) {
    const std::int32_t x = halfOf(record, 4);
    const std::int32_t width = halfOf(record, 10);
    std::int32_t left = x;
    std::int32_t right = width;
    if (isCentred(static_cast<std::int16_t>(x))) {
      const std::uint32_t offset = static_cast<std::uint32_t>(x) & 0x3FFFu;
      left = offset != 0u ? static_cast<std::int32_t>(offset) - halved(width)
                          : kLayoutBase - halved(width) - globals.originX;
      right = width + left;
    }
    QuadVertices vertices;
    vertices.point(0, left, halfOf(record, 6), halfOf(record, 8));
    vertices.point(1, right, halfOf(record, 6), halfOf(record, 8));
    vertices.point(2, left, halfOf(record, 12), halfOf(record, 8));
    vertices.point(3, right, halfOf(record, 12), halfOf(record, 8));
    vertices.colour(record[4]);
    result.take(
        port.draw(shadedCall(vertices, record[0] | body.attributes), static_cast<std::uint32_t>(PanelPart::Body)));
  }
  if ((record[kBorderFlags] & kDrawn) != 0u) {
    // The border is a call of its own, as it is in the guest, and a state of its own.
    const BorderArguments border{body.address + kBorderEdge,
                                 record[kBorderColour],
                                 record[kBorderFlags] | body.attributes,
                                 halfOf(record, 0x18),
                                 halfOf(record, 0x1A)};
    const BodyResult edge = port.border(border);
    result.v0 = edge.v0;
    if (edge.v1) {
      result.v1 = edge.v1;
    }
  } else {
    result.v0 = 0;
  }
  return result;
}

PanelBody blendPanel(const PanelBody &from, const PanelBody &to, float t) {
  PanelBody body = to;
  setHalf(body.record, 4, blendCoordinate(halfOf(from.record, 4), halfOf(to.record, 4), t));
  for (const std::uint32_t offset : {6u, 10u, 12u, 0x18u, 0x1Au}) {
    setHalf(body.record, offset, blendHalf(halfOf(from.record, offset), halfOf(to.record, offset), t));
  }
  return body;
}

void writePanel(psx::present::StateWriter &writer, const PanelBody &body) {
  writer.put(body);
}

PanelBody readPanel(psx::present::StateReader &reader) {
  return reader.get<PanelBody>();
}

BorderBody readBorderBody(Core &core) {
  BorderBody body;
  const std::uint32_t edge = core.r[4];
  for (std::uint32_t at = 0; at < body.edge.size(); ++at) {
    body.edge[at] = static_cast<std::int16_t>(core.mem_r16(edge + at * 2u));
  }
  body.colour = core.r[5];
  body.attributes = core.r[6];
  body.sideWidth = static_cast<std::int32_t>(core.r[7]);
  body.edgeHeight = static_cast<std::int32_t>(core.mem_r32(core.r[29] + 0x10u));
  return body;
}

BodyResult runBorder(const BorderBody &body, const DrawGlobals &globals, LeafPort &port) {
  const auto &[x, top, depth, width, bottom] = body.edge;
  std::int32_t left = x;
  std::int32_t right = width;
  if (isCentred(x)) {
    left = kLayoutBase - halved(width) - globals.originX;
    right = width + left;
  }
  const std::int32_t outerLeft = left - body.sideWidth;
  const std::int32_t outerRight = right + body.sideWidth;
  struct Strip {
    std::int32_t x0, x1, y0, y1;
    PanelPart part;
  };
  const Strip strips[] = {
      {outerLeft, left, top, bottom, PanelPart::Left},
      {right, outerRight, top, bottom, PanelPart::Right},
      {outerLeft, outerRight, top - body.edgeHeight, top, PanelPart::Top},
      {outerLeft, outerRight, bottom, bottom + body.edgeHeight, PanelPart::Bottom},
  };
  BodyResult result;
  for (const Strip &strip : strips) {
    QuadVertices vertices;
    vertices.point(0, strip.x0, strip.y0, depth);
    vertices.point(1, strip.x1, strip.y0, depth);
    vertices.point(2, strip.x0, strip.y1, depth);
    vertices.point(3, strip.x1, strip.y1, depth);
    vertices.colour(body.colour);
    result.take(port.draw(shadedCall(vertices, body.attributes), static_cast<std::uint32_t>(strip.part)));
  }
  return result;
}

BorderBody blendBorder(const BorderBody &from, const BorderBody &to, float t) {
  BorderBody body = to;
  body.edge[0] = blendCoordinate(from.edge[0], to.edge[0], t);
  for (const std::uint32_t at : {1u, 3u, 4u}) {
    body.edge[at] = blendHalf(from.edge[at], to.edge[at], t);
  }
  return body;
}

void writeBorder(psx::present::StateWriter &writer, const BorderBody &body) {
  writer.put(body);
}

BorderBody readBorder(psx::present::StateReader &reader) {
  return reader.get<BorderBody>();
}

} // namespace crashbash::render
