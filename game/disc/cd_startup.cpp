#include "cd_startup.h"

#include "core.h"
#include "crashbash_guest.h"
#include "disc.h"
#include "game.h"
#include "guest_execution.h"

#include <lucent/log.h>

namespace crashbash {
namespace {

void cdDriveReadyOwned(Core *core) {
  // Retail 0x800349AC waits in VSync(30) retries for GetTN readiness; a parsed non-empty CHD TOC is the host's proof.
  DiscState &disc = core->game->disc;
  core->r[2] = disc_open(&disc) && disc.track_count != 0u ? 2u : 5u;
}

void cdInitHandshakeOwned(Core *core) {
  // Retail 0x80034B8C initializes the controller; there is none here and commands are synchronous, so report ready.
  core->r[2] = 1u;
}

} // namespace

void registerCdStartupOverride(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kCdDriveReady, "CrashBash::CdDriveReady", cdDriveReadyOwned);
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kCdInitHandshake, "CrashBash::CdInitHandshake", cdInitHandshakeOwned);
}

} // namespace crashbash
