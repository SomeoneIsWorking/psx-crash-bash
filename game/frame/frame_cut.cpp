#include "frame_cut.h"

#include "core.h"

#include <lucent/log.h>

namespace crashbash {

FrameCut::SceneIdentity FrameCut::sample(Core &core) {
  return SceneIdentity{
      .processState = core.mem_r32(guest::kCurrentProcessState),
      .scene = core.mem_r32(guest::kSceneTransition + guest::kSceneCurrentSlot),
      .menuScreen = core.mem_r32(guest::kMenuSceneTransition + guest::kSceneCurrentSlot),
  };
}

void FrameCut::noteFrameEnded(Core &core) {
  noteFrameEnded(sample(core));
}

void FrameCut::noteFrameEnded(const SceneIdentity &current) {
  cut_ = !previous_.has_value() || *previous_ != current;
  lucent::debug("cut",
                "cut={} state={:08X} scene={:08X} menu={:08X}",
                cut_,
                current.processState,
                current.scene,
                current.menuScreen);
  previous_ = current;
}

} // namespace crashbash
