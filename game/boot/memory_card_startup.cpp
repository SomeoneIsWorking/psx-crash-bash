#include "memory_card_startup.h"

#include "core.h"
#include "crashbash_guest.h"
#include "execution_services.h"
#include "guest_execution.h"
#include "measured_guest_call.h"

#include <cstdint>
#include <lucent/log.h>

namespace crashbash {
namespace {

constexpr std::uint32_t kChangeClearPad = 0x8003B17Cu;
constexpr std::uint32_t kEnterCriticalSection = 0x8003B19Cu;
constexpr std::uint32_t kExitCriticalSection = 0x8003B15Cu;
constexpr std::uint32_t kCardInitialized = 0x8004B9B8u;
constexpr std::uint32_t kInitializeCardBios = 0x8004BC3Cu;
constexpr std::uint32_t kInstallCardVector = 0x8004BE2Cu;
constexpr std::uint32_t kInstallCardHandler = 0x8004BD28u;
constexpr std::uint32_t kInstallCardDevice = 0x8004BDBCu;
constexpr std::uint32_t kResetCardState = 0x8004BC4Cu;

void memoryCardStartupOwned(Core *core) {
  // Retail 0x800486DC waits VSync(0) before its memory-card critical section; native boot precedes any guest
  // frame that could race, so the wait is elided while every other call, branch, register and tick is kept.
  core->r[29] -= 32u;
  core->mem_w32(core->r[29] + 16u, core->r[16]);
  core->r[16] = core->r[4];
  core->r[4] = 0u;
  core->mem_w32(core->r[29] + 24u, core->r[31]);
  core->mem_w32(core->r[29] + 20u, core->r[17]);
  core->r[31] = 0x800486F8u;
  psx::cpu::accountGuestInstructions(*core, 7u);
  runtime::dispatchGuest(*core, kChangeClearPad);

  core->r[4] = 0u;
  psx::cpu::accountGuestInstructions(*core, 2u);

  measuredGuestCall(*core, kEnterCriticalSection, 0x80048708u, 2u);
  core->r[17] = measuredGuestCall(*core, kCardInitialized, 0x80048710u, 2u);
  psx::cpu::accountGuestInstructions(*core, 2u);
  if (core->r[2] == 0u) {
    core->r[16] = 0u;
    psx::cpu::accountGuestInstructions(*core, 1u);
  }

  measuredGuestCall(*core, kInitializeCardBios, 0x80048724u, 2u, core->r[16]);
  measuredGuestCall(*core, kInstallCardVector, 0x8004872Cu, 2u);
  measuredGuestCall(*core, kInstallCardHandler, 0x80048734u, 2u);
  measuredGuestCall(*core, kInstallCardDevice, 0x8004873Cu, 2u);
  measuredGuestCall(*core, kResetCardState, 0x80048744u, 2u);
  core->r[2] = 1u;
  psx::cpu::accountGuestInstructions(*core, 3u);
  if (core->r[17] == 1u) {
    measuredGuestCall(*core, kExitCriticalSection, 0x80048758u, 2u);
  }

  core->r[31] = core->mem_r32(core->r[29] + 24u);
  core->r[17] = core->mem_r32(core->r[29] + 20u);
  core->r[16] = core->mem_r32(core->r[29] + 16u);
  core->r[29] += 32u;
  psx::cpu::accountGuestInstructions(*core, 5u);
}

} // namespace

void registerMemoryCardStartupOverride(Core &core) {
  runtime::registerNativeOverride(core,
                                  runtime::GuestImage::Resident,
                                  guest::kMemoryCardStartup,
                                  "CrashBash::MemoryCardStartup",
                                  memoryCardStartupOwned);
}

} // namespace crashbash
