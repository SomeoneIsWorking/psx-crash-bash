#include "model_state.h"

#include "blend.h"
#include "core.h"
#include "frame_state.h"
#include "model_face_producer.h"
#include "ordering_table_slots.h"
#include "packet_decode.h"

#include <algorithm>
#include <optional>

namespace crashbash::render {
namespace {

constexpr std::uint32_t kWordBytes = 4u;

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

  static FaceState read(std::span<const std::byte> bytes) {
    psx::present::StateReader reader(bytes);
    reader.get<std::uint32_t>();
    FaceState state;
    state.header = reader.get<FaceStateHeader>();
    state.list = reader.getAll<std::uint8_t>(state.header.listBytes);
    state.vertices = reader.getAll<std::uint32_t>(state.header.vertexWords);
    state.faces = reader.getAll<SavedFace>(state.header.faces);
    return state;
  }
};

// Takes the faces a render of the saved state links: the saved packet with the points the GTE projected.
class HostFaceSink final : public FaceSink {
public:
  explicit HostFaceSink(const std::vector<SavedFace> &saved) : saved_(saved), byFace_() {
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
    Emitted emitted{face.bucket, saved_[byFace_[face.face]].words};
    const auto points = facePointWords(face.layout);
    for (std::uint32_t point = 0; point < 3u; ++point) {
      emitted.words[points[point]] = gte_read_data(psx::gte::kSxy0 + point);
    }
    linked.push_back(emitted);
  }

  struct Emitted {
    std::uint32_t bucket = 0;
    FaceWords words{};
  };
  std::vector<Emitted> linked;

private:
  static constexpr std::size_t kNone = ~std::size_t{0};
  const std::vector<SavedFace> &saved_;
  std::vector<std::size_t> byFace_;
};

// One s16 of a vertex word moved by `t`.
std::uint32_t blendVertexWord(std::uint32_t from, std::uint32_t to, bool depth, float t) {
  if (depth) {
    return (to & 0xFFFF0000u) | psx::present::lerpHalf(from, to, 0, t);
  }
  return psx::present::lerpHalves(from, to, t);
}

bool sameDraw(const FaceState &a, const FaceState &b) {
  return a.list == b.list && a.vertices.size() == b.vertices.size();
}

} // namespace

FaceCall readFaceCall(Core &core) {
  FaceCall call;
  call.packets = core.r[4];
  call.globals = readDrawGlobals(core);
  call.control = psx::present::readGteControl();
  if (const auto slot = core.otTables.slotOf(call.globals.otBase)) {
    call.named = true;
    call.table = slot->table;
    call.baseBucket = slot->index;
  }
  call.list = readFaceList(core, core.r[6]);
  const std::uint32_t words = faceVertexWords(call.list);
  call.vertices.resize(words);
  for (std::uint32_t word = 0; word < words; ++word) {
    call.vertices[word] = core.mem_r32(core.r[5] + word * kWordBytes);
  }
  return call;
}

bool saveFaceState(Core &core, const psx::present::RecordKey &owner, const FaceCall &call) {
  if (!call.named) {
    return false;
  }
  std::vector<SavedFace> faces;
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
    if (!decodeLinkedPacket(saved.words)) {
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
  psx::present::StateWriter writer;
  writer.put(kFaceStateTag);
  writer.put(header);
  writer.putAll(std::span<const std::uint8_t>(call.list));
  writer.putAll(std::span<const std::uint32_t>(call.vertices));
  writer.putAll(std::span<const SavedFace>(faces));
  core.frameStates.save(owner, writer.bytes());
  return true;
}

void renderFaceState(Core &core,
                     std::span<const std::byte> from,
                     std::span<const std::byte> to,
                     float t,
                     psx::present::PrimitiveSink &sink) {
  const FaceState state = FaceState::read(to);
  psx::present::GteControl control = state.header.control;
  std::vector<std::uint32_t> vertices = state.vertices;
  if (t < 1.0f && from.data() != to.data()) {
    const FaceState earlier = FaceState::read(from);
    if (sameDraw(earlier, state)) {
      control = psx::present::blendGteControl(earlier.header.control, state.header.control, t);
      for (std::size_t word = 0; word < vertices.size(); ++word) {
        vertices[word] = blendVertexWord(earlier.vertices[word], state.vertices[word], (word & 1u) != 0u, t);
      }
    }
  }
  HostFaceSink host(state.faces);
  {
    const psx::present::GteGuard guard;
    psx::present::writeGteControl(control);
    projectFaces(core, FaceInputs{state.list, vertices, state.header.zBias, state.header.zLimit}, host);
  }
  // A bucket's walk reaches the packet linked last first.
  for (auto emitted = host.linked.rbegin(); emitted != host.linked.rend(); ++emitted) {
    const auto primitive = decodeLinkedPacket(emitted->words);
    if (primitive) {
      sink.emit(psx::present::OtSlot{static_cast<std::uint16_t>(state.header.table),
                                     state.header.baseBucket + emitted->bucket},
                *primitive);
    }
  }
}

} // namespace crashbash::render
