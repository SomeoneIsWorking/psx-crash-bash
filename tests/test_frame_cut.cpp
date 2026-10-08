// A sealed record is a cut exactly when the guest's process state, scene or menu screen changed.
#include "core.h"
#include "crashbash_guest.h"
#include "frame_cut.h"
#include "game.h"
#include "testutil.h"
#include "title_adapter.h"

#include <memory>

namespace {

using crashbash::FrameCut;

crashbash::TitleAdapter runtime;

constexpr FrameCut::SceneIdentity kMatch{0x8004E0B8u, 0x8009F720u, 0x800B8E28u};

void test_first_record_is_a_cut() {
  FrameCut cut;
  CHECK(cut.isCut());
  cut.noteFrameEnded(kMatch);
  CHECK(cut.isCut());
}

void test_same_scene_is_not_a_cut() {
  FrameCut cut;
  cut.noteFrameEnded(kMatch);
  cut.noteFrameEnded(kMatch);
  CHECK(!cut.isCut());
  cut.noteFrameEnded(kMatch);
  CHECK(!cut.isCut());
}

void test_each_scene_word_change_is_a_cut() {
  FrameCut cut;
  cut.noteFrameEnded(kMatch);
  cut.noteFrameEnded({kMatch.processState, 0x800A00DCu, kMatch.menuScreen});
  CHECK(cut.isCut());
  cut.noteFrameEnded({kMatch.processState, 0x800A00DCu, kMatch.menuScreen});
  CHECK(!cut.isCut());
  cut.noteFrameEnded({kMatch.processState, 0x800A00DCu, 0x800BA72Cu});
  CHECK(cut.isCut());
  cut.noteFrameEnded({0x8004E0D0u, 0x800A00DCu, 0x800BA72Cu});
  CHECK(cut.isCut());
}

void test_sample_reads_the_guest_scene_records() {
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.mem_w32(crashbash::guest::kCurrentProcessState, kMatch.processState);
  core.mem_w32(crashbash::guest::kSceneTransition + crashbash::guest::kSceneCurrentSlot, kMatch.scene);
  core.mem_w32(crashbash::guest::kMenuSceneTransition + crashbash::guest::kSceneCurrentSlot, kMatch.menuScreen);
  CHECK(FrameCut::sample(core) == kMatch);
  // Before any frame has ended the adapter declares a cut, so nothing blends across power-on.
  CHECK(runtime.sealedFrameIsCut(core));
}

} // namespace

int main() {
  RUN(first_record_is_a_cut);
  RUN(same_scene_is_not_a_cut);
  RUN(each_scene_word_change_is_a_cut);
  RUN(sample_reads_the_guest_scene_records);
  return pt_summary();
}
