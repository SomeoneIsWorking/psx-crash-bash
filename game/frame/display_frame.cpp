#include "display_frame.h"

#include "core.h"
#include "crashbash_frame_driver.h"
#include "crashbash_guest.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_execution.h"
#include "measured_guest_call.h"
#include "ordering_table_slots.h"
#include "packet_collector.h"

#include <cstdint>
#include <lucent/log.h>

namespace crashbash {
namespace {

constexpr std::uint32_t kFrameHeap = 0x8004E0F0u;
constexpr std::uint32_t kFrameHeapWorkTicks = 0x8004E0E8u;
constexpr std::uint32_t kFrameHeapBudgetSpent = 0x8004E0ECu;
constexpr std::uint32_t kFrameTimerSample = 0x8004E0E4u;
constexpr std::uint32_t kDisplayDescriptor = 0x8005B698u;
constexpr std::uint32_t kOrderingTableDrawOffset = 0x3FFCu;
constexpr std::uint32_t kOrderingTableHeadOffset = 0x4000u;
constexpr std::uint32_t kVideoMode = 0x800637A8u;
constexpr std::uint32_t kDisplayIndex = 0x8005B690u;
constexpr std::uint32_t kDisplayResource = 0x800643E8u;

constexpr GuestFrameSpill kDisplaySpills[] = {{18, 48}, {16, 40}, {31, 52}, {17, 44}};
constexpr GuestFrameSpill kAllocatorSpills[] = {
    {19, 28}, {31, 44}, {22, 40}, {21, 36}, {20, 32}, {18, 24}, {17, 20}, {16, 16}};

class FrameArena {
public:
  FrameArena(Core &core, std::uint32_t address) : core_(core), address_(address) {}

  std::uint32_t activeBlock() const {
    return core_.mem_r32(address_ + 12u);
  }
  void setActiveBlock(std::uint32_t block) {
    core_.mem_w32(address_ + 12u, block);
  }
  std::uint32_t delayedBlock() const {
    return core_.mem_r32(address_ + 16u);
  }
  void setDelayedBlock(std::uint32_t block) {
    core_.mem_w32(address_ + 16u, block);
  }
  std::uint32_t address() const {
    return address_;
  }

private:
  Core &core_;
  std::uint32_t address_;
};

std::uint32_t eligibleSplit(Core &core, const FrameArena &arena) {
  const std::uint32_t block = arena.activeBlock();
  if (block == 0) {
    return 0;
  }
  const std::uint32_t candidate = block + 24u + core.mem_r32(block + 8u) * 4u;
  if (core.mem_r32(candidate + 4u) == 0xFFFFFFFFu || core.mem_r32(candidate + 8u) != 0u) {
    return 0;
  }
  const std::uint16_t flags = core.mem_r16(candidate);
  if ((flags & 2u) != 0u && (flags & 1u) == (core.mem_r32(kDisplayIndex) & 1u)) {
    return 0;
  }
  return candidate;
}

void copyWords(Core &core, std::uint32_t destination, std::uint32_t source, std::uint32_t count) {
  for (std::uint32_t index = 0; index < count; ++index) {
    core.mem_w32(destination + index * 4u, core.mem_r32(source + index * 4u));
  }
}

void splitActiveBlock(Core &core, FrameArena &arena, std::uint32_t candidate) {
  const std::uint32_t active = arena.activeBlock();
  measuredGuestCall(core, 0x80011B74u, 0x8001059Cu, 9u, active, arena.address());

  const std::uint32_t oldWords = core.mem_r32(active + 8u);
  const std::uint32_t candidateWords = core.mem_r32(candidate + 8u);
  const std::uint32_t activeLink = core.mem_r32(active + 12u);
  copyWords(core, active, candidate, 6u);
  core.mem_w32(active + 12u, activeLink);
  core.mem_w32(core.mem_r32(active + 4u), active + 24u);
  copyWords(core, active + 24u, candidate + 24u, candidateWords);

  const std::uint32_t next = active + 24u + oldWords * 4u;
  const std::uint32_t remainder = candidate + 36u + candidateWords * 4u;
  core.mem_w32(remainder, next);
  core.mem_w32(next + 8u, oldWords);
  core.mem_w32(next + 4u, 0xFFFFFFFFu);
  core.mem_w16(next, 0u);
  core.mem_w32(next + 12u, active);
  if (active == arena.activeBlock()) {
    arena.setActiveBlock(measuredGuestCall(core, 0x80011BC0u, 0x8001066Cu, 2u, next));
  }
  measuredGuestCall(core, 0x800110A8u, 0x8001067Cu, 3u, next + 24u, arena.address());
}

void retireDelayedBlocks(Core &core, FrameArena &arena) {
  std::uint32_t previous = 0;
  std::uint32_t block = arena.delayedBlock();
  while (block != 0) {
    const std::uint16_t remaining = static_cast<std::uint16_t>(core.mem_r16(block + 2u) - 1u);
    core.mem_w16(block + 2u, remaining);
    const std::uint32_t next = core.mem_r32(block + 20u);
    if (static_cast<std::int16_t>(remaining) <= 0) {
      if (previous == 0) {
        arena.setDelayedBlock(next);
      } else {
        core.mem_w32(previous + 20u, next);
      }
      measuredGuestCall(core, 0x800110A8u, 0x80010778u, 3u, block + 24u, kFrameHeap);
    } else {
      previous = block;
    }
    block = next;
  }
}

void serviceFrameArena(Core &core, FrameArena &arena) {
  GuestFrame<48, 8> frame(&core, kAllocatorSpills);
  core.r[19] = arena.address();
  std::uint32_t candidate = eligibleSplit(core, arena);
  const std::uint32_t startHsync = core.game->timing.hSyncCounter();
  std::int32_t budget = (510 - static_cast<std::int32_t>(startHsync)) * 112;
  while (candidate != 0 && (budget -= static_cast<std::int32_t>(core.mem_r32(candidate + 8u))) > 0) {
    const std::uint32_t before = core.game->timing.hSyncCounter();
    core.mem_w32(kFrameHeapBudgetSpent, core.mem_r32(kFrameHeapBudgetSpent) + core.mem_r32(candidate + 8u));
    splitActiveBlock(core, arena, candidate);
    const std::uint32_t after = core.game->timing.hSyncCounter();
    core.mem_w32(kFrameHeapWorkTicks, core.mem_r32(kFrameHeapWorkTicks) + (after - before));
    candidate = eligibleSplit(core, arena);
  }
  retireDelayedBlocks(core, arena);
}

class DisplayEnvironment {
public:
  DisplayEnvironment(Core &core, std::uint32_t address) : core_(core), address_(address) {}

  void copyOrigin(std::uint32_t source) {
    core_.mem_w32(address_, core_.mem_r32(source));
    core_.mem_w32(address_ + 4u, core_.mem_r32(source + 4u));
  }
  void configure(std::uint32_t descriptor, bool alternateMode) {
    core_.mem_w16(address_ + 8u, 0u);
    core_.mem_w16(address_ + 10u, alternateMode ? 16u : 8u);
    core_.mem_w16(address_ + 12u, 256u);
    core_.mem_w16(address_ + 14u, alternateMode ? 256u : 240u);
    core_.mem_w8(address_ + 16u, core_.mem_r8(descriptor + 20u));
    core_.mem_w8(address_ + 17u, core_.mem_r8(descriptor + 16u));
  }
  std::uint32_t address() const {
    return address_;
  }

private:
  Core &core_;
  std::uint32_t address_;
};

void displayFrameOwned(Core *core) {
  GuestFrame<56, 4> frame(core, kDisplaySpills);
  core->r[18] = core->r[4];
  const std::uint32_t descriptor = core->mem_r32(kDisplayDescriptor);
  const std::uint32_t orderingTable = core->mem_r32(guest::kOrderingTablePointer);
  core->r[16] = descriptor;

  measuredGuestCall(*core, 0x800338E8u, 0x800272D8u, 11u, core->mem_r32(kDisplayResource));
  measuredGuestCall(*core, 0x8002ED68u, 0x800272E0u, 2u, 0u);
  FrameArena arena(*core, kFrameHeap);
  serviceFrameArena(*core, arena);

  if (descriptor != 0) {
    const bool alternateMode = core->mem_r32(kVideoMode) != 0u;
    const std::uint32_t origin = orderingTable == guest::kOrderingTableA ? descriptor : descriptor + 8u;
    DisplayEnvironment environment(*core, core->r[29] + 16u);
    environment.copyOrigin(origin);
    environment.configure(descriptor, alternateMode);

    core->mem_w32(kFrameTimerSample, core->game->timing.hSyncCounter());
    const std::uint32_t fields = core->r[18];
    frameDriver(*core).deliverDisplayFields(*core, fields);

    measuredGuestCall(*core, 0x8002F598u, 0x80027398u, 2u, environment.address());
    frameDriver(*core).packetCollector().commit(*core);
    measuredGuestCall(*core, 0x8002F35Cu, 0x800273A8u, 4u, orderingTable + kOrderingTableDrawOffset);
    const std::uint32_t nextOrderingTable =
        orderingTable == guest::kOrderingTableA ? guest::kOrderingTableB : guest::kOrderingTableA;
    core->mem_w32(guest::kOrderingTablePointer, nextOrderingTable);
    render::followDrawBase(*core, orderingTable, nextOrderingTable);
    core->mem_w32(kDisplayIndex, core->mem_r32(kDisplayIndex) + 1u);
    measuredGuestCall(*core, 0x8002F254u, 0x800273DCu, 7u, nextOrderingTable, guest::kOrderingTableBuckets);
    core->mem_w32(nextOrderingTable + guest::kOrderingTablePoolCursor,
                  core->mem_r32(nextOrderingTable + kOrderingTableHeadOffset));
  }
  measuredGuestCall(*core, 0x80017388u, 0x800273F0u, 2u);
}

} // namespace

void registerDisplayFrameOverride(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kDisplayFrame, "CrashBash::DisplayFrame", displayFrameOwned);
}

} // namespace crashbash
