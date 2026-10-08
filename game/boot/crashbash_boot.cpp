#include "crashbash_boot.h"

#include "boot_image_identity.h"
#include "core.h"
#include "crashbash_guest.h"
#include "execution_services.h"
#include "measured_guest_call.h"

#include <cstdint>

namespace crashbash {
namespace {

constexpr std::uint32_t kGpuGlobals = 0x80060000u;
constexpr std::uint32_t kVideoMode = 0x800637A8u;
constexpr std::uint32_t kFrameHeap = 0x8004E0F0u;
constexpr std::uint32_t kLoadedApplication = boot_image::kLoadAddress;
constexpr std::uint32_t kApplicationDescriptor = 0x80050008u;
constexpr std::uint32_t kApplicationDispatch = 0x8004E0DCu;

void beginGuestFrame(Core &core, std::uint32_t bytes, std::uint32_t savedRaOffset, std::uint32_t savedS0Offset) {
  core.r[29] -= bytes;
  core.mem_w32(core.r[29] + savedRaOffset, core.r[31]);
  core.mem_w32(core.r[29] + savedS0Offset, core.r[16]);
}

void beginProcessRunnerActivation(Core &core) {
  // 0x800270F0 prologue: callbacks still execute beneath the retail runner's live ABI frame and
  // callee-saved register state, even though the native driver replaces its loops.
  core.r[29] -= 32u;
  core.mem_w32(core.r[29] + 16u, core.r[16]);
  core.r[16] = guest::kInitialProcessState;
  core.mem_w32(core.r[29] + 28u, core.r[31]);
  core.mem_w32(core.r[29] + 24u, core.r[18]);
  core.mem_w32(core.r[29] + 20u, core.r[17]);
  psx::cpu::accountGuestInstructions(core, 7u);
  core.r[18] = kGpuGlobals;
  psx::cpu::accountGuestInstructions(core, 1u);
}

void runApplicationPrefix(Core &core) {
  // 0x80010158 frame; the retail call that follows is the non-returning process runner
  // CrashBashFrameDriver now represents, so this frame stays live.
  beginGuestFrame(core, 24u, 20u, 16u);
  core.mem_w32(guest::kDisplayFieldsPerFrame, 2u);
  core.r[16] = kLoadedApplication;
  measuredGuestCall(core, 0x80011BF8u, 0x80010194u, 15u, kFrameHeap, kLoadedApplication, 0x00186370u);
  measuredGuestCall(core, 0x80012E90u, 0x8001019Cu, 2u);
  measuredGuestCall(core, 0x80013CA4u, 0x800101A4u, 2u);
  measuredGuestCall(core, 0x800138A4u, 0x800101ACu, 2u);
  measuredGuestCall(core, 0x800134FCu, 0x800101BCu, 4u, kApplicationDescriptor, kLoadedApplication);
  core.r[4] = guest::kInitialProcessState;
  core.r[31] = 0x800101D0u;
  core.mem_w32(kApplicationDispatch, kLoadedApplication);
  psx::cpu::accountGuestInstructions(core, 5u);
  core.mem_w32(guest::kCurrentProcessState, guest::kInitialProcessState);
  beginProcessRunnerActivation(core);
}

} // namespace

void runBootPrefix(Core &core) {
  // 0x8002718C frame. Like the retail lifetime main, this activation is intentionally not unwound.
  beginGuestFrame(core, 24u, 20u, 16u);
  measuredGuestCall(core, 0x8002E7A8u, 0x8002719Cu, 4u);
  core.r[16] = kGpuGlobals;
  measuredGuestCall(core, 0x800318ECu, 0x800271A4u, 2u);
  measuredGuestCall(core, 0x8002E9ECu, 0x800271ACu, 2u, 0u);
  measuredGuestCall(core, 0x8002D488u, 0x800271B4u, 2u);
  measuredGuestCall(core, 0x8002EB60u, 0x800271BCu, 2u, 0u);
  measuredGuestCall(core, 0x80033494u, 0x800271C4u, 2u);
  measuredGuestCall(core, 0x8003342Cu, 0x800271CCu, 2u);
  core.mem_w32(kVideoMode, 0u);
  measuredGuestCall(core, 0x80027F00u, 0x800271D4u, 2u);
  measuredGuestCall(core, 0x8003351Cu, 0x800271E0u, 3u, core.mem_r32(kVideoMode));
  measuredGuestCall(core, 0x8002ED68u, 0x800271E8u, 2u, 0u);
  measuredGuestCall(core, 0x8002EF7Cu, 0x80027200u, 6u, 0x8005B640u, 0u, 0u, 0u);
  measuredGuestCall(core, 0x8002ED68u, 0x80027208u, 2u, 0u);
  measuredGuestCall(core, 0x8002AABCu, 0x80027210u, 2u);
  measuredGuestCall(core, 0x80028C94u, 0x80027218u, 2u);
  core.r[31] = 0x80027220u;
  psx::cpu::accountGuestInstructions(core, 2u);
  runApplicationPrefix(core);
}

} // namespace crashbash
