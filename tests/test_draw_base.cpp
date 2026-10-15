// The draw base follows the table swap, so an update-time draw reaches the table walked next.
#include "core.h"
#include "game.h"
#include "render/ordering_table_slots.h"
#include "testutil.h"
#include "title/guest.h"
#include "title_game.h"

#include <memory>

namespace {

namespace guest = crashbash::guest;

constexpr std::uint32_t kSliceBytes = 0x40u;

void test_the_base_moves_to_the_same_slice_of_the_next_table() {
  auto game = crashbash::test::makeTitleGame();
  Core &core = game->core();
  core.mem_w32(guest::kDrawOtBase, guest::kOrderingTableA + kSliceBytes);
  crashbash::render::followDrawBase(core, guest::kOrderingTableA, guest::kOrderingTableB);
  CHECK(core.mem_r32(guest::kDrawOtBase) == guest::kOrderingTableB + kSliceBytes);
  crashbash::render::followDrawBase(core, guest::kOrderingTableB, guest::kOrderingTableA);
  CHECK(core.mem_r32(guest::kDrawOtBase) == guest::kOrderingTableA + kSliceBytes);
}

void test_a_base_outside_the_walked_table_is_left_alone() {
  auto game = crashbash::test::makeTitleGame();
  Core &core = game->core();
  core.mem_w32(guest::kDrawOtBase, guest::kOrderingTableB + kSliceBytes);
  crashbash::render::followDrawBase(core, guest::kOrderingTableA, guest::kOrderingTableB);
  CHECK(core.mem_r32(guest::kDrawOtBase) == guest::kOrderingTableB + kSliceBytes);
  core.mem_w32(guest::kDrawOtBase, 0u);
  crashbash::render::followDrawBase(core, guest::kOrderingTableA, guest::kOrderingTableB);
  CHECK(core.mem_r32(guest::kDrawOtBase) == 0u);
}

} // namespace

int main() {
  RUN(the_base_moves_to_the_same_slice_of_the_next_table);
  RUN(a_base_outside_the_walked_table_is_left_alone);
  return pt_summary();
}
