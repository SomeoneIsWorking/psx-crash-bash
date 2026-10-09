// game/render/leaf_port.h — where a component body's 2D leaf draws go: the guest's pool, or a render's packets.
//
// The string, digit, panel and border bodies (text_bodies.h, panel_bodies.h) fill in a leaf call's arguments and
// hand it to a port. Over the guest the port completes the call from guest memory, runs the leaf body and
// links its packet; in a render (body_render.h) it completes the call from the state it was saved with.
#pragma once

#include "leaf_packets.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class Core;

namespace crashbash::render {

// What a body leaves in the result registers: v0 always, v1 only when a leaf set it (else the caller's stays).
struct BodyResult {
  std::uint32_t v0 = 0;
  std::optional<std::uint32_t> v1;

  // The result registers a leaf call leaves.
  void take(const LeafExecution &done) {
    v0 = done.value;
    if (done.setsSecond) {
      v1 = done.second;
    }
  }
};

// The arguments of the border draw FUN_8001A43C: the rectangle it surrounds is at `edge`, five halfwords.
struct BorderArguments {
  std::uint32_t edge = 0;
  std::uint32_t colour = 0;
  std::uint32_t attributes = 0;
  std::int32_t sideWidth = 0;
  std::int32_t edgeHeight = 0;
};

class LeafPort {
public:
  virtual ~LeafPort() = default;
  // Draws one leaf for a call whose arguments are set. `element` names the part of the object it draws.
  virtual LeafExecution draw(LeafCall call, std::uint32_t element) = 0;
  // A panel's border, which is a draw of its own: over the guest the call the panel makes, in a render nothing,
  // as the border is a state of its own.
  virtual BodyResult border(const BorderArguments &arguments) = 0;
};

// The leaves over the guest: each is the call the guest body makes, to the override at the leaf's address, with
// the arguments the body gives it. `owner` is the address of the body running; it is the return address the
// leaves see, so no call site names them.
class GuestLeafPort final : public LeafPort {
public:
  GuestLeafPort(Core &core, std::uint32_t owner) : core_(core), owner_(owner) {}

  LeafExecution draw(LeafCall call, std::uint32_t element) override;
  BodyResult border(const BorderArguments &arguments) override;

private:
  Core &core_;
  std::uint32_t owner_;
};

} // namespace crashbash::render
