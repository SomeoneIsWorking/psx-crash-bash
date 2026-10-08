#include "gpu_timeout.h"

#include "core.h"
#include "crashbash_guest.h"
#include "execution_services.h"
#include "guest_execution.h"

#include <cstdint>
#include <lucent/log.h>

namespace crashbash {
namespace {

constexpr std::uint32_t kGpuTimeoutDeadline = 0x80067A90u;
constexpr std::uint32_t kGpuTimeoutCount = 0x80067A94u;
constexpr std::uint32_t kSynchronousDeadline = 0x7FFFFFFFu;
constexpr std::uint32_t kGpuTransferCommand = 0x800679F4u;
constexpr std::uint32_t kGpuDriver = 0x8006794Cu;

void armSynchronousGpuTimeout(Core &core) {
  // OT DMA completes synchronously; keep the two guest state writes with an unreachable deadline.
  core.mem_w32(kGpuTimeoutDeadline, kSynchronousDeadline);
  core.mem_w32(kGpuTimeoutCount, 0u);
}

void gpuTimeoutArmOwned(Core *core) {
  armSynchronousGpuTimeout(*core);
  core->r[2] = kSynchronousDeadline;
}

void gpuTimeoutCheckOwned(Core *core) {
  // Synchronous GPU: nothing to time out.
  core->r[2] = 0u;
}

void finishGpuTransfer(Core *core) {
  core->r[31] = core->mem_r32(core->r[29] + 28u);
  core->r[18] = core->mem_r32(core->r[29] + 24u);
  core->r[17] = core->mem_r32(core->r[29] + 20u);
  core->r[16] = core->mem_r32(core->r[29] + 16u);
  core->r[29] += 32u;
  psx::cpu::accountGuestInstructions(*core, 6u);
}

void gpuTransferOwned(Core *core) {
  core->r[29] -= 32u;
  core->mem_w32(core->r[29] + 16u, core->r[16]);
  core->r[16] = core->r[4];
  core->mem_w32(core->r[29] + 24u, core->r[18]);
  core->r[18] = core->r[5];
  core->mem_w32(core->r[29] + 20u, core->r[17]);
  core->r[17] = core->r[6];
  core->r[4] = 0x8004D040u;
  core->mem_w32(core->r[29] + 28u, core->r[31]);
  core->r[31] = 0x8003168Cu;
  core->r[5] = core->r[16];
  psx::cpu::accountGuestInstructions(*core, 12u);
  runtime::dispatchGuest(*core, 0x8002EDD0u);

  // Retail polls VSync(-1) and GPU/DMA readiness; submission is synchronous, so record idle and take the ready path.
  armSynchronousGpuTimeout(*core);

  core->r[5] = 0x8003189Cu;
  core->r[31] = 0x8003171Cu;
  core->r[4] = 2u;
  psx::cpu::accountGuestInstructions(*core, 4u);
  runtime::dispatchGuest(*core, 0x8003194Cu);
  const std::int16_t height = static_cast<std::int16_t>(core->mem_r16(core->r[16] + 4u));
  core->r[2] = 0xFFFFFFFFu;
  psx::cpu::accountGuestInstructions(*core, 4u);
  if (height == 0) {
    finishGpuTransfer(core);
    return;
  }
  const std::int16_t width = static_cast<std::int16_t>(core->mem_r16(core->r[16] + 6u));
  core->r[2] = core->r[17] << 16u;
  psx::cpu::accountGuestInstructions(*core, 4u);
  if (width == 0) {
    core->r[2] = 0xFFFFFFFFu;
    psx::cpu::accountGuestInstructions(*core, 2u);
    finishGpuTransfer(core);
    return;
  }
  core->r[3] = core->r[18] & 0xFFFFu;
  core->r[2] |= core->r[3];
  gte_hold_src(core, 5, core->r[16]);
  core->r[5] = core->mem_r32(core->r[16]);
  core->r[3] = core->mem_r32(kGpuDriver);
  core->r[4] = kGpuTransferCommand;
  core->mem_w32(core->r[4] + 4u, core->r[2]);
  core->mem_w32(core->r[4], core->r[5]);
  gte_copy_pz(core, 5, core->r[4]);
  gte_hold_src(core, 2, core->r[16] + 4u);
  core->r[2] = core->mem_r32(core->r[16] + 4u);
  core->mem_w32(core->r[4] + 8u, core->r[2]);
  gte_copy_pz(core, 2, core->r[4] + 8u);
  core->r[2] = core->mem_r32(core->r[3] + 24u);
  core->r[31] = 0x80031784u;
  core->r[4] -= 8u;
  psx::cpu::accountGuestInstructions(*core, 16u);
  runtime::dispatchGuest(*core, core->r[2]);
  core->r[2] = 0u;
  psx::cpu::accountGuestInstructions(*core, 1u);
  finishGpuTransfer(core);
}

} // namespace

void registerGpuTimeoutOverrides(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kGpuTimeoutArm, "CrashBash::GpuTimeoutArm", gpuTimeoutArmOwned);
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kGpuTimeoutCheck, "CrashBash::GpuTimeoutCheck", gpuTimeoutCheckOwned);
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kGpuTransfer, "CrashBash::GpuTransfer", gpuTransferOwned);
}

} // namespace crashbash
