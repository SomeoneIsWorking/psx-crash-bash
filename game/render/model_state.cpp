#include "render/model_state.h"

#include "blend.h"
#include "core.h"
#include "frame_state.h"
#include "render/model_face_producer.h"
#include "render/ordering_table_slots.h"
#include "render/packet_decode.h"
#include "runtime/session.h"

#include <array>

namespace crashbash::render {
namespace {

constexpr std::uint32_t kWordBytes = 4u;

// Takes the faces a render of the saved state links: the saved packet with the points the GTE projected.
class HostFaceSink final : public FaceSink {
public:
  HostFaceSink(Core &core, const std::vector<SavedFace> &saved, FaceScratch &scratch)
      : core_(core), saved_(saved), byFace_(scratch.byFace), linked_(scratch.linked) {
    for (std::size_t index = 0; index < saved.size(); ++index) {
      if (saved[index].face >= byFace_.size()) {
        byFace_.resize(saved[index].face + 1u, kNone);
      }
      byFace_[saved[index].face] = index;
    }
  }

  void emit(const FaceEmit &face) override {
    if (face.face >= byFace_.size() || byFace_[face.face] == kNone) {
      return;
    }
    LinkedFace linked{face.bucket,
                      psx::gpu::PacketBuffer<kFacePacketWords>(std::span(saved_[byFace_[face.face]].words))};
    const auto points = facePointWords(face.layout);
    for (std::uint32_t point = 0; point < 3u; ++point) {
      linked.words.storeVertex(points[point], core_.gte().projected(point));
    }
    linked_.push_back(linked);
  }

private:
  static constexpr std::size_t kNone = ~std::size_t{0};
  Core &core_;
  const std::vector<SavedFace> &saved_;
  std::vector<std::size_t> &byFace_;
  std::vector<LinkedFace> &linked_;
};

// One s16 of a vertex word moved by `t`.
std::uint32_t blendVertexWord(std::uint32_t from, std::uint32_t to, bool depth, float t) {
  if (depth) {
    return (to & 0xFFFF0000u) | psx::present::lerpHalf(from, to, psx::present::Halfword::Low, t);
  }
  return psx::present::lerpHalves(from, to, t);
}

bool sameDraw(const FaceState &a, const FaceState &b) {
  return a.list == b.list && a.vertices.size() == b.vertices.size();
}

} // namespace

void FaceState::read(std::span<const std::byte> bytes) {
  psx::present::StateReader reader(bytes);
  reader.get<std::uint32_t>();
  header = reader.get<FaceStateHeader>();
  reader.getAll(header.listBytes, list);
  reader.getAll(header.vertexWords, vertices);
  reader.getAll(header.faces, faces);
}

void readFaceCall(Core &core, FaceCall &call) {
  call.named = false;
  call.table = 0;
  call.baseBucket = 0;
  call.linked.clear();
  call.packets = core.r[4];
  call.globals = readDrawGlobals(core);
  call.control = psx::present::readGteControl(core.gte());
  if (const auto slot = core.otTables.slotOf(call.globals.otBase)) {
    call.named = true;
    call.table = slot->table;
    call.baseBucket = slot->index;
  }
  readFaceList(core, core.r[6], call.list);
  const std::uint32_t words = faceVertexWords(call.list);
  call.vertices.resize(words);
  for (std::uint32_t word = 0; word < words; ++word) {
    call.vertices[word] = core.mem_r32(core.r[5] + word * kWordBytes);
  }
}

bool saveFaceState(Core &core, const psx::present::RecordKey &owner, const FaceCall &call) {
  if (!call.named) {
    return false;
  }
  const psx::present::Kept<FaceScratch>::Use scratch(crashbash::session(core).faceScratch);
  std::vector<SavedFace> &faces = scratch->saved;
  for (const FaceEmit &face : call.linked) {
    const std::uint32_t packet = call.packets + face.face * kFacePacketBytes;
    const auto slot = linkedSlot(core, packet);
    if (!slot || slot->table != call.table || slot->index != call.baseBucket + face.bucket) {
      continue;
    }
    SavedFace saved;
    saved.face = face.face;
    saved.layout = static_cast<std::uint32_t>(face.layout);
    saved.bucket = face.bucket;
    for (std::uint32_t word = 0; word < kFacePacketWords; ++word) {
      saved.words[word] = core.mem_r32(packet + word * kWordBytes);
    }
    if (!decodeLinkedPacket({.words = saved.words, .places = {}})) {
      return false;
    }
    faces.push_back(saved);
  }
  if (faces.empty()) {
    return true;
  }
  FaceStateHeader header;
  header.control = call.control;
  header.zBias = call.globals.zBias;
  header.zLimit = call.globals.zLimit;
  header.table = call.table;
  header.baseBucket = call.baseBucket;
  header.listBytes = static_cast<std::uint32_t>(call.list.size());
  header.vertexWords = static_cast<std::uint32_t>(call.vertices.size());
  header.faces = static_cast<std::uint32_t>(faces.size());
  const std::array<std::span<const std::byte>, 5> parts = {
      std::as_bytes(std::span(&kFaceStateTag, 1)),
      std::as_bytes(std::span(&header, 1)),
      std::as_bytes(std::span(call.list)),
      std::as_bytes(std::span(call.vertices)),
      std::as_bytes(std::span(faces)),
  };
  core.frameStates.save(owner, parts);
  return true;
}

void renderFaceState(Core &core,
                     std::span<const std::byte> from,
                     std::span<const std::byte> to,
                     float t,
                     psx::present::PrimitiveSink &sink) {
  const psx::present::Kept<FaceScratch>::Use scratch(crashbash::session(core).faceScratch);
  FaceState &state = scratch->to;
  state.read(to);
  psx::present::GteControl control = state.header.control;
  std::vector<std::uint32_t> &vertices = scratch->vertices;
  vertices.assign(state.vertices.begin(), state.vertices.end());
  if (t < 1.0f && from.data() != to.data()) {
    FaceState &earlier = scratch->from;
    earlier.read(from);
    if (sameDraw(earlier, state)) {
      control = psx::present::blendGteControl(earlier.header.control, state.header.control, t);
      for (std::size_t word = 0; word < vertices.size(); ++word) {
        vertices[word] = blendVertexWord(earlier.vertices[word], state.vertices[word], (word & 1u) != 0u, t);
      }
    }
  }
  HostFaceSink host(core, state.faces, *scratch);
  {
    const psx::present::GteGuard guard(core.gte());
    psx::present::writeGteControl(core.gte(), control);
    projectFaces(core, FaceInputs{state.list, vertices, state.header.zBias, state.header.zLimit}, host);
  }
  // A bucket's walk reaches the packet linked last first.
  for (auto emitted = scratch->linked.rbegin(); emitted != scratch->linked.rend(); ++emitted) {
    const auto primitive = decodeLinkedPacket(emitted->words.view());
    if (primitive) {
      sink.emit(psx::present::OtSlot{static_cast<std::uint16_t>(state.header.table),
                                     state.header.baseBucket + emitted->bucket},
                *primitive);
    }
  }
}

} // namespace crashbash::render
