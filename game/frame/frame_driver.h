#pragma once

#include "game_runtime.h"

#include <cstdint>

class Core;
class Game;

namespace crashbash {

class FrameDriver final : public psx::game::FrameDriver {
public:
  explicit FrameDriver(Game &game);

  void stepFrame(Core &core, std::uint32_t frame) override;

  // Replaces DisplayFrame's VSync(n): the frame spans the guest's cadence word 0x8004E0E0, never the time its work took.
  void deliverDisplayFields(Core &core, std::uint32_t requested);

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
};

} // namespace crashbash
