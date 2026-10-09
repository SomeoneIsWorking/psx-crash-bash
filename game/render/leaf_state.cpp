#include "leaf_state.h"

#include "blend.h"
#include "core.h"
#include "frame_state.h"
#include "host_leaf.h"
#include "ordering_table_slots.h"
#include "state_bytes.h"

#include <algorithm>
#include <cstring>
#include <type_traits>

namespace crashbash::render {
namespace {

static_assert(std::is_trivially_copyable_v<LeafCall>);

constexpr std::uint32_t kWordBytes = 4u;

struct UiStateHeader {
  std::uint32_t tag = kLeafStateTag;
  std::uint32_t items = 0;
};

std::uint32_t vertexWord(const LeafCall &call, std::uint32_t index) {
  std::uint32_t word = 0;
  std::memcpy(&word, call.vertices.data() + index * kWordBytes, sizeof(word));
  return word;
}

void setVertexWord(LeafCall &call, std::uint32_t index, std::uint32_t word) {
  std::memcpy(call.vertices.data() + index * kWordBytes, &word, sizeof(word));
}

bool pairsLeaf(const LeafCall &from, const LeafCall &to) {
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

enum class ItemKind : std::uint32_t { Leaf, Body };

struct UiItem {
  ItemKind kind = ItemKind::Leaf;
  LeafCall leaf;
  ComponentBody body;
  BodyContext context;
};

struct UiState {
  std::vector<UiItem> items;

  static UiState read(std::span<const std::byte> bytes) {
    psx::present::StateReader reader(bytes);
    const auto header = reader.get<UiStateHeader>();
    UiState state;
    for (std::uint32_t at = 0; at < header.items; ++at) {
      UiItem item;
      item.kind = static_cast<ItemKind>(reader.get<std::uint32_t>());
      if (item.kind == ItemKind::Leaf) {
        item.leaf = reader.get<LeafCall>();
      } else {
        item.body = readBody(reader);
        item.context = readContext(reader);
      }
      state.items.push_back(std::move(item));
    }
    return state;
  }
};

bool linked(Core &core, const LeafNote &note) {
  const auto slot = linkedSlot(core, note.packet);
  return slot && slot->table == note.call.table;
}

} // namespace

void saveLeafState(Core &core, const psx::present::RecordKey &owner, std::span<const UiNote> notes) {
  psx::present::StateWriter writer;
  std::uint32_t items = 0;
  for (const UiNote &note : notes) {
    const bool kept =
        !note.leaves.empty() && std::all_of(note.leaves.begin(), note.leaves.end(), [&](const LeafNote &leaf) {
          return linked(core, leaf);
        });
    if (!kept) {
      continue;
    }
    if (note.body) {
      writer.put(static_cast<std::uint32_t>(ItemKind::Body));
      writeBody(writer, *note.body);
      writeContext(writer, contextOf(note.leaves));
    } else {
      writer.put(static_cast<std::uint32_t>(ItemKind::Leaf));
      writer.put(note.leaves.front().call);
    }
    ++items;
  }
  if (items == 0u) {
    return;
  }
  psx::present::StateWriter state;
  state.put(UiStateHeader{kLeafStateTag, items});
  state.putAll(writer.bytes());
  core.frameStates.save(owner, state.bytes());
}

void renderLeafState(Core &core,
                     std::span<const std::byte> from,
                     std::span<const std::byte> to,
                     float t,
                     psx::present::PrimitiveSink &sink) {
  const UiState later = UiState::read(to);
  UiState earlier;
  if (t < 1.0f && from.data() != to.data()) {
    earlier = UiState::read(from);
  }
  const bool blend = earlier.items.size() == later.items.size();
  const psx::present::GteGuard guard;
  std::vector<Emission> drawn;
  for (std::size_t index = 0; index < later.items.size(); ++index) {
    const UiItem &item = later.items[index];
    const UiItem *before = blend ? &earlier.items[index] : nullptr;
    if (item.kind == ItemKind::Leaf) {
      LeafCall call = item.leaf;
      if (before != nullptr && before->kind == ItemKind::Leaf && pairsLeaf(before->leaf, call)) {
        call = blendCall(before->leaf, call, t);
      }
      emitLeaf(core, call, drawn);
      continue;
    }
    ComponentBody body = item.body;
    BodyContext context = item.context;
    if (before != nullptr && before->kind == ItemKind::Body && sameKind(before->body, body)) {
      body = blendBody(before->body, body, t);
      context = blendContext(before->context, context, t);
    }
    HostLeafPort port(core, context, drawn);
    runBody(body, context.globals, port);
  }
  // A bucket's walk reaches the packet linked last first.
  for (auto emission = drawn.rbegin(); emission != drawn.rend(); ++emission) {
    sink.emit(emission->slot, emission->primitive);
  }
}

} // namespace crashbash::render
