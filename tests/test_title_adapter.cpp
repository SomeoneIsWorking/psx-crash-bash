#include "title_adapter.h"

#include "boot_image_identity.h"
#include "cd_control.h"
#include "crashbash_frame_driver.h"
#include "crashbash_guest.h"
#include "executable_identity.h"
#include "game.h"
#include "guest_execution.h"
#include "measured_guest_call.h"
#include "platform_hle.h"
#include "testutil.h"

#include <fstream>
#include <lucent/log.h>
#include <memory>
#include <vector>

namespace {

crashbash::TitleAdapter runtime;
std::vector<std::uint8_t> retailBytes;

std::unique_ptr<Game> makeGame() {
  psxport_install_game(runtime);
  return std::make_unique<Game>();
}

crashbash::runtime::GuestExecution &context(Core &core) {
  return *static_cast<crashbash::runtime::GuestExecution *>(core.gameCtx);
}

void test_native_composition_preserves_picture_device_and_boot_contracts() {
  auto game = makeGame();
  CHECK(game->core.runtime == &runtime);
  CHECK(game->core.gameCtx != nullptr);
  CHECK(game->core.cfg == nullptr);
  CHECK(game->core.hooks == nullptr);
  CHECK(dynamic_cast<crashbash::CrashBashFrameDriver *>(game->frameDriver.get()) != nullptr);
  CHECK(runtime.guestVramIsPicture(*game));
  CHECK_EQ(runtime.guestProgramImage()->gameMainEntry, crashbash::guest::kGameMain);
  CHECK_EQ(runtime.guestProgramImage()->crt0Entry, 0x8002E7B0u);
  CHECK_EQ(runtime.guestProgramImage()->residentText.begin, 0x10000u);
  CHECK_EQ(runtime.guestProgramImage()->residentText.end, 0x6E9F0u);
  CHECK_EQ(runtime.platformHlePlan()->vsyncAddress, crashbash::guest::kVSync.begin);
  CHECK_EQ(runtime.platformHlePlan()->cdCommandAddress, crashbash::guest::kCdCommand);
  CHECK_EQ(runtime.platformHlePlan()->cdSyncAddress, crashbash::guest::kCdSync);
  CHECK_EQ(runtime.platformHlePlan()->cdSearchFileAddress, crashbash::guest::kCdSearchFile);
  runtime.registerOverrides(*game);
  game->platform_hle.initBuiltins();
  CHECK(game->platform_hle.lookup(crashbash::guest::kCdCommand) == cd_command_stock_sync);
  CHECK(game->platform_hle.lookup(crashbash::guest::kCdSync) == cd_sync_stock_sync);
  CHECK(game->platform_hle.lookup(crashbash::guest::kCdSearchFile) == cd_searchfile_stock_sync);
  CHECK_EQ(context(game->core).registeredOverrideCount(), 35u);
  CHECK(game->hle.deviceFind("bu") != 0u);
  CHECK_EQ(game->core.imageCatalog().activeCount(), 0u);
}

void test_invalid_executable_does_not_publish_or_mutate_guest_state() {
  auto game = makeGame();
  Core &core = game->core;
  core.pc = 0x100u;
  core.r[29] = 0x200u;
  core.mem_w32(0x10000u, 0x12345678u);
  const std::vector<std::uint8_t> invalid(crashbash::identity::kExecutableBytes, 0u);
  const auto result = runtime.loadExecutable(core, invalid);
  CHECK(!result);
  CHECK(result.detail.find("SHA-256") != std::string::npos);
  CHECK_EQ(core.pc, 0x100u);
  CHECK_EQ(core.r[29], 0x200u);
  CHECK_EQ(core.mem_r32(0x10000u), 0x12345678u);
  CHECK_EQ(core.imageCatalog().activeCount(), 0u);
  CHECK(!context(core).activeKey(crashbash::runtime::GuestImage::Resident, 0x80010000u));
  CHECK(!runtime.loadExecutable(core, {}));
  crashbash::TitleAdapter other;
  CHECK(other.loadExecutable(core, invalid).detail.find("Core context") != std::string::npos);
}

void argumentSum(Core *core) {
  core->r[2] = core->r[4] + core->r[5];
}

void test_measured_call_accounts_real_guest_ticks_and_preserves_return_abi() {
  auto game = makeGame();
  Core &core = game->core;
  constexpr std::uint32_t entry = 0x80010000u;
  constexpr GuestAddressRange range{0x10000u, 0x10100u};
  const auto image = core.imageCatalog().activate("synthetic-call", range, 1u);
  context(core).bindAuthenticatedImage(crashbash::runtime::GuestImage::Resident, image, range);
  crashbash::runtime::registerNativeOverride(core, crashbash::runtime::GuestImage::Resident, entry, "sum", argumentSum);
  const auto ticks = game->timing.guestInstructionTicks;
  core.r[29] = 0x801fff00u;
  const auto result = crashbash::measuredGuestCall(core, entry, 0x80010040u, 7u, 13u, 29u);
  CHECK_EQ(result, 42u);
  CHECK_EQ(game->timing.guestInstructionTicks, ticks + 7u);
  CHECK_EQ(core.r[31], 0x80010040u);
  CHECK_EQ(core.r[29], 0x801fff00u);
}

void test_authenticated_retail_image_loads_and_corruption_preserves_prior_residency() {
  auto game = makeGame();
  Core &core = game->core;
  runtime.registerOverrides(*game);
  const auto loaded = runtime.loadExecutable(core, retailBytes);
  CHECK(loaded);
  CHECK_EQ(core.pc, runtime.guestProgramImage()->crt0Entry);
  CHECK_EQ(core.r[29], 0x801FFFF0u);
  const auto key = context(core).activeKey(crashbash::runtime::GuestImage::Resident, crashbash::guest::kCdFileRead);
  CHECK(key.has_value());
  CHECK(key->image == *loaded.identity);
  CHECK(core.nativeDispatcher().isInstalled(*key));
  const auto priorWord = core.mem_r32(loaded.image.textAddress);
  auto corrupted = retailBytes;
  corrupted.back() ^= 1u;
  CHECK(!runtime.loadExecutable(core, corrupted));
  CHECK_EQ(core.mem_r32(loaded.image.textAddress), priorWord);
  CHECK(core.currentImageIdentity(loaded.image.textAddress) == *loaded.identity);
  CHECK(!core.currentImageIdentity(crashbash::boot_image::kLoadAddress));
  CHECK(core.nativeDispatcher().isInstalled(*key));
  const auto reloaded = runtime.loadExecutable(core, retailBytes);
  CHECK(reloaded);
  CHECK(*reloaded.identity != *loaded.identity);
  CHECK_EQ(core.imageCatalog().activeCount(), 1u);
  CHECK(!core.nativeDispatcher().isInstalled(*key));
  CHECK(core.nativeDispatcher().isInstalled({*reloaded.identity, crashbash::guest::kCdFileRead}));
}

} // namespace

int main(int argc, char **argv) {
  RUN(native_composition_preserves_picture_device_and_boot_contracts);
  RUN(invalid_executable_does_not_publish_or_mutate_guest_state);
  RUN(measured_call_accounts_real_guest_ticks_and_preserves_return_abi);
  // Retail qualification runs only with a supplied executable; the asset-free CTest never fakes the fingerprint.
  if (argc == 2) {
    std::ifstream stream(argv[1], std::ios::binary | std::ios::ate);
    if (!stream || stream.tellg() < 0 || stream.tellg() > static_cast<std::streamoff>(psx::cpu::kPsxExeMaxBytes)) {
      lucent::error("test", "cannot read bounded retail executable input");
      return 2;
    }
    retailBytes.resize(static_cast<std::size_t>(stream.tellg()));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char *>(retailBytes.data()), static_cast<std::streamsize>(retailBytes.size()))) {
      lucent::error("test", "retail executable input read failed");
      return 2;
    }
    RUN(authenticated_retail_image_loads_and_corruption_preserves_prior_residency);
  } else if (argc != 1) {
    return 2;
  }
  return pt_summary();
}
