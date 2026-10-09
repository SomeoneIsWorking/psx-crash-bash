#include "face_projection.h"

#include "core.h"
#include "gte_control.h"
#include "model_face_producer.h"

namespace crashbash::render {
namespace {

constexpr std::uint32_t kWordBytes = 4u;
constexpr std::uint32_t kLayoutBit = 1u;
constexpr std::uint32_t kBackFaceBit = 0x10000u;
constexpr std::uint32_t kTwoSidedBit = 0x20000u;
constexpr std::uint32_t kLinkMask = 0xFFFFFFu;
constexpr std::uint32_t kTextureLength = 9u;
constexpr std::uint32_t kLengthShift = 24u;

constexpr std::array<std::uint32_t, 3> kTexturedPoints = {2u, 5u, 8u};
constexpr std::array<std::uint32_t, 3> kShadedPoints = {4u, 6u, 8u};

std::uint32_t wordAt(std::span<const std::uint32_t> words, std::size_t index) {
  return index < words.size() ? words[index] : 0u;
}

// Projects vertex `index`: the point and its depth enter the FIFOs.
void project(Core &core, std::span<const std::uint32_t> vertices, std::size_t index) {
  gte_write_data(psx::gte::kVxy0, wordAt(vertices, index * 2u));
  gte_write_data(psx::gte::kVz0, wordAt(vertices, index * 2u + 1u));
  gte_op(&core, psx::gte::kRtps);
}

} // namespace

std::array<std::uint32_t, 3> facePointWords(FaceLayout layout) {
  return layout == FaceLayout::Textured ? kTexturedPoints : kShadedPoints;
}

std::vector<std::uint8_t> readFaceList(Core &core, std::uint32_t address) {
  std::vector<std::uint8_t> list;
  for (std::uint32_t strip = address;; strip += 2u) {
    list.push_back(core.mem_r8(strip));
    list.push_back(core.mem_r8(strip + 1u));
    if (list.back() == kFaceListEnd) {
      return list;
    }
  }
}

std::uint32_t faceVertexWords(std::span<const std::uint8_t> list) {
  std::uint32_t words = 0;
  for (std::size_t strip = 0; strip * 2u + 1u < list.size() && list[strip * 2u + 1u] != kFaceListEnd; ++strip) {
    words += 4u + 2u * list[strip * 2u + 1u];
  }
  return words;
}

std::uint32_t faceCount(std::span<const std::uint8_t> list) {
  std::uint32_t faces = 0;
  for (std::size_t strip = 0; strip * 2u + 1u < list.size() && list[strip * 2u + 1u] != kFaceListEnd; ++strip) {
    faces += list[strip * 2u + 1u];
  }
  return faces;
}

void projectFaces(Core &core, const FaceInputs &inputs, FaceSink &sink) {
  std::uint32_t first = 0; // the strip's first vertex
  std::uint32_t face = 0;
  for (std::size_t strip = 0; strip * 2u + 1u < inputs.list.size() && inputs.list[strip * 2u + 1u] != kFaceListEnd;
       ++strip) {
    const std::uint32_t flags = inputs.list[strip * 2u];
    const std::uint32_t count = inputs.list[strip * 2u + 1u];
    const FaceLayout layout = (flags & kLayoutBit) == 0u ? FaceLayout::Textured : FaceLayout::Shaded;
    project(core, inputs.vertices, first);
    project(core, inputs.vertices, first + 1u);
    for (std::uint32_t k = 0; k < count; ++k, ++face) {
      const std::uint32_t corner = first + 2u + k;
      project(core, inputs.vertices, corner);
      const std::uint32_t vertexFlags = wordAt(inputs.vertices, corner * 2u + 1u);
      gte_op(&core, psx::gte::kAvsz3);
      const auto depth = static_cast<std::int32_t>(gte_read_data(psx::gte::kOtz));
      if (layout == FaceLayout::Shaded && depth == 0) {
        continue;
      }
      const std::uint32_t bucket = static_cast<std::uint32_t>(depth + inputs.zBias) >> 1;
      if (bucket >= static_cast<std::uint32_t>(inputs.zLimit)) {
        continue;
      }
      bool visible = true;
      if ((vertexFlags & kTwoSidedBit) == 0u) {
        gte_op(&core, psx::gte::kNclip);
        const auto winding = static_cast<std::int32_t>(gte_read_data(psx::gte::kMac0));
        visible = (vertexFlags & kBackFaceBit) == 0u ? winding > 0 : winding < 1;
      }
      if (visible) {
        sink.emit({face, layout, bucket});
      }
    }
    first += count + 2u;
  }
}

void GuestFaceSink::emit(const FaceEmit &face) {
  const std::uint32_t packet = packets_ + face.face * kFacePacketBytes;
  const std::uint32_t bucket = otBase_ + face.bucket * kWordBytes;
  const std::uint32_t head = core_.mem_r32(bucket);
  if (face.layout == FaceLayout::Textured) {
    core_.mem_w32(packet, (head & kLinkMask) | (kTextureLength << kLengthShift));
  } else {
    core_.mem_w32(packet, (core_.mem_r32(packet) & ~kLinkMask) | (head & kLinkMask));
  }
  core_.mem_w32(bucket, (core_.mem_r32(bucket) & ~kLinkMask) | (packet & kLinkMask));
  const auto points = facePointWords(face.layout);
  for (std::uint32_t point = 0; point < 3u; ++point) {
    gte_store_xy(&core_, packet + points[point] * kWordBytes, static_cast<int>(psx::gte::kSxy0 + point));
  }
  linked_.push_back(face);
}

} // namespace crashbash::render
