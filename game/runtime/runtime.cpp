#include "runtime/runtime.h"

#include "boot/boot_logo_skip.h"
#include "boot/boot_object_callbacks.h"
#include "boot/boot_prefix.h"
#include "boot/memory_card_startup.h"
#include "core.h"
#include "crt0_boot.h"
#include "disc/cd_file_read.h"
#include "disc/cd_license_startup.h"
#include "disc/cd_startup.h"
#include "disc/loading_card_skip.h"
#include "execution/boot_image_identity.h"
#include "execution/guest_execution.h"
#include "execution/image_content_identity.h"
#include "frame/display_frame.h"
#include "frame/frame_driver.h"
#include "frame/gpu_timeout.h"
#include "game.h"
#include "gameplay/polar_push_contact.h"
#include "guest_packet_pool_windows.h"
#include "image_identity.h"
#include "memcard.h"
#include "psx_exe_image.h"
#include "render/component_incarnation.h"
#include "render/model_face_producer.h"
#include "render/ordering_table_slots.h"
#include "render/state_renders.h"
#include "render/ui_producer.h"
#include "runtime/session.h"
#include "title/guest.h"

#include <lucent/content.h>
#include <lucent/log.h>
#include <stdexcept>
#include <string>

namespace crashbash {
namespace {

constexpr psx::game::Crt0Image kCrt0{.entry = identity::kEntry};

// Packet pool bounds published by the renderer setup (0x800274FC, 0x800276C4): {base, end} per parity.
constexpr psx::gpu::GuestPacketPoolWindows kPacketPool{
    .representation = psx::gpu::GuestPacketPoolWindows::Representation::LiveBaseEndPointers,
    .basePointer = {0x8005F790u, 0x8006379Cu},
    .endPointer = {0x8005F794u, 0x800637A0u},
};

// The stock libcd command, sync and file search; reads go through the title's own CdFileRead owner.
constexpr psx::cd::StockLibcd::Entries kLibcd{
    .command = guest::kCdCommand,
    .sync = guest::kCdSync,
    .searchFile = guest::kCdSearchFile,
};

} // namespace

Runtime::Runtime() : libcd_(kLibcd, {}, 0), vsync_({.entry = guest::kVSync.begin}) {}

psx::cpu::PsxExeLoadResult Runtime::admitExecutable(Core &core, std::span<const std::uint8_t> bytes) {
  if (&core.runtime != this) {
    return {std::nullopt, {}, "Crash Bash executable admission requires this title's Core"};
  }
  runtime::GuestExecution &execution = session(core).execution;
  // The host authenticated these bytes against the catalog identity before the session admitted them.
  auto result = psx::cpu::loadPsxExeImage(core, bytes, "crashbash-usa-resident");
  if (result) {
    // The header text size includes BSS and heap tail; only the pre-BSS interval is code.
    runtime::retireAuthenticatedImagesForWrite(core, result.image.physicalText);
    if (!result.identity || !core.imageCatalog().deactivate(*result.identity)) {
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

void Runtime::startProgram(Core &core) {
  psx::game::Crt0Boot{kCrt0}.run(core);
  lucent::info("boot", "executing finite Crash Bash boot prefix; native FrameDriver owns repetition");
  runBootPrefix(core);
}

void Runtime::registerOverrides(Game &game) {
  libcd_.bind(game);
  vsync_.bind(game);
  // libmcrd walks the BIOS device table itself; its completion wait never ends without the device install.
  game.memcard.install();
  Core &core = game.core;
  registerCdFileReadOverride(core);
  registerLoadPumpDrainOverride(core);
  registerCdLicenseStartupOverride(core);
  registerCdStartupOverride(core);
  registerMemoryCardStartupOverride(core);
  registerGpuTimeoutOverrides(core);
  registerDisplayFrameOverride(core);
  registerBootObjectCallbackOverrides(core);
  registerBootLogoSkipOverride(core);
  registerLoadingCardSkipOverride(core);
  polar::registerPolarPushContactOverride(core);
  render::registerModelFaceProducer(core);
  render::registerComponentIncarnationOwners(core);
  render::registerUiProducers(core);
  render::nameOrderingTables(core);
  render::registerStateRenders(core, session(core).stateProducers);
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &game) {
  return std::make_unique<Session>(game.core);
}

std::unique_ptr<psx::game::FrameDriver> Runtime::createFrameDriver(Game &game) {
  return std::make_unique<FrameDriver>(game);
}

std::optional<psx::gpu::PacketPoolExtent> Runtime::packetPool(Core &core) const {
  return kPacketPool.extent(core);
}

const psx::present::StateProducer *Runtime::stateProducer(Core &core, std::uint32_t producer) const {
  return session(core).stateProducers.find(producer);
}

bool Runtime::sealedFrameIsCut(Core &core) const {
  return session(core).frameCut.isCut();
}

const GuestWidescreenProjection *Runtime::guestWidescreenProjection() const {
  return &aspectPolicy_;
}

bool Runtime::titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) {
  Session &title = session(core);
  if (title.devArena.handle(core, input, reply)) {
    return true;
  }
  std::string text;
  if (!title.devPause.handle(core, input, text)) {
    return false;
  }
  std::fprintf(reply, "%s\n", text.c_str());
  return true;
}

psx::state::NativeStatePort *Runtime::nativeState(Core &core) const {
  return &session(core).execution.statePort();
}

void Runtime::reportRun(Core &core) const {
  runtime::reportGuestCallTurnCap(core);
}

} // namespace crashbash
