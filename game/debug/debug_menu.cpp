#include "debug/debug_menu.h"

#include "debug/dev_arena.h"
#include "debug/dev_pause.h"
#include "runtime/session.h"

#include "core.h"

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
  verb({
      .name = "arena",
      .summary = "Enter that arena through the menu",
      .params = {DebugParam::integer("arena", 0, DevArena::kLevelTableBound - 1),
                 DebugParam::word("mode", DebugNames::of(DevArena::kModeNames)).orAbsent()},
      .run =
          [](const DebugCall &call) {
            const std::optional<std::uint32_t> mode =
                call.values.has(1) ? std::optional<std::uint32_t>(call.values.u32(1)) : std::nullopt;
            call.reply = session(call.core).devArena.arm(call.core, call.values.u32(0), mode);
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
