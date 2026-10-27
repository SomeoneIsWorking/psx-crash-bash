#pragma once

#include "debug/debug_menu.h"
#include "execution/executable_identity.h"
#include "game_runtime.h"
#include "guest_widescreen_projection.h"
#include "input/recording_phase.h"
#include "stock_libcd.h"
#include "stock_vsync.h"

#include <cstdint>
#include <memory>

namespace crashbash {

// The resident executable's code: the header's text up to BSS, which the text size also covers.
inline constexpr std::uint32_t kResidentBssBegin = 0x8006E9F0u;
inline constexpr GuestAddressRange kResidentCodeRange{identity::kTextAddress & 0x1fffffffu,
                                                      kResidentBssBegin & 0x1fffffffu};
static_assert(kResidentCodeRange.begin < kResidentCodeRange.end);
static_assert(kResidentCodeRange.end <= (identity::kTextAddress & 0x1fffffffu) + identity::kTextBytes);

// Replays crt0 and the finite boot prefix, then FrameDriver owns each process iteration. Records
// interpolate through the model, text, panel and quad producers.
class Runtime final : public psx::game::GameRuntime {
public:
  Runtime();

  // Authenticates the resident executable and binds its native owners to that image generation.
  psx::cpu::PsxExeLoadResult admitExecutable(Core &core, std::span<const std::uint8_t> bytes) override;
  void startProgram(Core &core) override;
  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;

  // The renderer allocates both parity pools from the guest heap and publishes their live bounds.
  std::optional<psx::gpu::PacketPoolExtent> packetPool(Core &core) const override;
  bool interpolates() const override {
    return true;
  }
  const psx::present::StateProducer *stateProducer(Core &core, std::uint32_t producer) const override;
  bool sealedFrameIsCut(Core &core) const override;
  const psx::present::GuestWidescreenProjection *guestWidescreenProjection() const override;

  const psx::game::TitleDebugMenu &debugMenu() const override {
    return debugMenu_;
  }
  std::uint64_t inputPhase(Core &core) const override {
    return recordingPhase_.of(core);
  }
  psx::state::NativeStatePort *nativeState(Core &core) const override;
  // The title's guest-call turn cap beside psxport's guest-call census.
  void reportRun(Core &core) const override;

private:
  psx::cd::StockLibcd libcd_;
  psx::hle::StockVSync vsync_;
  RecordingPhase recordingPhase_;
  psx::present::GuestWidescreenProjection aspectPolicy_;
  debug::DebugMenu debugMenu_;
};

} // namespace crashbash
