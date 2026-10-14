#include "crashbash_catalog.h"

#include "executable_identity.h"
#include "single_title_catalog.h"
#include "title_adapter.h"

namespace crashbash {
namespace {

constexpr psx::host::TitleIdentity kTitleIdentity{
    .displayName = "Crash Bash",
    .serial = "SCUS_945.70",
    .slug = "crashbash",
    .fileSize = identity::kExecutableBytes,
    .sha256 = identity::kExecutableSha256,
    .entry = identity::kEntry,
    .globalPointer = identity::kGlobalPointer,
    .textAddress = identity::kTextAddress,
    .textSize = identity::kTextBytes,
    .stackAddress = identity::kStackAddress,
    .stackOffset = identity::kStackOffset,
    .discEnvVar = "PSXPORT_CRASHBASH_DISC",
    .discExecutablePath = "SCUS_945.70",
};

} // namespace

std::unique_ptr<psx::host::TitleCatalog> createCatalog() {
  return std::make_unique<psx::host::SingleTitleCatalog>(
      kTitleIdentity, "scratch/bin", std::make_unique<TitleAdapter>());
}

} // namespace crashbash
