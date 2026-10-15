
#include "execution/boot_image_identity.h"
#include "execution/executable_identity.h"
#include "execution/guest_execution.h"
#include "frame/frame_driver.h"
#include "frame/measured_guest_call.h"
#include "game.h"
#include "platform_hle.h"
#include "runtime/session.h"
#include "testutil.h"
#include "title/guest.h"
#include "title_game.h"

#include <fstream>
#include <lucent/log.h>
#include <memory>
#include <vector>

namespace {

std::vector<std::uint8_t> retailBytes;

std::unique_ptr<crashbash::test::TitleGame> makeGame() {
  return crashbash::test::makeTitleGame();
}

crashbash::runtime::GuestExecution &context(Core &core) {
  return crashbash::session(core).execution;
}

void test_overrides_bind_stock_libraries_and_the_card_device() {
  auto game = makeGame();
  CHECK(&game->core().runtime == &game->runtime());
  CHECK(dynamic_cast<crashbash::Session *>(game->game().sessionState.get()) != nullptr);
  CHECK(dynamic_cast<crashbash::FrameDriver *>(game->game().frameDriver.get()) != nullptr);
  CHECK(game->runtime().interpolates());
  game->runtime().registerOverrides(game->game());
  CHECK(game->game().cd.stockLibrary() != nullptr);
  CHECK(game->game().platform_hle.lookup(crashbash::guest::kCdCommand) != nullptr);
  CHECK(game->game().platform_hle.lookup(crashbash::guest::kCdSync) != nullptr);
  CHECK(game->game().platform_hle.lookup(crashbash::guest::kCdSearchFile) != nullptr);
  CHECK_EQ(game->game().platform_hle.vsyncAddress(), crashbash::guest::kVSync.begin);
  using crashbash::runtime::GuestImage;
  const crashbash::runtime::GuestExecution &execution = context(game->core());
  CHECK(execution.registers(GuestImage::Resident, crashbash::guest::kCdFileRead));
  CHECK(execution.registers(GuestImage::Resident, crashbash::guest::kCdLicenseStartup));
  CHECK(execution.registers(GuestImage::Resident, crashbash::guest::kMemoryCardStartup));
  CHECK(execution.registers(GuestImage::Resident, crashbash::guest::kGpuTimeoutArm));
  CHECK(execution.registers(GuestImage::Resident, crashbash::guest::kDisplayFrame));
  CHECK(execution.registers(GuestImage::Resident, crashbash::guest::kModelDraw));
  CHECK(execution.registers(GuestImage::Resident, crashbash::guest::kTextComponentDraw));
  CHECK(execution.registers(GuestImage::Resident, crashbash::guest::kLoadingCardDraw));
  CHECK(execution.registers(GuestImage::Boot, crashbash::guest::kBootLogoUpdate));
  CHECK(!execution.registers(GuestImage::Boot, crashbash::guest::kModelDraw));
  CHECK(game->game().hle.deviceFind("bu") != 0u);
  CHECK_EQ(game->core().imageCatalog().activeCount(), 0u);
  CHECK(game->runtime().nativeState(game->core()) == &context(game->core()).statePort());
}

void test_packet_pool_reads_both_parity_pools_live() {
  auto game = makeGame();
  Core &core = game->core();
  core.mem_w32(0x8005F790u, 0x80100000u);
  core.mem_w32(0x8005F794u, 0x80104000u);
  core.mem_w32(0x8006379Cu, 0x80110000u);
  core.mem_w32(0x800637A0u, 0x80114000u);
  const auto pool = game->runtime().packetPool(core);
  CHECK(pool.has_value());
  CHECK(pool->contains(0x80100010u));
  CHECK(pool->contains(0x80110010u));
  CHECK(!pool->contains(0x80108000u));
  CHECK(pool->readFrom({0x8006379Cu, 4u}));
}

void test_invalid_executable_does_not_publish_or_mutate_guest_state() {
  auto game = makeGame();
  Core &core = game->core();
  core.pc = 0x100u;
  core.r[29] = 0x200u;
  core.mem_w32(0x10000u, 0x12345678u);
  const std::vector<std::uint8_t> invalid(crashbash::identity::kExecutableBytes, 0u);
  const auto result = game->runtime().admitExecutable(core, invalid);
  CHECK(!result);
  CHECK(!result.detail.empty());
  CHECK_EQ(core.pc, 0x100u);
  CHECK_EQ(core.r[29], 0x200u);
  CHECK_EQ(core.mem_r32(0x10000u), 0x12345678u);
  CHECK_EQ(core.imageCatalog().activeCount(), 0u);
  CHECK(!context(core).activeKey(crashbash::runtime::GuestImage::Resident, 0x80010000u));
  CHECK(!game->runtime().admitExecutable(core, {}));
  crashbash::Runtime other;
  CHECK(other.admitExecutable(core, invalid).detail.find("this title's Core") != std::string::npos);
}

void argumentSum(Core *core) {
  core->r[2] = core->r[4] + core->r[5];
}

void test_measured_call_accounts_real_guest_ticks_and_preserves_return_abi() {
  auto game = makeGame();
  Core &core = game->core();
  constexpr std::uint32_t entry = 0x80010000u;
  constexpr GuestAddressRange range{0x10000u, 0x10100u};
  const auto image = core.imageCatalog().activate("synthetic-call", range, 1u);
  context(core).bindAuthenticatedImage(crashbash::runtime::GuestImage::Resident, image, range);
  crashbash::runtime::registerNativeOverride(core, crashbash::runtime::GuestImage::Resident, entry, "sum", argumentSum);
  const auto ticks = game->game().timing.guestInstructionTicks;
  core.r[29] = 0x801fff00u;
  const auto result = crashbash::measuredGuestCall(core, entry, 0x80010040u, 7u, 13u, 29u);
  CHECK_EQ(result, 42u);
  CHECK_EQ(game->game().timing.guestInstructionTicks, ticks + 7u);
  CHECK_EQ(core.r[31], 0x80010040u);
  CHECK_EQ(core.r[29], 0x801fff00u);
}

void test_authenticated_retail_image_loads_and_corruption_preserves_prior_residency() {
  auto game = makeGame();
  Core &core = game->core();
  game->runtime().registerOverrides(game->game());
  const auto loaded = game->runtime().admitExecutable(core, retailBytes);
  CHECK(loaded && loaded.identity);
  if (!loaded || !loaded.identity) {
    return;
  }
  CHECK_EQ(core.pc, crashbash::identity::kEntry);
  CHECK_EQ(core.r[29], 0x801FFFF0u);
  const auto key = context(core).activeKey(crashbash::runtime::GuestImage::Resident, crashbash::guest::kCdFileRead);
  CHECK(key.has_value());
  if (!key) {
    return;
  }
  CHECK(key->image == *loaded.identity);
  CHECK(core.nativeDispatcher().isInstalled(*key));
  const auto priorWord = core.mem_r32(loaded.image.textAddress);
  auto corrupted = retailBytes;
  corrupted.front() ^= 1u;
  CHECK(!game->runtime().admitExecutable(core, corrupted));
  CHECK_EQ(core.mem_r32(loaded.image.textAddress), priorWord);
  CHECK(core.currentImageIdentity(loaded.image.textAddress) == *loaded.identity);
  CHECK(!core.currentImageIdentity(crashbash::boot_image::kLoadAddress));
  CHECK(core.nativeDispatcher().isInstalled(*key));
  const auto reloaded = game->runtime().admitExecutable(core, retailBytes);
  CHECK(reloaded && reloaded.identity);
  if (!reloaded || !reloaded.identity) {
    return;
  }
  CHECK(*reloaded.identity != *loaded.identity);
  CHECK_EQ(core.imageCatalog().activeCount(), 1u);
  CHECK(!core.nativeDispatcher().isInstalled(*key));
  CHECK(core.nativeDispatcher().isInstalled({*reloaded.identity, crashbash::guest::kCdFileRead}));
}

} // namespace

int main(int argc, char **argv) {
  RUN(overrides_bind_stock_libraries_and_the_card_device);
  RUN(packet_pool_reads_both_parity_pools_live);
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
  return psx::test::summary();
}
