// The `pause` request: armed by a command, applied at frame boundaries as pad presses.
#include "core.h"
#include "debug/dev_pause.h"
#include "game.h"
#include "runtime/session.h"
#include "testutil.h"
#include "title/guest.h"
#include "title_command.h"
#include "title_game.h"

#include <memory>
#include <string>

namespace {

using crashbash::debug::DevPause;
namespace guest = crashbash::guest;

constexpr std::uint16_t kStartPressed = 0xFFFF & ~0x0008u;
constexpr std::uint16_t kDownPressed = 0xFFFF & ~0x0040u;
constexpr std::uint16_t kConfirmPressed = 0xFFFF & ~0x4000u;

std::unique_ptr<crashbash::test::TitleGame> makeGame() {
  return crashbash::test::makeTitleGame();
}

void enterMatch(Core &core) {
  core.mem_w32(guest::kSceneTransition + guest::kSceneCurrentSlot, guest::kModeScene);
  core.mem_w32(guest::kPauseFlags, 0u);
}

void test_the_command_arms_and_changes_nothing() {
  auto game = makeGame();
  Core &core = game->core();
  enterMatch(core);
  DevPause pause;
  std::string reply;
  CHECK(!pause.handle(core, {"arena", "arena list"}, reply));
  CHECK(pause.handle(core, {"pause", "pause open"}, reply));
  CHECK(pause.armed());
  CHECK_EQ(game->game().pad.repl_on, 0);
  CHECK_EQ(core.mem_r32(guest::kPauseFlags), 0u);
}

void test_open_presses_start_in_the_mode_scene_until_the_page_opens() {
  auto game = makeGame();
  Core &core = game->core();
  enterMatch(core);
  DevPause pause;
  std::string reply;
  CHECK(pause.handle(core, {"pause", "pause open"}, reply));
  pause.applyArmed(core, game->game().pad, 100u);
  CHECK_EQ(game->game().pad.repl_on, 1);
  CHECK_EQ(static_cast<unsigned>(game->game().pad.repl_tap), static_cast<unsigned>(kStartPressed));
  CHECK(pause.armed());
  // The guest's dispatcher sets its flag; the request completes without another press.
  core.mem_w32(guest::kPauseFlags, guest::kPauseOpenBit);
  game->game().pad.driveRelease();
  pause.applyArmed(core, game->game().pad, 101u);
  CHECK(!pause.armed());
  CHECK_EQ(game->game().pad.repl_on, 0);
}

void test_open_waits_outside_the_mode_scene() {
  auto game = makeGame();
  Core &core = game->core();
  enterMatch(core);
  core.mem_w32(guest::kSceneTransition + guest::kSceneCurrentSlot, 0x800A00DCu);
  DevPause pause;
  std::string reply;
  CHECK(pause.handle(core, {"pause", "pause open"}, reply));
  pause.applyArmed(core, game->game().pad, 100u);
  CHECK_EQ(game->game().pad.repl_on, 0);
  CHECK(pause.armed());
  core.mem_w32(guest::kSceneTransition + guest::kSceneCurrentSlot, guest::kModeScene);
  pause.applyArmed(core, game->game().pad, 130u);
  CHECK_EQ(game->game().pad.repl_on, 1);
}

void test_open_is_abandoned_after_the_budget() {
  auto game = makeGame();
  Core &core = game->core();
  core.mem_w32(guest::kSceneTransition + guest::kSceneCurrentSlot, 0x800A00DCu);
  DevPause pause;
  std::string reply;
  CHECK(pause.handle(core, {"pause", "pause open"}, reply));
  pause.applyArmed(core, game->game().pad, 100u);
  pause.applyArmed(core, game->game().pad, 100u + 601u);
  CHECK(!pause.armed());
  CHECK_EQ(game->game().pad.repl_on, 0);
}

void test_down_needs_an_open_page_and_presses_down_once() {
  auto game = makeGame();
  Core &core = game->core();
  enterMatch(core);
  DevPause pause;
  std::string reply;
  CHECK(pause.handle(core, {"pause", "pause down"}, reply));
  CHECK(!pause.armed());
  CHECK(reply.find("refused") != std::string::npos);
  core.mem_w32(guest::kPauseFlags, guest::kPauseOpenBit);
  CHECK(pause.handle(core, {"pause", "pause down"}, reply));
  CHECK(pause.armed());
  pause.applyArmed(core, game->game().pad, 200u);
  CHECK_EQ(static_cast<unsigned>(game->game().pad.repl_tap), static_cast<unsigned>(kDownPressed));
  CHECK(!pause.armed());
}

void test_confirm_presses_cross_and_an_unknown_word_is_usage() {
  auto game = makeGame();
  Core &core = game->core();
  core.mem_w32(guest::kPauseFlags, guest::kPauseOpenBit);
  DevPause pause;
  std::string reply;
  CHECK(pause.handle(core, {"pause", "pause confirm"}, reply));
  pause.applyArmed(core, game->game().pad, 300u);
  CHECK_EQ(static_cast<unsigned>(game->game().pad.repl_tap), static_cast<unsigned>(kConfirmPressed));
  CHECK(pause.handle(core, {"pause", "pause sideways"}, reply));
  CHECK(!pause.armed());
  CHECK(reply.find("usage") != std::string::npos);
}

void test_open_is_refused_when_already_open() {
  auto game = makeGame();
  Core &core = game->core();
  core.mem_w32(guest::kPauseFlags, guest::kPauseOpenBit);
  DevPause pause;
  std::string reply;
  CHECK(pause.handle(core, {"pause", "pause open"}, reply));
  CHECK(!pause.armed());
}

void test_the_title_command_arms_the_session_request() {
  auto game = makeGame();
  Core &core = game->core();
  enterMatch(core);
  CHECK(psx::game::runTitleCommand(core, "pause open").handled);
  CHECK(crashbash::session(core).devPause.armed());
  CHECK(!psx::game::runTitleCommand(core, "unknown").handled);
}

} // namespace

int main() {
  RUN(the_command_arms_and_changes_nothing);
  RUN(open_presses_start_in_the_mode_scene_until_the_page_opens);
  RUN(open_waits_outside_the_mode_scene);
  RUN(open_is_abandoned_after_the_budget);
  RUN(down_needs_an_open_page_and_presses_down_once);
  RUN(confirm_presses_cross_and_an_unknown_word_is_usage);
  RUN(open_is_refused_when_already_open);
  RUN(the_title_command_arms_the_session_request);
  return psx::test::summary();
}
