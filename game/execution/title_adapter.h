#pragma once

#include <cstdio>

#include "crashbash_input_phase.h"
#include "game_runtime.h"
#include "guest_widescreen_projection.h"
#include "psx_exe_image.h"

#include <memory>

namespace crashbash {

// Typed psxport composition boundary: GuestExecution, the authenticated image lifecycle and the native frame owners.
class TitleAdapter final : public GameRuntime {
public:
  // Authenticates the resident executable and binds its native owners to that image generation.
  psx::cpu::PsxExeLoadResult admitExecutable(Core &core, std::span<const std::uint8_t> bytes) override;

  const GuestProgramImage *guestProgramImage() const override;
  const PlatformHlePlan *platformHlePlan() const override;
  // Guest globals naming the two heap packet pools.
  const GuestPacketPoolWindows *guestPacketPoolWindows() const override;
  // The developer `arena` command only.
  bool controlCommand(Core &core, const char *cmd, const char *line, FILE *out) override;
  bool replCommand(Core &core, const char *cmd, const char *line) override;
  // Pad recording phase (crashbash_input_phase.h), held by value.
  std::uint64_t inputPhase(Core &core) const override {
    return inputPhase_.of(core);
  }
  psx::state::NativeStatePort *nativeState(Core &core) const override;

  void *createContext(Core &core) override;
  void destroyContext(void *context) override;
  void registerOverrides(Game &game) override;
  void bootInit(Core &core) override;
  RenderCapabilities renderCapabilities() const override;
  bool guestVramIsPicture(const Game &game) const override;
  const GuestWidescreenProjection *guestWidescreenProjection() const override;
  bool sealedFrameIsCut(Core &core) const override;
  std::unique_ptr<FrameDriver> createFrameDriver(Game &game) override;
  // The title's guest-call turn cap beside psxport's guest-call census.
  void reportRun(Core &core) const override;

private:
  InputPhase inputPhase_;
  GuestWidescreenProjection aspectPolicy_;
};

} // namespace crashbash
