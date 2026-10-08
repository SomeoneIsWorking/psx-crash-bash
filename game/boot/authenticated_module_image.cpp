#include "authenticated_module_image.h"

#include "core.h"
#include "image_content_identity.h"
#include "image_identity.h"
#include "invalidation.h"
#include "lightrec_executor.h"

#include <cstddef>
#include <cstdint>
#include <lucent/content.h>
#include <lucent/log.h>
#include <span>
#include <stdexcept>

namespace crashbash {

bool isModuleImageRead(const AuthenticatedModuleSpec &spec,
                       std::uint32_t lba,
                       std::uint32_t destination,
                       std::uint32_t sectorCount) {
  return lba == spec.discLba && destination == spec.loadAddress && sectorCount == spec.sectorCount;
}

ModuleImageReadResult completeModuleImageRead(Core &core,
                                              const AuthenticatedModuleSpec &spec,
                                              std::uint32_t lba,
                                              std::uint32_t destination,
                                              std::uint32_t sectorCount) {
  if (!validModuleSpec(spec)) {
    throw std::logic_error("Crash Bash loaded-module specification is invalid");
  }
  if (!isModuleImageRead(spec, lba, destination, sectorCount)) {
    return ModuleImageReadResult::Unrelated;
  }
  const std::uint32_t physical = spec.loadAddress & 0x1fffffffu;
  const GuestAddressRange range{physical, static_cast<std::uint32_t>(physical + spec.payloadBytes)};
  runtime::retireAuthenticatedImagesForWrite(core, range);
  const std::span<const std::uint8_t> bytes(core.ram + physical, spec.payloadBytes);
  const auto digest = lucent::content::sha256(std::as_bytes(bytes));
  const auto actualSha = lucent::content::sha256_hex(digest);
  // Read the entry witness only when the manifest recorded one.
  std::uint32_t entry = 0u;
  if (hasEntryWitness(spec)) {
    for (std::uint32_t index = 0; index < sizeof(entry); ++index) {
      entry |= static_cast<std::uint32_t>(bytes[spec.entryPointerOffset + index]) << (index * 8u);
    }
  }
  if (actualSha != spec.sha256 || entry != spec.entry) {
    lucent::error("crashbash-module",
                  "completed {} read failed image authentication: sha256 {} entry 0x{:08X} (expected "
                  "sha256 {} entry 0x{:08X}; this module carries {} content witness)",
                  spec.name,
                  actualSha,
                  entry,
                  spec.sha256,
                  spec.entry,
                  hasEntryWitness(spec) ? "2 (sha256 and a table-of-contents entry word)"
                                        : "1 (sha256 alone; this module carries no entry word)");
    return ModuleImageReadResult::Rejected;
  }
  psx::cpu::notifyExecutableWrite(core, range, psx::cpu::ExecutableWriteSource::ModuleLoad);

  const auto image = core.imageCatalog().activate(spec.name, range, imageContentIdentity(digest));
  auto &execution = *static_cast<runtime::GuestExecution *>(core.gameCtx);
  execution.bindAuthenticatedImage(spec.image, image, range);
  lucent::info("crashbash-module",
               "authenticated {} image generation {} at 0x{:08X}: {} sector(s), {} byte(s), {} — sha256 {}",
               spec.name,
               image.generation,
               spec.loadAddress,
               spec.sectorCount,
               spec.payloadBytes,
               hasEntryWitness(spec)
                   ? "2 content witnesses (sha256 plus the table-of-contents entry word)"
                   : "1 content witness (sha256 alone; this module's header is a name table, not a pointer table)",
               spec.sha256);
  return ModuleImageReadResult::Published;
}

} // namespace crashbash
