#include "crashbash_frame_driver.h"

#include "core.h"
#include "crashbash_guest.h"
#include "dev_arena.h"
#include "execution_services.h"
#include "game.h"
#include "guest_execution.h"
#include "measured_guest_call.h"
#include "snapshot.h"

#include <cstdlib>
#include <lucent/log.h>
#include <optional>

namespace crashbash {

CrashBashFrameDriver::CrashBashFrameDriver(Game &game) : game_(game) {}

void CrashBashFrameDriver::enterProcessState(Core &core, std::uint32_t state) {
  activeState_ = state;
  core.r[16] = state;
  core.mem_w32(guest::kCurrentProcessState, state);
  const std::uint32_t enter = core.mem_r32(state);
  if (enter == 0) {
    lucent::error("crashbash-frame", "process state 0x{:08X} has no enter function", state);
    std::abort();
  }
  measuredGuestCall(core, enter, 0x80027120u, 4u);
  psx::cpu::accountGuestInstructions(core, 4u);
  ++stateEntries_;
  dwellFrames_ = 0;
  dwellReports_ = 0;
  lucent::info("crashbash-frame",
               "process state -> 0x{:08X} (entry #{}, enter=0x{:08X} update=0x{:08X} present=0x{:08X})",
               state,
               stateEntries_,
               enter,
               core.mem_r32(state + 4u),
               core.mem_r32(state + 8u));
}

void CrashBashFrameDriver::deliverDisplayFields(Core &core, std::uint32_t fields) {
  if (fields == 0 || fields > 4) {
    lucent::error("crashbash-frame", "retail display requested {} fields; expected a bounded positive cadence", fields);
    std::abort();
  }
  if (deliveredFields_ != 0) {
    lucent::error("crashbash-frame", "retail process iteration invoked DisplayFrame more than once");
    std::abort();
  }
  deliveredFields_ = fields;
  for (std::uint32_t field = 0; field < fields; ++field) {
    const std::uint32_t before = core.mem_r32(guest::kVblankCounter);
    const R3000 interrupted = static_cast<const R3000 &>(core);
    runtime::dispatchGuest(core, guest::kVblankRoot);
    static_cast<R3000 &>(core) = interrupted;
    const std::uint32_t after = core.mem_r32(guest::kVblankCounter);
    if (after != before + 1u) {
      lucent::error("crashbash-frame",
                    "VBlank root 0x{:08X} advanced counter 0x{:08X} by {} instead of one field",
                    guest::kVblankRoot,
                    guest::kVblankCounter,
                    after - before);
      std::abort();
    }
    // No Audio phase bracket: phases partition time, so the SPU advance counts as GameLogic and `audio` reads 0.
    game_.spu_audio.frame();
  }
}

void CrashBashFrameDriver::finishUpdateSlice(Core &core) {
  measuredGuestCall(core, presentFn_, kPresentReturnPc, 4u);
  psx::cpu::accountGuestInstructions(core, 4u);
}

void CrashBashFrameDriver::stepFrame(Core &core, std::uint32_t frame) {
  // Developer request, applied at the frame boundary.
  devArena_.applyArmed(core, game_.pad, frame);
  // Per-frame profiler (`PSXPORT_DEBUG=perf`).
  game_.perf.frameBegin();
  game_.timing.logicFrame = frame;
  game_.timing.frameTick();
  core.rsub.otAttr.beginLogicFrame(frame);
  game_.pad.serviceFrame();
  deliveredFields_ = 0;
  // Above is host-side per-tick work; below is the guest tick and present.
  game_.perf.markPre();
  game_.perf.phaseBegin(GpuPerf::Phase::GameLogic);

  std::uint32_t state = core.mem_r32(guest::kCurrentProcessState);
  bool enteredState = false;
  while (state != activeState_) {
    if (state == 0) {
      lucent::error("crashbash-frame", "retail process runner has no current state");
      std::abort();
    }
    if (core.pending_work) {
      psx::cpu::servicePendingWork(core);
    }
    enterProcessState(core, state);
    enteredState = true;
    const std::uint32_t next = core.mem_r32(guest::kCurrentProcessState);
    if (next == activeState_) {
      break;
    }
    psx::cpu::accountGuestInstructions(core, 4u);
    state = next;
  }

  // Retail 0x800270F0 runs update and present as one iteration after the transition check; the update call
  // runs to its return address so a long iteration is not split across host turns.
  if (core.mem_r32(guest::kCurrentProcessState) == activeState_) {
    if (enteredState) {
      core.r[17] = 0x80060000u;
      psx::cpu::accountGuestInstructions(core, 1u);
    }
    if (core.pending_work) {
      psx::cpu::servicePendingWork(core);
    }
    const std::uint32_t update = core.mem_r32(activeState_ + 4u);
    const std::uint32_t present = core.mem_r32(activeState_ + 8u);
    if (update == 0 || present == 0) {
      lucent::error("crashbash-frame", "process state 0x{:08X} has an incomplete update/present pair", activeState_);
      std::abort();
    }
    updateFn_ = update;
    presentFn_ = present;
    measuredGuestCallSetup(core, kUpdateReturnPc, 4u);
    runtime::runGuestCallToReturn(core, update, kUpdateReturnPc, "Crash Bash process update", std::nullopt);
    finishUpdateSlice(core);
  }

  reportProgress(core, frame);
  snapshot_tick(&core);
  game_.perf.phaseEnd(GpuPerf::Phase::GameLogic);
  game_.perf.phaseBegin(GpuPerf::Phase::Present);
  if (deliveredFields_ == 0) {
    game_.presentation.commitUnpresented(&core);
  } else {
    frameCut_.noteFrameEnded(core);
    game_.presentation.commit(&core, static_cast<int>(deliveredFields_), game_.temporalPresentation.get());
  }
  game_.perf.phaseEnd(GpuPerf::Phase::Present);
  game_.perf.frameEnd();
}

void CrashBashFrameDriver::reportProgress(Core &core, std::uint32_t frame) {
  // The nested root scene (set once by the resident main at 0x800101CC); the mode machine is the scene record below.
  const std::uint32_t mode = core.mem_r32(guest::kAppModeVtable);
  if (mode != appMode_) {
    appMode_ = mode;
    ++appModeChanges_;
    if (mode == 0) {
      lucent::info("crashbash-frame", "f{}: root app scene cleared to 0 — the shell has no scene to dispatch", frame);
    } else {
      lucent::info("crashbash-frame",
                   "f{}: root app scene -> 0x{:08X} (change #{}, enter=0x{:08X} update=0x{:08X} "
                   "present=0x{:08X}) — the shell's own scene; the mode machine is 0x{:08X}",
                   frame,
                   mode,
                   appModeChanges_,
                   core.mem_r32(mode),
                   core.mem_r32(mode + guest::kSceneTransitionUpdateSlot),
                   core.mem_r32(mode + guest::kSceneTransitionPresentSlot),
                   guest::kSceneTransition);
    }
  }

  // The live scene machine; boot, menu, attract and gameplay are all scenes here.
  const std::uint32_t scene = core.mem_r32(guest::kSceneTransition + guest::kSceneCurrentSlot);
  if (scene != scene_) {
    scene_ = scene;
    ++sceneChanges_;
    if (scene == 0) {
      lucent::info("crashbash-frame", "f{}: scene machine has NO current scene", frame);
    } else {
      lucent::info("crashbash-frame",
                   "f{}: scene -> 0x{:08X} (change #{}, enter=0x{:08X} update=0x{:08X} "
                   "present=0x{:08X}; previous=0x{:08X} target=0x{:08X} flags=0x{:X} clock age {})",
                   frame,
                   scene,
                   sceneChanges_,
                   core.mem_r32(scene + guest::kSceneTransitionEnterSlot),
                   core.mem_r32(scene + guest::kSceneTransitionUpdateSlot),
                   core.mem_r32(scene + guest::kSceneTransitionPresentSlot),
                   core.mem_r32(guest::kSceneTransition + guest::kScenePreviousSlot),
                   core.mem_r32(guest::kSceneTransition + guest::kSceneTargetSlot),
                   core.mem_r32(guest::kSceneTransition + guest::kSceneFlagsSlot),
                   core.mem_r32(guest::kSceneTransitionClock + guest::kSceneClockAgeSlot));
    }
  }

  ++dwellFrames_;
  if (activeState_ == 0) {
    lucent::info("crashbash-frame", "f{}: no process state is active — nothing was updated or presented", frame);
    return;
  }
  if (updateFn_ == 0) {
    lucent::info("crashbash-frame",
                 "f{}: state 0x{:08X} is active but its update/present pair did NOT run this frame",
                 frame,
                 activeState_);
    return;
  }
  if ((dwellFrames_ & (dwellFrames_ - 1u)) != 0) {
    return; // dwelling, and this is not one of the capped boring samples
  }
  ++dwellReports_;
  lucent::info("crashbash-frame",
               "f{}: dwelling in state 0x{:08X} for {} frame(s) (update=0x{:08X} present=0x{:08X}, "
               "{} field(s) delivered, vblank counter 0x{:08X}, app mode 0x{:08X} / scene 0x{:08X} unchanged for "
               "the whole dwell)",
               frame,
               activeState_,
               dwellFrames_,
               updateFn_,
               presentFn_,
               deliveredFields_,
               core.mem_r32(guest::kVblankCounter),
               appMode_,
               scene_);
}

CrashBashFrameDriver &frameDriver(Core &core) {
  if (!core.game || !core.game->frameDriver) {
    lucent::error("crashbash-frame", "Crash Bash runtime has no FrameDriver");
    std::abort();
  }
  return static_cast<CrashBashFrameDriver &>(*core.game->frameDriver);
}

} // namespace crashbash
