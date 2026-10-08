#include "boot_image.h"

#include "boot_image_identity.h"
#include "cd_file_read.h"
#include "core.h"
#include "crashbash_guest.h"
#include "game.h"
#include "guest_execution.h"
#include "native_dispatch.h"
#include "testutil.h"
#include "title_adapter.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <vector>

namespace {

crashbash::TitleAdapter runtime;
std::vector<std::uint8_t> bootBytes;

std::unique_ptr<Game> makeGame() {
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  runtime.registerOverrides(*game);
  return game;
}

crashbash::runtime::GuestExecution &execution(Core &core) {
  return *static_cast<crashbash::runtime::GuestExecution *>(core.gameCtx);
}

void syntheticMenuOwner(Core *) {}

void test_unrelated_read_does_not_publish_and_bad_boot_bytes_refuse() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(!crashbash::isBootImageRead(
      crashbash::boot_image::kDiscLba + 1u, crashbash::boot_image::kLoadAddress, crashbash::boot_image::kSectorCount));
  CHECK(!crashbash::isBootImageRead(
      crashbash::boot_image::kDiscLba, crashbash::boot_image::kLoadAddress + 4u, crashbash::boot_image::kSectorCount));
  CHECK(!crashbash::isBootImageRead(
      crashbash::boot_image::kDiscLba, crashbash::boot_image::kLoadAddress, crashbash::boot_image::kSectorCount - 1u));
  CHECK_EQ(crashbash::completeBootImageRead(core,
                                            crashbash::boot_image::kDiscLba + 1u,
                                            crashbash::boot_image::kLoadAddress,
                                            crashbash::boot_image::kSectorCount),
           crashbash::BootImageReadResult::Unrelated);
  CHECK_EQ(crashbash::completeBootImageRead(core,
                                            crashbash::boot_image::kDiscLba,
                                            crashbash::boot_image::kLoadAddress,
                                            crashbash::boot_image::kSectorCount),
           crashbash::BootImageReadResult::Rejected);
  CHECK_EQ(core.imageCatalog().activeCount(), 0u);
  CHECK(!execution(core).activeKey(crashbash::runtime::GuestImage::Boot, crashbash::boot_image::kEntry));

  constexpr std::uint32_t physicalBoot = crashbash::boot_image::kLoadAddress & 0x1fffffffu;
  const GuestAddressRange bootRange{physicalBoot, physicalBoot + crashbash::boot_image::kPayloadBytes};
  const auto boot = core.imageCatalog().activate("synthetic-boot", bootRange, 1u);
  execution(core).bindAuthenticatedImage(crashbash::runtime::GuestImage::Boot, boot, bootRange);
  constexpr GuestAddressRange nested{0xB32B4u, 0xBB2B4u};
  crashbash::runtime::registerNativeOverride(
      core, crashbash::runtime::GuestImage::Menu, 0x800B4000u, "synthetic-menu", syntheticMenuOwner);
  const auto menu = core.imageCatalog().activate("synthetic-menu", nested, 2u);
  execution(core).bindAuthenticatedImage(crashbash::runtime::GuestImage::Menu, menu, nested);
  CHECK_EQ(core.imageCatalog().activeCount(), 2u);
  CHECK(core.nativeDispatcher().isInstalled({menu, 0x800B4000u}));
  crashbash::retireImagesForCdSectorWrite(core, 0x9F800000u);
  CHECK_EQ(core.imageCatalog().activeCount(), 2u);
  crashbash::retireImagesForCdSectorWrite(core, 0x802B4000u);
  CHECK_EQ(core.imageCatalog().activeCount(), 2u);
  CHECK(execution(core).activeKey(crashbash::runtime::GuestImage::Boot, crashbash::boot_image::kEntry));
  CHECK(core.currentImageIdentity(0x80093244u) == boot);
  CHECK(core.nativeDispatcher().isInstalled({boot, crashbash::guest::kBootLogoUpdate}));
  CHECK(execution(core).activeKey(crashbash::runtime::GuestImage::Menu, nested.begin));
  CHECK(!core.currentImageIdentity(0x800B4000u));
  CHECK(!core.nativeDispatcher().isInstalled({menu, 0x800B4000u}));
  execution(core).unbindImage(crashbash::runtime::GuestImage::Menu);
  CHECK(core.imageCatalog().deactivate(menu));
  CHECK(!core.currentImageIdentity(0x800B4000u));

  const auto resident = runtime.guestProgramImage()->residentText;
  const auto residentImage = core.imageCatalog().activate("synthetic-resident", resident, 3u);
  execution(core).bindAuthenticatedImage(crashbash::runtime::GuestImage::Resident, residentImage, resident);
  CHECK_EQ(core.imageCatalog().activeCount(), 2u);
  crashbash::retireImagesForCdSectorWrite(core, crashbash::guest::kCdFileRead);
  CHECK_EQ(core.imageCatalog().activeCount(), 2u);
  CHECK(core.currentImageIdentity(0x80010000u) == residentImage);
  CHECK(!core.currentImageIdentity(crashbash::guest::kCdFileRead));
  CHECK(!core.nativeDispatcher().isInstalled({residentImage, crashbash::guest::kCdFileRead}));
}

void test_authenticated_boot_publication_and_corrupt_reload() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK_EQ(bootBytes.size(), crashbash::boot_image::kPayloadBytes);
  const GuestAddressRange resident = runtime.guestProgramImage()->residentText;
  const auto residentImage = core.imageCatalog().activate("resident", resident, 1u);
  execution(core).bindAuthenticatedImage(crashbash::runtime::GuestImage::Resident, residentImage, resident);
  const auto physical = crashbash::boot_image::kLoadAddress & 0x1fffffffu;
  std::copy(bootBytes.begin(), bootBytes.end(), core.ram + physical);
  CHECK_EQ(crashbash::completeBootImageRead(core,
                                            crashbash::boot_image::kDiscLba,
                                            crashbash::boot_image::kLoadAddress,
                                            crashbash::boot_image::kSectorCount),
           crashbash::BootImageReadResult::Published);
  const auto first = execution(core).activeKey(crashbash::runtime::GuestImage::Boot, crashbash::boot_image::kEntry);
  CHECK(first.has_value());
  CHECK(core.nativeDispatcher().isInstalled({first->image, crashbash::guest::kBootLogoUpdate}));
  CHECK_EQ(core.imageCatalog().activeCount(), 2u);

  constexpr GuestAddressRange nested{0xB32B4u, 0xBB2B4u};
  const auto menu = core.imageCatalog().activate("nested-menu", nested, 2u);
  execution(core).bindAuthenticatedImage(crashbash::runtime::GuestImage::Menu, menu, nested);
  CHECK_EQ(core.imageCatalog().activeCount(), 3u);
  crashbash::retireImagesForCdSectorWrite(core, 0x9F800000u);
  CHECK_EQ(core.imageCatalog().activeCount(), 3u);

  crashbash::retireImagesForCdSectorWrite(core, 0x802B4000u);
  CHECK_EQ(core.imageCatalog().activeCount(), 3u);
  CHECK(core.nativeDispatcher().isInstalled({residentImage, crashbash::guest::kCdFileRead}));
  CHECK(core.currentImageIdentity(crashbash::boot_image::kLoadAddress) == first->image);
  CHECK(core.currentImageIdentity(0x80093244u) == first->image);
  CHECK(!core.currentImageIdentity(0x800B4000u));
  CHECK(execution(core).activeKey(crashbash::runtime::GuestImage::Menu, nested.begin));
  CHECK(core.nativeDispatcher().isInstalled({first->image, crashbash::guest::kBootLogoUpdate}));
  std::copy(bootBytes.begin(), bootBytes.end(), core.ram + physical);
  core.ram[physical + bootBytes.size() - 1u] ^= 1u;
  CHECK_EQ(crashbash::completeBootImageRead(core,
                                            crashbash::boot_image::kDiscLba,
                                            crashbash::boot_image::kLoadAddress,
                                            crashbash::boot_image::kSectorCount),
           crashbash::BootImageReadResult::Rejected);
  CHECK_EQ(core.imageCatalog().activeCount(), 1u);

  std::copy(bootBytes.begin(), bootBytes.end(), core.ram + physical);
  CHECK_EQ(crashbash::completeBootImageRead(core,
                                            crashbash::boot_image::kDiscLba,
                                            crashbash::boot_image::kLoadAddress,
                                            crashbash::boot_image::kSectorCount),
           crashbash::BootImageReadResult::Published);
  const auto reloaded = execution(core).activeKey(crashbash::runtime::GuestImage::Boot, crashbash::boot_image::kEntry);
  CHECK(reloaded.has_value());
  CHECK(reloaded->image != first->image);
  CHECK(core.nativeDispatcher().isInstalled({reloaded->image, crashbash::guest::kBootLogoUpdate}));
  CHECK_EQ(core.imageCatalog().activeCount(), 2u);
}

} // namespace

int main(int argc, char **argv) {
  RUN(unrelated_read_does_not_publish_and_bad_boot_bytes_refuse);
  if (argc == 2) {
    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    if (!input || input.tellg() != static_cast<std::streamoff>(crashbash::boot_image::kPayloadBytes)) {
      return 2;
    }
    bootBytes.resize(static_cast<std::size_t>(input.tellg()));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char *>(bootBytes.data()), static_cast<std::streamsize>(bootBytes.size()))) {
      return 2;
    }
    RUN(authenticated_boot_publication_and_corrupt_reload);
  } else if (argc != 1) {
    return 2;
  }
  return pt_summary();
}
