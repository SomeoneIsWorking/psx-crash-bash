#include "title_adapter.h"

#include "boot_image_identity.h"
#include "core.h"
#include "crashbash_boot.h"
#include "crashbash_frame_driver.h"
#include "crashbash_guest.h"
#include "executable_identity.h"
#include "game.h"
#include "guest_execution.h"
#include "image_content_identity.h"
#include "image_identity.h"
#include "memcard.h"
#include "native_owner_set.h"
#include "platform_hle.h"
#include "resident_image.h"

#include <lucent/content.h>
#include <lucent/log.h>
#include <stdexcept>

namespace crashbash {
namespace {

// Recovered SCUS_945.70 CRT0 group; psxport crt0_audit re-checks it against the guest bytes at boot.
constexpr std::uint32_t kBssBegin = 0x8006E9F0u;
constexpr GuestAddressRange kResidentCodeRange{identity::kTextAddress & 0x1fffffffu, kBssBegin & 0x1fffffffu};
static_assert(kResidentCodeRange.begin < kResidentCodeRange.end);
static_assert(kResidentCodeRange.end <= (identity::kTextAddress & 0x1fffffffu) + identity::kTextBytes);

constexpr GuestProgramImage kProgramImage{
    .bss = {kBssBegin, boot_image::kLoadAddress},
    .stackTopWordAddress = 0x8002E860u,
    .stackReserveWordAddress = 0x8006D8B4u,
    .heapBase = boot_image::kLoadAddress,
    .globalPointer = 0x8006E9ECu,
    .libcInitEntry = 0x8003ACCCu,
    .gameMainEntry = guest::kGameMain,
    .crt0Entry = identity::kEntry,
    .residentText = kResidentCodeRange,
    .stackBias = {true, 0},
};

constexpr PlatformHlePlan kPlatformPlan{
    .cdCommandAddress = guest::kCdCommand,
    .cdSyncAddress = guest::kCdSync,
    .cdSearchFileAddress = guest::kCdSearchFile,
    .vsyncAddress = guest::kVSync.begin,
    .windowLo = {guest::kVSync.begin, guest::kCdInitHandshake},
    .windowHi = {guest::kVSync.end, guest::kCdCommand + 4u},
};

// Packet pool bounds published by the renderer setup: {base, end} at 0x8005F790/94 and 0x8006379C/A0 per parity.
constexpr GuestPacketPoolWindows kPacketPoolWindows{
    .representation = GuestPacketPoolWindows::Representation::LiveBaseEndPointers,
    .basePointer = {0x8005F790u, 0x8006379Cu},
    .endPointer = {0x8005F794u, 0x800637A0u},
};

} // namespace

psx::cpu::PsxExeLoadResult TitleAdapter::loadExecutable(Core &core, std::span<const std::uint8_t> bytes) {
  if (core.runtime != this || !core.gameCtx) {
    return {std::nullopt, {}, "Crash Bash executable load requires this title's Core context"};
  }
  auto &execution = *static_cast<runtime::GuestExecution *>(core.gameCtx);
  auto result = loadResidentImage(core, bytes);
  if (result) {
    // The header text size includes BSS and heap tail; only the pre-BSS interval is code.
    runtime::retireAuthenticatedImagesForWrite(core, result.image.physicalText);
    if (!core.imageCatalog().deactivate(*result.identity)) {
      throw std::logic_error("Crash Bash resident loader lost its freshly published image");
    }
    const auto digest = lucent::content::sha256(std::as_bytes(bytes));
    const auto image =
        core.imageCatalog().activate("crashbash-usa-resident-code", kResidentCodeRange, imageContentIdentity(digest));
    result.identity = image;
    execution.bindAuthenticatedImage(runtime::GuestImage::Resident, image, kResidentCodeRange);
  }
  return result;
}

const GuestProgramImage *TitleAdapter::guestProgramImage() const {
  return &kProgramImage;
}
const PlatformHlePlan *TitleAdapter::platformHlePlan() const {
  return &kPlatformPlan;
}

// The renderer allocates its pools from the guest heap (0x800274FC, 0x800276C4), so the windows are
// declared as live pointer pairs; the producer census attributes pool packets to their submitters through them.
const GuestPacketPoolWindows *TitleAdapter::guestPacketPoolWindows() const {
  return &kPacketPoolWindows;
}

const char *TitleAdapter::discEnvVar() const {
  return "PSXPORT_CRASHBASH_DISC";
}

psx::state::NativeStatePort *TitleAdapter::nativeState(Core &core) const {
  if (core.runtime != this || !core.gameCtx) {
    return nullptr;
  }
  return &static_cast<runtime::GuestExecution *>(core.gameCtx)->statePort();
}

void *TitleAdapter::createContext(Core &core) {
  return new runtime::GuestExecution(core);
}
void TitleAdapter::destroyContext(void *context) {
  delete static_cast<runtime::GuestExecution *>(context);
}

void TitleAdapter::registerOverrides(Game &game) {
  // libmcrd walks the BIOS device table itself; its completion wait never ends without the device install.
  card_overrides_init(&game);
  registerNativeOwners(game.core);
}

void TitleAdapter::bootInit(Core &core) {
  lucent::info("boot", "executing finite Crash Bash boot prefix; native FrameDriver owns repetition");
  runBootPrefix(core);
}

RenderCapabilities TitleAdapter::renderCapabilities() const {
  // The picture is the guest's GP0 output replayed from the frame record; producers key it for 60 fps.
  return RenderCapabilities{
      .defaultPath = RenderPath::Record,
      .nativeRenderPath = false,
      .temporalInterpolation = true,
  };
}

bool TitleAdapter::guestVramIsPicture(const Game &) const {
  return true;
}

const GuestWidescreenProjection *TitleAdapter::guestWidescreenProjection() const {
  return &aspectPolicy_;
}

bool TitleAdapter::sealedFrameIsCut(Core &core) const {
  return frameDriver(core).frameCut().isCut();
}

bool TitleAdapter::controlCommand(Core &core, const char *cmd, const char *line, FILE *out) {
  // Only the developer `arena` command lives here.
  return frameDriver(core).devArena().handle(core, cmd, line, out);
}

std::unique_ptr<FrameDriver> TitleAdapter::createFrameDriver(Game &game) {
  return std::make_unique<CrashBashFrameDriver>(game);
}

} // namespace crashbash
