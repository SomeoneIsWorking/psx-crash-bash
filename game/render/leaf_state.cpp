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
// Where a render's packet lives in its host memory.
constexpr std::uint32_t kHostPacket = 0x80000000u;

struct LeafStateHeader {
  std::uint32_t tag = kLeafStateTag;
  std::uint32_t calls = 0;
};

std::array<std::uint32_t, kPacketWords> packetWords(const psx::present::HostMemory &host) {
  std::array<std::uint32_t, kPacketWords> words{};
  std::memcpy(words.data(), host.view(kHostPacket, kPacketWords * kWordBytes).data(), sizeof(words));
  return words;
}

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
    call.position = psx::present::lerpHalves(from.position, to.position, t);
    return call;
  }
  call.globals.originX = psx::present::lerpInt(from.globals.originX, to.globals.originX, t);
  call.globals.originY = psx::present::lerpInt(from.globals.originY, to.globals.originY, t);
  // Corners are an xy word and a z word each; the z word's high half is not a coordinate.
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    const std::uint32_t xy = corner * 2u;
    setVertexWord(call, xy, psx::present::lerpHalves(vertexWord(from, xy), vertexWord(to, xy), t));
    const std::uint32_t z = xy + 1u;
    setVertexWord(call,
                  z,
                  (vertexWord(to, z) & 0xFFFF0000u) |
                      psx::present::lerpHalf(vertexWord(from, z), vertexWord(to, z), 0, t));
  }
  if (to.hasControl != 0u) {
    call.control = psx::present::blendGteControl(from.control, to.control, t);
  }
  return call;
}

struct LeafState {
  std::vector<LeafCall> calls;

  static LeafState read(std::span<const std::byte> bytes) {
    psx::present::StateReader reader(bytes);
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
  psx::present::StateWriter writer;
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
  const psx::present::GteGuard guard;
  // A bucket's walk reaches the packet linked last first.
  for (std::size_t index = later.calls.size(); index-- > 0;) {
    LeafCall call = later.calls[index];
    if (blend && pairs(earlier.calls[index], call)) {
      call = blendCall(earlier.calls[index], call, t);
    }
    if (call.hasControl != 0u) {
      psx::present::writeGteControl(call.control);
    }
    psx::present::HostMemory host;
    host.zero(kHostPacket, kPacketWords * kWordBytes);
    const LeafResult result = buildLeaf(psx::present::EmitMemory(core, host), kHostPacket, call);
    if (!result.bucket) {
      continue;
    }
    if (const auto primitive = decodeLinkedPacket(packetWords(host))) {
      sink.emit(psx::present::OtSlot{static_cast<std::uint16_t>(call.table), call.baseBucket + *result.bucket},
                *primitive);
    }
  }
}

} // namespace crashbash::render
