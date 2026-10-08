#include "boot_image.h"
#include "boot_image_identity.h"
#include "cd_file_read.h"
#include "core.h"
#include "crashbash_guest.h"
#include "game.h"
#include "guest_execution.h"
#include "menu_image.h"
#include "menu_image_identity.h"
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
std::vector<std::uint8_t> menuBytes;

std::unique_ptr<Game> makeGame() {
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  runtime.registerOverrides(*game);
  return game;
}

crashbash::runtime::GuestExecution &execution(Core &core) {
  return *static_cast<crashbash::runtime::GuestExecution *>(core.gameCtx);
}

void copyMenuRead(Core &core, const std::vector<std::uint8_t> &bytes) {
  constexpr std::uint32_t physical = crashbash::menu_image::kLoadAddress & 0x1fffffffu;
  for (std::uint32_t sector = 0; sector < crashbash::menu_image::kSectorCount; ++sector) {
    const std::uint32_t offset = sector * 2048u;
    crashbash::retireImagesForCdSectorWrite(core, crashbash::menu_image::kLoadAddress + offset);
    std::copy_n(bytes.begin() + offset, 2048u, core.ram + physical + offset);
  }
}

void test_wrong_tuple_and_unauthenticated_ram_refuse() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(!crashbash::isMenuImageRead(
      crashbash::menu_image::kDiscLba + 1u, crashbash::menu_image::kLoadAddress, crashbash::menu_image::kSectorCount));
  CHECK(!crashbash::isMenuImageRead(
      crashbash::menu_image::kDiscLba, crashbash::menu_image::kLoadAddress + 4u, crashbash::menu_image::kSectorCount));
  CHECK(!crashbash::isMenuImageRead(
      crashbash::menu_image::kDiscLba, crashbash::menu_image::kLoadAddress, crashbash::menu_image::kSectorCount - 1u));
  CHECK_EQ(crashbash::completeMenuImageRead(core,
                                            crashbash::menu_image::kDiscLba + 1u,
                                            crashbash::menu_image::kLoadAddress,
                                            crashbash::menu_image::kSectorCount),
           crashbash::MenuImageReadResult::Unrelated);
  CHECK_EQ(crashbash::completeMenuImageRead(core,
                                            crashbash::menu_image::kDiscLba,
                                            crashbash::menu_image::kLoadAddress,
                                            crashbash::menu_image::kSectorCount),
           crashbash::MenuImageReadResult::Rejected);
  CHECK_EQ(core.imageCatalog().activeCount(), 0u);
  CHECK(!execution(core).activeKey(crashbash::runtime::GuestImage::Menu, crashbash::menu_image::kEntry));
}

void test_real_menu_publication_preserves_boot_fragments_and_reloads() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK_EQ(bootBytes.size(), crashbash::boot_image::kPayloadBytes);
  CHECK_EQ(menuBytes.size(), crashbash::menu_image::kPayloadBytes);
  const std::uint32_t physicalBoot = crashbash::boot_image::kLoadAddress & 0x1fffffffu;
  std::copy(bootBytes.begin(), bootBytes.end(), core.ram + physicalBoot);
  CHECK_EQ(crashbash::completeBootImageRead(core,
                                            crashbash::boot_image::kDiscLba,
                                            crashbash::boot_image::kLoadAddress,
                                            crashbash::boot_image::kSectorCount),
           crashbash::BootImageReadResult::Published);
  const auto boot = execution(core).activeKey(crashbash::runtime::GuestImage::Boot, crashbash::boot_image::kEntry);
  CHECK(boot.has_value());
  copyMenuRead(core, menuBytes);
  CHECK(core.currentImageIdentity(0x80093244u) == boot->image);
  CHECK(!core.currentImageIdentity(crashbash::menu_image::kEntry));
  CHECK(core.nativeDispatcher().isInstalled({boot->image, crashbash::guest::kBootLogoUpdate}));

  CHECK_EQ(crashbash::completeMenuImageRead(core,
                                            crashbash::menu_image::kDiscLba + 1u,
                                            crashbash::menu_image::kLoadAddress,
                                            crashbash::menu_image::kSectorCount),
           crashbash::MenuImageReadResult::Unrelated);
  CHECK(!core.currentImageIdentity(crashbash::menu_image::kEntry));

  auto wrongEntry = crashbash::menuImageSpec();
  wrongEntry.entry += 4u;
  CHECK_EQ(crashbash::completeModuleImageRead(core,
                                              wrongEntry,
                                              crashbash::menu_image::kDiscLba,
                                              crashbash::menu_image::kLoadAddress,
                                              crashbash::menu_image::kSectorCount),
           crashbash::ModuleImageReadResult::Rejected);
  CHECK(core.currentImageIdentity(0x80093244u) == boot->image);
  CHECK(!core.currentImageIdentity(crashbash::menu_image::kEntry));
  CHECK_EQ(crashbash::completeMenuImageRead(core,
                                            crashbash::menu_image::kDiscLba,
                                            crashbash::menu_image::kLoadAddress,
                                            crashbash::menu_image::kSectorCount),
           crashbash::MenuImageReadResult::Published);
  const auto firstMenu = execution(core).activeKey(crashbash::runtime::GuestImage::Menu, crashbash::menu_image::kEntry);
  CHECK(firstMenu.has_value());
  CHECK(core.nativeDispatcher().isInstalled(*firstMenu));
  CHECK_EQ(core.imageCatalog().activeCount(), 2u);
  CHECK(core.currentImageIdentity(0x80093244u) == boot->image);
  CHECK(core.nativeDispatcher().isInstalled({boot->image, crashbash::guest::kBootLogoUpdate}));

  auto corrupt = menuBytes;
  corrupt.back() ^= 1u;
  copyMenuRead(core, corrupt);
  CHECK_EQ(crashbash::completeMenuImageRead(core,
                                            crashbash::menu_image::kDiscLba,
                                            crashbash::menu_image::kLoadAddress,
                                            crashbash::menu_image::kSectorCount),
           crashbash::MenuImageReadResult::Rejected);
  CHECK_EQ(core.imageCatalog().activeCount(), 1u);
  CHECK(!core.currentImageIdentity(crashbash::menu_image::kEntry));
  CHECK(!core.nativeDispatcher().isInstalled(*firstMenu));
  CHECK(core.currentImageIdentity(0x80093244u) == boot->image);

  copyMenuRead(core, menuBytes);
  CHECK_EQ(crashbash::completeMenuImageRead(core,
                                            crashbash::menu_image::kDiscLba,
                                            crashbash::menu_image::kLoadAddress,
                                            crashbash::menu_image::kSectorCount),
           crashbash::MenuImageReadResult::Published);
  const auto reloaded = execution(core).activeKey(crashbash::runtime::GuestImage::Menu, crashbash::menu_image::kEntry);
  CHECK(reloaded.has_value());
  CHECK(reloaded->image != firstMenu->image);
  CHECK(core.nativeDispatcher().isInstalled(*reloaded));
  CHECK(core.currentImageIdentity(0x80093244u) == boot->image);
}

std::vector<std::uint8_t> readImage(const char *path, std::size_t expectedSize) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input || input.tellg() != static_cast<std::streamoff>(expectedSize)) {
    return {};
  }
  std::vector<std::uint8_t> bytes(expectedSize);
  input.seekg(0);
  if (!input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
    return {};
  }
  return bytes;
}

} // namespace

int main(int argc, char **argv) {
  RUN(wrong_tuple_and_unauthenticated_ram_refuse);
  if (argc == 3) {
    bootBytes = readImage(argv[1], crashbash::boot_image::kPayloadBytes);
    menuBytes = readImage(argv[2], crashbash::menu_image::kPayloadBytes);
    if (bootBytes.empty() || menuBytes.empty()) {
      return 2;
    }
    RUN(real_menu_publication_preserves_boot_fragments_and_reloads);
  } else if (argc != 1) {
    return 2;
  }
  return pt_summary();
}
