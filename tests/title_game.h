// title_game.h — a Game around its own Runtime, for a test of one title owner.
#pragma once

#include "core.h"
#include "game.h"
#include "runtime/runtime.h"
#include "typed_config.h"

#include <memory>

namespace crashbash::test {

class TitleGame {
public:
  // On the heap: a Game holds the whole PSX address space.
  TitleGame() : game_(std::make_unique<Game>(runtime_, psx::config::Config{})) {}

  Game &game() {
    return *game_;
  }
  Core &core() {
    return game_->core;
  }
  Runtime &runtime() {
    return runtime_;
  }

private:
  Runtime runtime_;
  std::unique_ptr<Game> game_;
};

inline std::unique_ptr<TitleGame> makeTitleGame() {
  return std::make_unique<TitleGame>();
}

} // namespace crashbash::test
