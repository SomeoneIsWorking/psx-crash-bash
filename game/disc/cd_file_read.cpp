#include "cd_file_read.h"

#include "boot_image.h"
#include "core.h"
#include "crashbash_guest.h"
#include "disc.h"
#include "game.h"
#include "guest_execution.h"
#include "menu_image.h"
#include "nested_module_image.h"

#include <array>
#include <cstdint>
#include <lucent/log.h>
#include <optional>

namespace crashbash {
namespace {

constexpr std::uint32_t kSectorBytes = 2048u;

} // namespace

void retireImagesForCdSectorWrite(Core &core, std::uint32_t destination) {
  if (const auto mapped = core.mappedMainRamRange(destination, kSectorBytes)) {
    runtime::retireAuthenticatedImagesForWrite(core, *mapped);
    return;
  }
  // The sector may cross a mirrored-RAM boundary; map per byte so I/O or scratchpad never retires a RAM image.
  std::optional<GuestAddressRange> run;
  const auto flush = [&] {
    if (run) {
      runtime::retireAuthenticatedImagesForWrite(core, *run);
      run.reset();
    }
  };
  for (std::uint32_t byte = 0; byte < kSectorBytes; ++byte) {
    const auto mapped = core.mappedMainRamRange(destination + byte, 1u);
    if (!mapped) {
      flush();
      continue;
    }
    const auto offset = mapped->begin;
    if (!run || run->end != offset) {
      flush();
      run = GuestAddressRange{offset, offset + 1u};
    } else {
      ++run->end;
    }
  }
  flush();
}

namespace {

void cdFileReadOwned(Core *core) {
  // Retail 0x80027790 starts an async sector read; here it completes synchronously from the disc.
  const std::uint32_t descriptor = core->r[4];
  const std::uint32_t descriptorOffset = core->r[5];
  const std::uint32_t destination = core->r[6];
  const std::uint32_t sectorCount = core->r[7];
  const std::uint32_t lba = core->mem_r32(descriptor) + descriptorOffset + core->mem_r32(guest::kCdBaseLba);
  std::array<std::uint8_t, kSectorBytes> sector{};

  for (std::uint32_t index = 0; index < sectorCount; ++index) {
    if (!disc_read_sector(&core->game->disc, lba + index, sector.data())) {
      core->mem_w32(guest::kCdReadActive, 0u);
      core->r[2] = 0xFFFFFFFFu;
      lucent::error("crashbash-cd", "native file read failed at LBA {} ({}/{})", lba + index, index, sectorCount);
      return;
    }
    const std::uint32_t sectorDestination = destination + index * kSectorBytes;
    retireImagesForCdSectorWrite(*core, sectorDestination);
    for (std::uint32_t byte = 0; byte < kSectorBytes; ++byte) {
      core->mem_w8(sectorDestination + byte, sector[byte]);
    }
  }

  if (completeBootImageRead(*core, lba, destination, sectorCount) == BootImageReadResult::Rejected ||
      completeMenuImageRead(*core, lba, destination, sectorCount) == MenuImageReadResult::Rejected ||
      NestedModuleImage::offer(*core, lba, destination, sectorCount) == ModuleImageReadResult::Rejected) {
    core->mem_w32(guest::kCdReadActive, 0u);
    core->r[2] = 0xFFFFFFFFu;
    return;
  }

  core->mem_w32(guest::kCdReadActive, 0u);
  core->r[2] = 0u;
  lucent::debug("crashbash-cd",
                "native file read completed: {} sector(s) from LBA {} to 0x{:08X}",
                sectorCount,
                lba,
                destination);
}

constexpr std::uint32_t kLoadPumpDrainLimit = 1024u;
// Return address of the pump's per-frame call site (0x8001039C); the other seven callers loop until idle.
constexpr std::uint32_t kPerFramePumpReturn = 0x800103A4u;

// The pump's three queue words; FUN_80012ffc (pending test) is their OR.
struct LoadQueue {
  std::uint32_t cooldown;
  std::uint32_t readActive;
  std::uint32_t queueHead;
};

LoadQueue readLoadQueue(Core &core) {
  return {core.mem_r32(guest::kLoadCooldownWord),
          core.mem_r32(guest::kLoadReadActiveWord),
          core.mem_r32(guest::kLoadQueueHeadWord)};
}

bool loadQueueIdle(const LoadQueue &queue) {
  return queue.cooldown == 0u && queue.readActive == 0u && queue.queueHead == 0u;
}

void loadPumpOwned(Core *core) {
  // One pump step per call at retail's seven loop sites; only the per-frame site repeats until idle, so
  // the loop helpers retail pairs with the pump are not called: they would publish an unasked-for completion.
  // The queue words are read directly because the original-call seam exists only at installed owners.
  // A step that changes nothing means the start gate refused, so stop and leave it to the next frame.
  const std::uint32_t continuation = core->r[31];
  const bool perFrame = continuation == kPerFramePumpReturn;
  LoadQueue before = readLoadQueue(*core);
  for (std::uint32_t step = 0; step < kLoadPumpDrainLimit; ++step) {
    const bool moreSteps = perFrame && !loadQueueIdle(before);
    core->r[31] = continuation;
    runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kLoadPump);
    const LoadQueue after = readLoadQueue(*core);
    if (!moreSteps) {
      break;
    }
    if (loadQueueIdle(after)) {
      break;
    }
    if (before.cooldown == after.cooldown && before.readActive == after.readActive &&
        before.queueHead == after.queueHead) {
      lucent::debug("crashbash-cd",
                    "load pump stalled with queue head 0x{:08X} cooldown {}; deferring to retail pacing",
                    after.queueHead,
                    after.cooldown);
      break;
    }
    before = after;
  }
  core->r[31] = continuation;
  core->r[2] = 0u;
}

} // namespace

void registerCdFileReadOverride(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kCdFileRead, "CrashBash::CdFileRead", cdFileReadOwned);
}

void registerLoadPumpDrainOverride(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kLoadPump, "CrashBash::LoadPumpDrain", loadPumpOwned);
}

} // namespace crashbash
