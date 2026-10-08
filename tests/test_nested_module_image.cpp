#include "cd_file_read.h"
#include "core.h"
#include "crashbash_guest.h"
#include "dat22510_image_identity.h"
#include "dat28136_image_identity.h"
#include "dat28241_image_identity.h"
#include "dat28272_image_identity.h"
#include "dat28382_image_identity.h"
#include "game.h"
#include "guest_execution.h"
#include "nested_module_image.h"
#include "testutil.h"
#include "title_adapter.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

crashbash::TitleAdapter runtime;

// Provisioned overlays arrive on argv: CTest runs in the build tree, so repo-relative paths do not resolve.
std::string nestedOverlayPath;
std::string compareOverlayPath;

std::unique_ptr<Game> makeGame() {
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  runtime.registerOverrides(*game);
  return game;
}

crashbash::runtime::GuestExecution &execution(Core &core) {
  return *static_cast<crashbash::runtime::GuestExecution *>(core.gameCtx);
}

// DAT28272 installs its behaviour table at 0x8005AA70 first; this is its initializer.
constexpr std::uint32_t kDat28272Initializer = 0x800C3434u;

void copySectors(Core &core, const std::vector<std::uint8_t> &bytes, std::uint32_t destination) {
  const std::uint32_t physical = destination & 0x1fffffffu;
  for (std::size_t offset = 0; offset < bytes.size(); offset += 2048u) {
    crashbash::retireImagesForCdSectorWrite(core, destination + static_cast<std::uint32_t>(offset));
    std::copy_n(bytes.begin() + offset, 2048u, core.ram + physical + offset);
  }
}

std::vector<std::uint8_t> readImage(const std::string &path, std::size_t expectedSize) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input || input.tellg() != static_cast<std::streamsize>(expectedSize)) {
    return {};
  }
  std::vector<std::uint8_t> bytes(expectedSize);
  input.seekg(0);
  if (!input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
    return {};
  }
  return bytes;
}

// Every tracked spec is a distinct image and the nested slot is one address.
void test_every_tracked_spec_is_usable_and_unambiguous() {
  CHECK_EQ(crashbash::NestedModuleImage::specCount(), 5u);
  std::vector<std::uint32_t> lbas;
  for (const crashbash::AuthenticatedModuleSpec &spec : crashbash::NestedModuleImage::specs()) {
    CHECK(crashbash::validModuleSpec(spec));
    CHECK_EQ(spec.loadAddress, 0x800B32B4u);
    lbas.push_back(spec.discLba);
  }
  std::sort(lbas.begin(), lbas.end());
  CHECK(std::adjacent_find(lbas.begin(), lbas.end()) == lbas.end());
  CHECK(crashbash::hasEntryWitness(crashbash::NestedModuleImage::specs().front()) == false);
}

// A read matching no tracked module stays Unrelated; most reads are asset loads.
void test_only_an_exact_module_read_is_offered() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(!crashbash::NestedModuleImage::isNestedModuleRead(crashbash::dat28272_image::kDiscLba + 1u,
                                                          crashbash::dat28272_image::kLoadAddress,
                                                          crashbash::dat28272_image::kSectorCount));
  CHECK(!crashbash::NestedModuleImage::isNestedModuleRead(crashbash::dat28272_image::kDiscLba,
                                                          crashbash::dat28272_image::kLoadAddress + 4u,
                                                          crashbash::dat28272_image::kSectorCount));
  CHECK(!crashbash::NestedModuleImage::isNestedModuleRead(crashbash::dat28272_image::kDiscLba,
                                                          crashbash::dat28272_image::kLoadAddress,
                                                          crashbash::dat28272_image::kSectorCount - 1u));
  CHECK_EQ(crashbash::NestedModuleImage::offer(core,
                                               crashbash::dat28272_image::kDiscLba + 1u,
                                               crashbash::dat28272_image::kLoadAddress,
                                               crashbash::dat28272_image::kSectorCount),
           crashbash::ModuleImageReadResult::Unrelated);
  // An asset-shaped read at the heap is not a module.
  CHECK_EQ(crashbash::NestedModuleImage::offer(core, 20130u, 0x801CFB88u, 49u),
           crashbash::ModuleImageReadResult::Unrelated);
  CHECK_EQ(core.imageCatalog().activeCount(), 0u);
  CHECK(!core.currentImageIdentity(kDat28272Initializer));
}

// Regression: the DAT28272 read must publish an identity, else its first dispatch is refused as ambiguous.
void test_real_nested_module_publication_gives_its_code_an_identity() {
  const std::string overlayPath = nestedOverlayPath;
  const std::vector<std::uint8_t> bytes = readImage(overlayPath, crashbash::dat28272_image::kPayloadBytes);
  if (bytes.empty()) {
    std::fprintf(stderr, "no usable image at %s\n", overlayPath.c_str());
    std::exit(2);
  }
  auto game = makeGame();
  Core &core = game->core;

  CHECK(!core.currentImageIdentity(kDat28272Initializer));

  copySectors(core, bytes, crashbash::dat28272_image::kLoadAddress);
  CHECK_EQ(crashbash::NestedModuleImage::offer(core,
                                               crashbash::dat28272_image::kDiscLba,
                                               crashbash::dat28272_image::kLoadAddress,
                                               crashbash::dat28272_image::kSectorCount),
           crashbash::ModuleImageReadResult::Published);
  CHECK_EQ(core.imageCatalog().activeCount(), 1u);

  const auto identity = core.currentImageIdentity(kDat28272Initializer);
  CHECK(identity.has_value());
  CHECK_EQ(identity->generation, 1u);
  const auto key = execution(core).activeKey(crashbash::runtime::GuestImage::Dat28272, kDat28272Initializer);
  CHECK(key.has_value());
  CHECK(key->image == *identity);
  CHECK(
      core.currentImageIdentity(crashbash::dat28272_image::kLoadAddress + crashbash::dat28272_image::kPayloadBytes - 4u)
          .has_value());
  // One byte past the payload is not this module.
  CHECK(!core.currentImageIdentity(crashbash::dat28272_image::kLoadAddress + crashbash::dat28272_image::kPayloadBytes));
}

// Hermetic: the exact tuple over unwritten RAM is rejected and publishes nothing.
void test_exact_tuple_over_unwritten_ram_is_rejected() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(!core.currentImageIdentity(kDat28272Initializer));
  CHECK_EQ(crashbash::NestedModuleImage::offer(core,
                                               crashbash::dat28272_image::kDiscLba,
                                               crashbash::dat28272_image::kLoadAddress,
                                               crashbash::dat28272_image::kSectorCount),
           crashbash::ModuleImageReadResult::Rejected);
  CHECK_EQ(core.imageCatalog().activeCount(), 0u);
  CHECK(!core.currentImageIdentity(kDat28272Initializer));
  CHECK(!execution(core).activeKey(crashbash::runtime::GuestImage::Dat28272, kDat28272Initializer).has_value());
}

// A spec match with the wrong bytes fails the read and retires the prior keys.
void test_corrupt_nested_bytes_are_rejected_without_disturbing_the_prior_image() {
  const std::vector<std::uint8_t> good = readImage(nestedOverlayPath, crashbash::dat28272_image::kPayloadBytes);
  const std::vector<std::uint8_t> other = readImage(compareOverlayPath, crashbash::dat28136_image::kPayloadBytes);
  if (good.empty() || other.empty()) {
    std::fprintf(stderr, "no usable images at %s / %s\n", nestedOverlayPath.c_str(), compareOverlayPath.c_str());
    std::exit(2);
  }
  auto game = makeGame();
  Core &core = game->core;
  copySectors(core, good, crashbash::dat28272_image::kLoadAddress);
  CHECK_EQ(crashbash::NestedModuleImage::offer(core,
                                               crashbash::dat28272_image::kDiscLba,
                                               crashbash::dat28272_image::kLoadAddress,
                                               crashbash::dat28272_image::kSectorCount),
           crashbash::ModuleImageReadResult::Published);
  const auto first = core.currentImageIdentity(kDat28272Initializer);
  CHECK(first.has_value());

  // DAT28136 is longer than DAT28272; only the overlapping prefix is copied.
  auto mismatched = std::vector<std::uint8_t>(good.size(), 0u);
  CHECK(other.size() > good.size());
  std::copy(other.begin(), other.begin() + static_cast<std::ptrdiff_t>(good.size()), mismatched.begin());
  CHECK(mismatched != good);
  copySectors(core, mismatched, crashbash::dat28272_image::kLoadAddress);
  CHECK_EQ(crashbash::NestedModuleImage::offer(core,
                                               crashbash::dat28272_image::kDiscLba,
                                               crashbash::dat28272_image::kLoadAddress,
                                               crashbash::dat28272_image::kSectorCount),
           crashbash::ModuleImageReadResult::Rejected);
  CHECK(!core.currentImageIdentity(kDat28272Initializer));
  CHECK(!execution(core).activeKey(crashbash::runtime::GuestImage::Dat28272, kDat28272Initializer).has_value());

  // A good reload publishes a fresh generation.
  copySectors(core, good, crashbash::dat28272_image::kLoadAddress);
  CHECK_EQ(crashbash::NestedModuleImage::offer(core,
                                               crashbash::dat28272_image::kDiscLba,
                                               crashbash::dat28272_image::kLoadAddress,
                                               crashbash::dat28272_image::kSectorCount),
           crashbash::ModuleImageReadResult::Published);
  const auto reloaded = core.currentImageIdentity(kDat28272Initializer);
  CHECK(reloaded.has_value());
  CHECK(reloaded->generation != first->generation);
}

} // namespace

int main(int argc, char **argv) {
  RUN(every_tracked_spec_is_usable_and_unambiguous);
  RUN(only_an_exact_module_read_is_offered);
  RUN(exact_tuple_over_unwritten_ram_is_rejected);
  if (argc == 3) {
    nestedOverlayPath = argv[1];
    compareOverlayPath = argv[2];
    RUN(real_nested_module_publication_gives_its_code_an_identity);
    RUN(corrupt_nested_bytes_are_rejected_without_disturbing_the_prior_image);
  } else if (argc != 1) {
    std::fprintf(stderr, "usage: %s [DAT28272.BIN DAT28136.BIN]\n", argv[0]);
    return 2;
  }
  return pt_summary();
}
