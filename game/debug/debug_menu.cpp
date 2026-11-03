#include "debug/debug_menu.h"

#include "debug/dev_arena.h"
#include "debug/dev_pause.h"
#include "runtime/session.h"

#include "core.h"

#include <lucent/log.h>

#include <vector>

namespace crashbash::debug {

DebugMenu::DebugMenu() {
  using psx::game::DebugCall;
  using psx::game::DebugNames;
  using psx::game::DebugParam;
  section("Arena");
  verb({
      .name = "arenas",
      .summary = "List the menu's arena table",
      .run =
          [](const DebugCall &call) {
            call.reply = DevArena::list(call.core);
          },
      .inMenu = false,
  });
  warp({
      .levels = {"arena", "mode"},
      .places =
          [](Core &) {
            std::vector<psx::game::WarpPlace> arenas;
            for (std::uint32_t arena = 0; arena < DevArena::kLevelTableBound; ++arena) {
              psx::game::WarpPlace place{.name = lucent::format("arena {}", arena)};
              for (std::uint32_t mode = 0; mode < DevArena::kModeNames.size(); ++mode) {
                place.places.push_back({
                    .name = DevArena::kModeNames[mode],
                    .arrive =
                        [arena, mode](Core &core) {
                          return session(core).devArena.arm(core, arena, mode);
                        },
                });
              }
              arenas.push_back(std::move(place));
            }
            return arenas;
          },
  });
  section("Pause Page");
  verb({
      .name = "pause",
      .summary = "Open or walk the pause page",
      .params = {DebugParam::word("action", DebugNames::of(DevPause::kActionNames))},
      .run =
          [](const DebugCall &call) {
            call.reply = session(call.core).devPause.arm(call.core, call.values.u32(0));
          },
  });
}

} // namespace crashbash::debug
