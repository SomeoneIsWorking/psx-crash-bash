// game/render/host_leaf.h — the 2D leaves a render draws: the leaf body over host words, its packet as a primitive.
//
// `emitLeaf` runs `buildLeaf` on one call into host memory and decodes the packet it linked. A component body's
// leaves share what their calls read besides their arguments (`BodyContext`); `HostLeafPort` completes each
// call from it, so the same body that drew over the guest draws here with its inputs moved.
#pragma once

#include "frame_record.h"
#include "leaf_packets.h"
#include "leaf_port.h"
#include "state_bytes.h"

#include <cstdint>
#include <span>
#include <vector>

class Core;

namespace crashbash::render {

// One primitive a render produces and the slot it lands in.
struct Emission {
  psx::present::OtSlot slot;
  psx::present::DrawPrimitive primitive;
};

// Builds the leaf into host memory and appends the primitive it links, if it links one.
void emitLeaf(Core &core, const LeafCall &call, std::vector<Emission> &out);

struct TextureEntry {
  std::uint32_t address = 0;
  TextureRecord texture;
};

// What the leaves of one component body read besides their arguments.
struct BodyContext {
  DrawGlobals globals;
  std::uint32_t table = 0;
  std::uint32_t baseBucket = 0;
  std::uint32_t hasControl = 0;
  psx::present::GteControl control{};
  std::vector<TextureEntry> textures;
};

// The context the leaves shared; `leaves` must not be empty.
BodyContext contextOf(std::span<const LeafNote> leaves);
// `to`'s context with the origin and the GTE transform `t` of the way from `from`'s.
BodyContext blendContext(const BodyContext &from, const BodyContext &to, float t);
void writeContext(psx::present::StateWriter &writer, const BodyContext &context);
BodyContext readContext(psx::present::StateReader &reader);

class HostLeafPort final : public LeafPort {
public:
  HostLeafPort(Core &core, const BodyContext &context, std::vector<Emission> &out)
      : core_(core), context_(context), out_(out) {}

  LeafExecution draw(LeafCall call, std::uint32_t element) override;
  BodyResult border(const BorderArguments &) override {
    return {};
  }

private:
  Core &core_;
  const BodyContext &context_;
  std::vector<Emission> &out_;
};

} // namespace crashbash::render
