#include "host_leaf.h"

#include "blend.h"
#include "core.h"
#include "emit_memory.h"
#include "gte_control.h"
#include "packet_decode.h"
#include <lucent/log.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace crashbash::render {
namespace {

static_assert(std::is_trivially_copyable_v<TextureEntry>);

constexpr std::uint32_t kPacketWords = 16u;
constexpr std::uint32_t kWordBytes = 4u;
// Where a render's packet lives in its host memory.
constexpr std::uint32_t kHostPacket = 0x80000000u;

struct ContextHeader {
  DrawGlobals globals;
  std::uint32_t table = 0;
  std::uint32_t baseBucket = 0;
  std::uint32_t hasControl = 0;
  psx::present::GteControl control{};
  std::uint32_t textures = 0;
};

std::array<std::uint32_t, kPacketWords> packetWords(const psx::present::HostMemory &host) {
  std::array<std::uint32_t, kPacketWords> words{};
  std::memcpy(words.data(), host.view(kHostPacket, kPacketWords * kWordBytes).data(), sizeof(words));
  return words;
}

} // namespace

void emitLeaf(Core &core, const LeafCall &call, std::vector<Emission> &out) {
  if (call.hasControl != 0u) {
    psx::present::writeGteControl(call.control);
  }
  psx::present::HostMemory host;
  host.zero(kHostPacket, kPacketWords * kWordBytes);
  const LeafResult result = buildLeaf(psx::present::EmitMemory(core, host), kHostPacket, call);
  if (!result.bucket) {
    return;
  }
  if (const auto primitive = decodeLinkedPacket(packetWords(host))) {
    out.push_back(
        {psx::present::OtSlot{static_cast<std::uint16_t>(call.table), call.baseBucket + *result.bucket}, *primitive});
  }
}

BodyContext contextOf(std::span<const LeafNote> leaves) {
  const LeafCall &first = leaves.front().call;
  BodyContext context;
  context.globals = first.globals;
  context.table = first.table;
  context.baseBucket = first.baseBucket;
  context.hasControl = first.hasControl;
  context.control = first.control;
  for (const LeafNote &note : leaves) {
    if (note.call.kind == LeafKind::Shaded) {
      continue;
    }
    const auto known = std::find_if(context.textures.begin(), context.textures.end(), [&](const TextureEntry &entry) {
      return entry.address == note.call.textureAddress;
    });
    if (known == context.textures.end()) {
      context.textures.push_back({note.call.textureAddress, note.call.texture});
    }
  }
  return context;
}

BodyContext blendContext(const BodyContext &from, const BodyContext &to, float t) {
  BodyContext context = to;
  context.globals.originX = psx::present::lerpInt(from.globals.originX, to.globals.originX, t);
  context.globals.originY = psx::present::lerpInt(from.globals.originY, to.globals.originY, t);
  if (from.hasControl != 0u && to.hasControl != 0u) {
    context.control = psx::present::blendGteControl(from.control, to.control, t);
  }
  return context;
}

void writeContext(psx::present::StateWriter &writer, const BodyContext &context) {
  writer.put(ContextHeader{context.globals,
                           context.table,
                           context.baseBucket,
                           context.hasControl,
                           context.control,
                           static_cast<std::uint32_t>(context.textures.size())});
  writer.putAll(std::span<const TextureEntry>(context.textures));
}

BodyContext readContext(psx::present::StateReader &reader) {
  const auto header = reader.get<ContextHeader>();
  BodyContext context;
  context.globals = header.globals;
  context.table = header.table;
  context.baseBucket = header.baseBucket;
  context.hasControl = header.hasControl;
  context.control = header.control;
  context.textures = reader.getAll<TextureEntry>(header.textures);
  return context;
}

LeafExecution HostLeafPort::draw(LeafCall call, std::uint32_t) {
  call.globals = context_.globals;
  call.table = context_.table;
  call.baseBucket = context_.baseBucket;
  if (call.kind == LeafKind::Shaded) {
    if ((call.attributes & kDrawn) != 0u && (call.attributes & kFlatLayout) == 0u) {
      call.hasControl = context_.hasControl;
      call.control = context_.control;
    }
  } else if (call.globals.hasEnvironment != 0u) {
    const auto found = std::find_if(context_.textures.begin(), context_.textures.end(), [&](const TextureEntry &entry) {
      return entry.address == call.textureAddress;
    });
    if (found == context_.textures.end()) {
      lucent::error(
          "host-leaf", "a body drew texture 0x{:08X}, which its saved state does not hold", call.textureAddress);
      std::abort();
    }
    call.texture = found->texture;
  }
  emitLeaf(core_, call, out_);
  return {};
}

} // namespace crashbash::render
