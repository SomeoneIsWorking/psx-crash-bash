// debug_menu.h — Crash Bash's debug menu: the arena route through the game's own menu flow and the pause page.
#pragma once

#include "title_debug_menu.h"

namespace crashbash::debug {

class DebugMenu final : public psx::game::TitleDebugMenu {
public:
  DebugMenu();
};

} // namespace crashbash::debug
