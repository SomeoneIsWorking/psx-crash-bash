// Input phase: guest words it reads, per-half re-keying, and that it never yields the reserved
// kUnkeyedPhase (a keyed recording replayed under it would be refused).

#include "core.h"
#include "crashbash_guest.h"
#include "crashbash_input_phase.h"
#include "input_phase.h"
#include "testutil.h"

#include <cstdint>
#include <memory>

namespace {

void test_addresses_are_the_guest_facts() {
  CHECK_EQ(crashbash::guest::kSceneTransition, 0x8009F658u);
  CHECK_EQ(crashbash::guest::kMenuSceneTransition, 0x8009F8A4u);
  CHECK_EQ(crashbash::guest::kSceneCurrentSlot, 0u);
}

void test_reads_the_two_scene_records() {
  const crashbash::InputPhase phase;
  // On the heap: Core holds the whole PSX address space and would overflow a test main() stack.
  auto core = std::make_unique<Core>();

  // Reset state: both scene records are zero and the key is not the reserved one.
  const std::uint64_t reset = phase.of(*core);
  CHECK_EQ(reset, crashbash::InputPhase::pack(0, 0));
  CHECK(reset != psx::input::kUnkeyedPhase);

  // SELECT GAME TYPE: outer scene 0x8009F720, nested screen struct 0x800B8E28.
  core->mem_w32(crashbash::guest::kSceneTransition, 0x8009F720u);
  CHECK_EQ(phase.of(*core), crashbash::InputPhase::pack(0x8009F720u, 0u));
  core->mem_w32(crashbash::guest::kMenuSceneTransition, 0x800B8E28u);
  CHECK_EQ(phase.of(*core), crashbash::InputPhase::pack(0x8009F720u, 0x800B8E28u));

  // A different screen under the same outer scene is a different phase.
  core->mem_w32(crashbash::guest::kMenuSceneTransition, 0x800B8E3Cu);
  CHECK(phase.of(*core) != crashbash::InputPhase::pack(0x8009F720u, 0x800B8E28u));
  CHECK_EQ(phase.of(*core), crashbash::InputPhase::pack(0x8009F720u, 0x800B8E3Cu));

  // Leaving the menu world re-keys on the outer half alone (a match runs as scene 0x8009F480).
  core->mem_w32(crashbash::guest::kSceneTransition, 0x8009F480u);
  CHECK_EQ(phase.of(*core), crashbash::InputPhase::pack(0x8009F480u, 0x800B8E3Cu));

  // Same words give the same key.
  CHECK_EQ(phase.of(*core), phase.of(*core));
}

void test_halves_do_not_alias_and_the_reserved_key_is_unreachable() {
  using crashbash::InputPhase;
  // The halves must not swap.
  CHECK(InputPhase::pack(0x8009F720u, 1u) != InputPhase::pack(1u, 0x8009F720u));
  // Masking to 24 bits drops only the constant 0x80 KSEG prefix.
  CHECK_EQ(InputPhase::pack(0x8009F720u, 0x800B8E28u), InputPhase::pack(0x0009F720u, 0x000B8E28u));
  // The reserved key stays unreachable at the worst case.
  CHECK(InputPhase::pack(0xFFFFFFFFu, 0xFFFFFFFFu) != psx::input::kUnkeyedPhase);
}

} // namespace

int main() {
  RUN(addresses_are_the_guest_facts);
  RUN(reads_the_two_scene_records);
  RUN(halves_do_not_alias_and_the_reserved_key_is_unreachable);
  return pt_summary();
}
