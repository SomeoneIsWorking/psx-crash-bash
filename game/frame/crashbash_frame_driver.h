#pragma once

#include "component_incarnation.h"
#include "dev_arena.h"
#include "execution_exit.h"
#include "frame_cut.h"
#include "game_runtime.h"
#include "packet_collector.h"
#include "polar_push_contact.h"

#include <cstdint>

class Core;
class Game;

namespace crashbash {

class CrashBashFrameDriver final : public FrameDriver {
public:
  explicit CrashBashFrameDriver(Game &game);

  void stepFrame(Core &core, std::uint32_t frame) override;

  // Called at the exact point native DisplayFrame removes retail VSync(fields). This owns field
  // callbacks and audio; presentation remains the single commit at the frame-driver tail.
  void deliverDisplayFields(Core &core, std::uint32_t fields);

  // The developer arena request the control channel arms and this driver applies.
  debug::DevArena &devArena() {
    return devArena_;
  }

  // The Polar Push contact totals live here because this is the per-Core object native overrides reach
  // from a bare `Core *`.
  polar::ContactCensus &polarContactCensus() {
    return polarContactCensus_;
  }
  // The render components' incarnations, which the model producer's object keys name.
  render::ComponentIncarnations &componentIncarnations() {
    return componentIncarnations_;
  }
  // The packets a producer call links, which its state keeps.
  render::PacketCollector &packetCollector() {
    return packetCollector_;
  }
  // Whether the record this frame sealed is a cut (TitleAdapter::sealedFrameIsCut).
  const FrameCut &frameCut() const {
    return frameCut_;
  }

private:
  static inline constexpr std::uint32_t kUpdateReturnPc = 0x80027144u;
  static inline constexpr std::uint32_t kPresentReturnPc = 0x80027154u;

  void enterProcessState(Core &core, std::uint32_t state);
  // Runs the pair's present after the completed update (carried by runtime::runGuestCallToReturn).
  void finishUpdateSlice(Core &core);
  // Progress reporting: state changes always print, dwelling in one state is capped.
  void reportProgress(Core &core, std::uint32_t frame);

  Game &game_;
  std::uint32_t activeState_ = 0;
  std::uint32_t deliveredFields_ = 0;
  std::uint32_t stateEntries_ = 0;
  std::uint32_t dwellFrames_ = 0;
  std::uint32_t dwellReports_ = 0;
  std::uint32_t updateFn_ = 0;
  std::uint32_t presentFn_ = 0;
  // The root scene (`kAppModeVtable`) and the live scene record (`0x8009F658`) that selects boot / menu / gameplay.
  std::uint32_t appMode_ = 0;
  std::uint32_t appModeChanges_ = 0;
  std::uint32_t scene_ = 0;
  std::uint32_t sceneChanges_ = 0;
  debug::DevArena devArena_;
  polar::ContactCensus polarContactCensus_;
  render::ComponentIncarnations componentIncarnations_;
  render::PacketCollector packetCollector_;
  FrameCut frameCut_;
};

CrashBashFrameDriver &frameDriver(Core &core);

} // namespace crashbash
