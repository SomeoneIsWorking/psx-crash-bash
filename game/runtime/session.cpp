#include "runtime/session.h"

#include "core.h"
#include "game.h"

#include <cstdlib>
#include <lucent/log.h>

namespace crashbash {

Session &session(Core &core) {
  if (core.game == nullptr || !core.game->sessionState) {
    lucent::error("crashbash-runtime", "Session is absent from the Game");
    std::abort();
  }
  return static_cast<Session &>(*core.game->sessionState);
}

} // namespace crashbash
