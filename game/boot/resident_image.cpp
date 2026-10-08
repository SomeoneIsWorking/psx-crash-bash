#include "resident_image.h"

#include "core.h"
#include "executable_identity.h"

#include <lucent/content.h>

namespace crashbash {

psx::cpu::PsxExeLoadResult loadResidentImage(Core &core, std::span<const std::uint8_t> bytes) {
  if (bytes.size() != identity::kExecutableBytes) {
    return {std::nullopt, {}, "Crash Bash USA executable size does not match the authenticated manifest"};
  }
  if (lucent::content::sha256_hex(lucent::content::sha256(std::as_bytes(bytes))) != identity::kExecutableSha256) {
    return {std::nullopt, {}, "Crash Bash USA executable SHA-256 does not match the authenticated manifest"};
  }
  // What is published is the framework's own authenticated code interval, not the file: the pre-BSS
  // text is what a translated block can belong to.
  return psx::cpu::loadPsxExeImage(core, bytes, "crashbash-usa-resident");
}

} // namespace crashbash