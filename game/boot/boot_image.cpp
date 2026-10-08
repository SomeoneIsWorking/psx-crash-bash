#include "boot_image.h"

#include "boot_image_identity.h"

namespace crashbash {
namespace {

constexpr AuthenticatedModuleSpec kBootSpec{
    runtime::GuestImage::Boot,
    "crashbash-usa-boot",
    boot_image::kDiscLba,
    boot_image::kLoadAddress,
    boot_image::kSectorCount,
    boot_image::kPayloadBytes,
    boot_image::kSha256,
    boot_image::kEntryPointerOffset,
    boot_image::kEntry,
};
static_assert(validModuleSpec(kBootSpec));

} // namespace

bool isBootImageRead(std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount) {
  return isModuleImageRead(kBootSpec, lba, destination, sectorCount);
}

BootImageReadResult
completeBootImageRead(Core &core, std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount) {
  return completeModuleImageRead(core, kBootSpec, lba, destination, sectorCount);
}

} // namespace crashbash
