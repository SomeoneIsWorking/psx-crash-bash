#include "state_renders.h"

#include "core.h"
#include "crashbash_guest.h"
#include "leaf_state.h"
#include "model_state.h"
#include "state_bytes.h"
#include "state_producer.h"

#include <memory>

namespace crashbash::render {
namespace {

std::uint32_t tagOf(std::span<const std::byte> state) {
  return StateReader(state).get<std::uint32_t>();
}

// Picks the render by the state's tag; a producer saves a mesh state or a leaf state per call.
class StateRender final : public psx::present::StateProducer {
public:
  explicit StateRender(Core &core) : core_(core) {}

  void render(std::span<const std::byte> from,
              std::span<const std::byte> to,
              float t,
              psx::present::PrimitiveSink &sink) const override {
    // A state of another kind is a different draw: nothing to move from.
    const auto earlier = tagOf(from) == tagOf(to) ? from : to;
    if (tagOf(to) == kFaceStateTag) {
      renderFaceState(core_, earlier, to, t, sink);
    } else {
      renderLeafState(core_, earlier, to, t, sink);
    }
  }

private:
  Core &core_;
};

} // namespace

void registerStateRenders(Core &core) {
  for (const std::uint32_t producer :
       {guest::kModelDraw, guest::kTextComponentDraw, guest::kPanelComponentDraw, guest::kQuadComponentDraw}) {
    core.stateProducers.install(producer, std::make_unique<StateRender>(core));
  }
}

} // namespace crashbash::render
