#include "cd_license_startup.h"

#include "core.h"
#include "crashbash_guest.h"
#include "disc.h"
#include "game.h"
#include "guest_execution.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>
#include <string_view>

namespace crashbash {
namespace {

constexpr std::uint32_t kPassedState = 0u;

struct DiscFileFact {
  std::string_view path;
  std::uint32_t lba;
  std::uint32_t size;
};

constexpr DiscFileFact kDiscFiles[] = {
    {"SCUS_945.70", guest::kDiscExecutableLba, guest::kDiscExecutableSize},
    {"SYSTEM.CNF", guest::kDiscSystemCnfLba, guest::kDiscSystemCnfSize},
    {"CRASHBSH/CRASHBSH.DAT", guest::kDiscDataLba, guest::kDiscDataSize},
};

constexpr std::array<std::uint8_t, 69> kSystemCnf = {
    'B', 'O',  'O',  'T', ' ', '=', ' ', 'c', 'd', 'r',  'o',  'm',  ':', '\\', 'S',  'C', 'U', 'S',
    '_', '9',  '4',  '5', '.', '7', '0', ';', '1', '\t', '\r', '\n', 'T', 'C',  'B',  ' ', '=', ' ',
    '4', '\r', '\n', 'E', 'V', 'E', 'N', 'T', ' ', '=',  ' ',  '1',  '6', '\r', '\n', 'S', 'T', 'A',
    'C', 'K',  ' ',  '=', ' ', '8', '0', '1', 'F', 'F',  'F',  '0',  '0', '\r', '\n',
};

bool hasMeasuredDiscLayout(DiscState &disc) {
  if (!disc_open(&disc) || disc.track_count != 1u) {
    return false;
  }
  const DiscTrackInfo &track = disc.tracks[0];
  if (track.number != 1u || track.lba != 0 || track.sectors != guest::kDiscTrackSectors || track.pregap != 150 ||
      track.pregap_dv != 0 || track.postgap != 0) {
    return false;
  }
  for (const DiscFileFact &fact : kDiscFiles) {
    std::uint32_t lba = 0;
    std::uint32_t size = 0;
    if (!disc_find_file(&disc, fact.path.data(), &lba, &size) || lba != fact.lba || size != fact.size) {
      return false;
    }
  }
  std::array<std::uint8_t, 2048> sector{};
  if (!disc_read_sector(&disc, kDiscFiles[1].lba, sector.data())) {
    return false;
  }
  return std::equal(kSystemCnf.begin(), kSystemCnf.end(), sector.begin());
}

void cdLicenseStartupOwned(Core *core) {
  // Retail 0x8002D4F4 is a 20-state controller; state 16 calls 0x8002E0F0, the copy-protection failure screen
  // (BIOS B0:38), while the authentic path completes Pause in state 18 and returns to idle.
  if (core->mem_r32(guest::kCdLicenseState) == kPassedState) {
    core->r[2] = kPassedState;
    return;
  }

  DiscState &disc = core->game->disc;
  // The launcher already hash-verified the executable and modules; bind that identity to the opened medium,
  // whose mismatch is a host refusal, never the failure renderer.
  if (!hasMeasuredDiscLayout(disc)) {
    lucent::error("crashbash-cd", "license startup rejected a disc that does not match SCUS-94570 layout");
    std::abort();
  }

  core->mem_w32(guest::kCdLicenseState, kPassedState);
  core->r[2] = kPassedState;
  lucent::info("crashbash-cd", "license startup accepted measured SCUS-94570 disc identity");
}

} // namespace

void registerCdLicenseStartupOverride(Core &core) {
  runtime::registerNativeOverride(core,
                                  runtime::GuestImage::Resident,
                                  guest::kCdLicenseStartup,
                                  "CrashBash::CdLicenseStartup",
                                  cdLicenseStartupOwned);
}

} // namespace crashbash
