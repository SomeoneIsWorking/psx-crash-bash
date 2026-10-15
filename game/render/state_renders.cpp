#include "render/state_renders.h"

#include "core.h"
#include "render/leaf_state.h"
#include "render/model_state.h"
#include "state_bytes.h"
#include "state_producer.h"
#include "title/guest.h"

#include <memory>

namespace crashbash::render {
namespace {

std::uint32_t tagOf(std::span<const std::byte> state) {
  return psx::present::StateReader(state).get<std::uint32_t>();
}

// Picks the render by the state's tag; a producer saves a mesh state or a leaf state per call.
class StateRender final : public psx::present::StateProducer {
public:
  explicit StateRender(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states, psx::present::PrimitiveSink &sink) const override {
    // A state of another kind is a different draw: nothing to move from.
    const auto earlier = tagOf(states.from) == tagOf(states.to) ? states.from : states.to;
    if (tagOf(states.to) == kFaceStateTag) {
      renderFaceState(core_, earlier, states.to, states.t, sink);
    } else {
      renderLeafState(core_, earlier, states.to, states.t, sink);
    }
  }

private:
  Core &core_;
};

} // namespace

void registerStateRenders(Core &core, psx::present::StateProducerTable &renders) {
  for (const std::uint32_t producer : {guest::kModelDraw,
                                       guest::kTextComponentDraw,
                                       guest::kPanelComponentDraw,
                                       guest::kQuadComponentDraw,
                                       guest::kBootHudDraw,
                                       guest::kBootMenuPageDraw}) {
    renders.install(producer, std::make_unique<StateRender>(core));
  }
}

} // namespace crashbash::render
