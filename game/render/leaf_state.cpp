#include "leaf_state.h"

#include "blend.h"
#include "core.h"
#include "frame_state.h"
#include "ordering_table_slots.h"
#include "packet_decode.h"
#include "state_bytes.h"

#include <array>
#include <cstring>
#include <type_traits>

namespace crashbash::render {
namespace {

static_assert(std::is_trivially_copyable_v<LeafCall>);

constexpr std::uint32_t kPacketWords = 16u;
constexpr std::uint32_t kWordBytes = 4u;

struct LeafStateHeader {
  std::uint32_t tag = kLeafStateTag;
  std::uint32_t calls = 0;
};

// A packet in host words.
class HostPacket final : public PacketWriter {
public:
  void w8(std::uint32_t offset, std::uint32_t value) override {
    bytes()[offset] = static_cast<std::uint8_t>(value);
  }
  void w16(std::uint32_t offset, std::uint32_t value) override {
    const auto half = static_cast<std::uint16_t>(value);
    std::memcpy(bytes() + offset, &half, sizeof(half));
  }
  void w32(std::uint32_t offset, std::uint32_t value) override {
    std::memcpy(bytes() + offset, &value, sizeof(value));
  }
  void storeXy(std::uint32_t offset, std::uint32_t reg) override {
    w32(offset, gte_read_data(static_cast<int>(reg)));
  }
  std::uint32_t r32(std::uint32_t offset) override {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes() + offset, sizeof(value));
    return value;
  }
  const std::array<std::uint32_t, kPacketWords> &words() const {
    return words_;
  }

private:
  std::uint8_t *bytes() {
    return reinterpret_cast<std::uint8_t *>(words_.data());
  }
  std::array<std::uint32_t, kPacketWords> words_{};
};

std::uint32_t vertexWord(const LeafCall &call, std::uint32_t index) {
  std::uint32_t word = 0;
  std::memcpy(&word, call.vertices.data() + index * kWordBytes, sizeof(word));
  return word;
}

void setVertexWord(LeafCall &call, std::uint32_t index, std::uint32_t word) {
  std::memcpy(call.vertices.data() + index * kWordBytes, &word, sizeof(word));
}

bool pairs(const LeafCall &from, const LeafCall &to) {
  if (from.kind != to.kind) {
    return false;
  }
  if (to.kind == LeafKind::Shaded) {
    return from.attributes == to.attributes && from.hasControl == to.hasControl;
  }
  return from.textureAddress == to.textureAddress;
}

// `to` with the inputs that move `t` of the way from `from`'s.
LeafCall blendCall(const LeafCall &from, const LeafCall &to, float t) {
  LeafCall call = to;
  if (to.kind != LeafKind::Shaded) {
    call.position = lerpPoint(from.position, to.position, t);
    return call;
  }
  call.globals.originX = lerpInt(from.globals.originX, to.globals.originX, t);
  call.globals.originY = lerpInt(from.globals.originY, to.globals.originY, t);
  // Corners are an xy word and a z word each; the z word's high half is not a coordinate.
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    const std::uint32_t xy = corner * 2u;
    setVertexWord(call, xy, lerpPoint(vertexWord(from, xy), vertexWord(to, xy), t));
    const std::uint32_t z = xy + 1u;
    setVertexWord(call, z, (vertexWord(to, z) & 0xFFFF0000u) | lerpHalf(vertexWord(from, z), vertexWord(to, z), 0, t));
  }
  if (to.hasControl != 0u) {
    call.control = gte::blendControl(from.control, to.control, t);
  }
  return call;
}

struct LeafState {
  std::vector<LeafCall> calls;

  static LeafState read(std::span<const std::byte> bytes) {
    StateReader reader(bytes);
    const auto header = reader.get<LeafStateHeader>();
    LeafState state;
    state.calls = reader.getAll<LeafCall>(header.calls);
    return state;
  }
};

} // namespace

void saveLeafState(Core &core, const psx::present::RecordKey &owner, std::span<const LeafNote> notes) {
  std::vector<LeafCall> calls;
  for (const LeafNote &note : notes) {
    const auto slot = linkedSlot(core, note.packet);
    if (slot && slot->table == note.call.table) {
      calls.push_back(note.call);
    }
  }
  if (calls.empty()) {
    return;
  }
  StateWriter writer;
  writer.put(LeafStateHeader{kLeafStateTag, static_cast<std::uint32_t>(calls.size())});
  writer.putAll(std::span<const LeafCall>(calls));
  core.frameStates.save(owner, writer.bytes());
}

void renderLeafState(Core &core,
                     std::span<const std::byte> from,
                     std::span<const std::byte> to,
                     float t,
                     psx::present::PrimitiveSink &sink) {
  const LeafState later = LeafState::read(to);
  LeafState earlier;
  if (t < 1.0f && from.data() != to.data()) {
    earlier = LeafState::read(from);
  }
  const bool blend = earlier.calls.size() == later.calls.size();
  const gte::Guard guard;
  // A bucket's walk reaches the packet linked last first.
  for (std::size_t index = later.calls.size(); index-- > 0;) {
    LeafCall call = later.calls[index];
    if (blend && pairs(earlier.calls[index], call)) {
      call = blendCall(earlier.calls[index], call, t);
    }
    if (call.hasControl != 0u) {
      gte::writeControl(call.control);
    }
    HostPacket packet;
    const LeafResult result = buildLeaf(core, packet, call);
    if (!result.bucket) {
      continue;
    }
    if (const auto primitive = decodeLinkedPacket(packet.words())) {
      sink.emit(psx::present::OtSlot{static_cast<std::uint16_t>(call.table), call.baseBucket + *result.bucket},
                *primitive);
    }
  }
}

} // namespace crashbash::render
