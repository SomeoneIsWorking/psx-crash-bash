#include "menu_image.h"

#include "menu_image_identity.h"

namespace crashbash {
namespace {

constexpr AuthenticatedModuleSpec kMenuSpec{
    runtime::GuestImage::Menu,
    "crashbash-usa-menu",
    menu_image::kDiscLba,
    menu_image::kLoadAddress,
    menu_image::kSectorCount,
    menu_image::kPayloadBytes,
    menu_image::kSha256,
    menu_image::kEntryPointerOffset,
    menu_image::kEntry,
};
static_assert(validModuleSpec(kMenuSpec));

} // namespace

const AuthenticatedModuleSpec &menuImageSpec() {
  return kMenuSpec;
}

bool isMenuImageRead(std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount) {
  return isModuleImageRead(kMenuSpec, lba, destination, sectorCount);
}

MenuImageReadResult
completeMenuImageRead(Core &core, std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount) {
  return completeModuleImageRead(core, kMenuSpec, lba, destination, sectorCount);
}

} // namespace crashbash
