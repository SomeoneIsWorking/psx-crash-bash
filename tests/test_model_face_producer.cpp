// The model producer keys each face packet by the drawing object's incarnation and the face, through the
// shipping overrides.
#include "component_incarnation.h"
#include "core.h"
#include "crashbash_frame_driver.h"
#include "crashbash_guest.h"
#include "game.h"
#include "guest_execution.h"
#include "model_face_producer.h"
#include "testutil.h"
#include "title_adapter.h"

#include <memory>

namespace {

using crashbash::render::incarnationObject;
using crashbash::render::kFacePacketBytes;
using crashbash::render::meshFaceElement;
using psx::present::RecordKey;

constexpr GuestAddressRange kRange{0x18000u, 0x22000u};
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr std::uint32_t kFaceList = 0x80120003u;
constexpr std::uint32_t kObjectA = 0x80130000u;
constexpr std::uint32_t kObjectB = 0x80130100u;
constexpr std::uint32_t kEvenBuffer = 0x80140000u;
constexpr std::uint32_t kOddBuffer = 0x80141000u;
constexpr std::uint32_t kComponents = 0x80150000u;
constexpr std::uint32_t kNode = 0x80160000u;

crashbash::TitleAdapter runtime;

// FUN_80019A60 stand-in: calls the face emitter with a0 = a1 (the packet buffer), a2 = the face list.
void modelDrawStub(Core &core) {
  const std::uint32_t words[] = {
      0x27BDFFE8u, // addiu sp, sp, -24
      0xAFBF0010u, // sw ra, 16(sp)
      0x0C0064EAu, // jal 0x800193A8
      0x00A02025u, // move a0, a1
      0x8FBF0010u, // lw ra, 16(sp)
      0x03E00008u, // jr ra
      0x27BD0018u, // addiu sp, sp, 24
  };
  for (std::uint32_t i = 0; i < std::size(words); ++i) {
    core.mem_w32(crashbash::guest::kModelDraw + i * 4u, words[i]);
  }
}

// FUN_800193A8 stand-in: writes the command word of one packet at t0, as the guest writes each visible face.
void faceEmitStub(Core &core) {
  core.mem_w32(crashbash::guest::kMeshFaceEmit, 0x03E00008u);      // jr ra
  core.mem_w32(crashbash::guest::kMeshFaceEmit + 4u, 0xAD000004u); // sw zero, 4(t0)
}

// A template copier stand-in: returns `taken` in v0, as the pool allocators report their count.
void copierStub(Core &core, std::uint32_t address, std::uint32_t taken) {
  core.mem_w32(address, 0x03E00008u);              // jr ra
  core.mem_w32(address + 4u, 0x24020000u | taken); // li v0, taken
}

std::unique_ptr<Game> makeGame() {
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  modelDrawStub(core);
  faceEmitStub(core);
  copierStub(core, crashbash::guest::kBufferComponentInit, 0u);
  copierStub(core, crashbash::guest::kEntityComponentAlloc, 2u);
  copierStub(core, crashbash::guest::kAnimationNodeCreateType0, 0u);
  // Strips of 2 and 1 faces.
  const std::uint8_t faceList[] = {0x00u, 0x02u, 0x01u, 0x01u, 0x00u, 0xFFu};
  for (std::uint32_t i = 0; i < std::size(faceList); ++i) {
    core.mem_w8(kFaceList + i, faceList[i]);
  }
  const auto image = core.imageCatalog().activate("synthetic-model-draw", kRange, 1u);
  static_cast<crashbash::runtime::GuestExecution *>(core.gameCtx)
      ->bindAuthenticatedImage(crashbash::runtime::GuestImage::Resident, image, kRange);
  crashbash::render::registerModelFaceProducer(core);
  crashbash::render::registerComponentIncarnationOwners(core);
  return game;
}

bool drawObject(Core &core, std::uint32_t object, std::uint32_t buffer, std::uint32_t written) {
  core.r[4] = 0x1000u;
  core.r[5] = buffer;
  core.r[6] = kFaceList;
  core.r[7] = object;
  core.r[8] = written;
  core.r[29] = 0x801FFF00u;
  core.r[31] = kReturn;
  return psx::cpu::dispatchGuest(core, crashbash::guest::kModelDraw, psx::cpu::ExecutionBudget::fromCycles(100000u))
      .returned();
}

bool callGuest(Core &core, std::uint32_t address, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
  core.r[4] = a0;
  core.r[5] = a1;
  core.r[6] = a2;
  core.r[29] = 0x801FFF00u;
  core.r[31] = kReturn;
  return psx::cpu::dispatchGuest(core, address, psx::cpu::ExecutionBudget::fromCycles(100000u)).returned();
}

RecordKey faceKey(std::uint32_t object, std::uint32_t face, std::uint32_t generation = 0u) {
  return RecordKey{
      crashbash::guest::kModelDraw, incarnationObject(object, generation), meshFaceElement(kFaceList, face), 0u};
}

void test_face_list_counts_every_strip() {
  auto game = makeGame();
  CHECK_EQ(crashbash::render::meshFaceCount(game->core, kFaceList), 3u);
  static_assert(meshFaceElement(0x80120003u, 2u) == ((0x120003u << 11) | 2u));
}

void test_each_face_is_keyed_by_object_and_index() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawObject(core, kObjectA, kEvenBuffer, kEvenBuffer + 4u));
  for (std::uint32_t face = 0; face < 3u; ++face) {
    CHECK(core.emission.keyFor(kEvenBuffer + face * kFacePacketBytes) == faceKey(kObjectA, face));
  }
  CHECK(!core.emission.keyFor(kEvenBuffer + 3u * kFacePacketBytes));
  CHECK(core.emission.keyFor(kEvenBuffer + 4u) ==
        (RecordKey{crashbash::guest::kModelDraw, incarnationObject(kObjectA, 0u), 0u, 0u}));
}

void test_keys_follow_the_object_not_the_packet_buffer() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawObject(core, kObjectA, kEvenBuffer, kEvenBuffer));
  CHECK(drawObject(core, kObjectB, kOddBuffer, kOddBuffer));
  CHECK(core.emission.keyFor(kEvenBuffer + kFacePacketBytes) == faceKey(kObjectA, 1u));
  CHECK(core.emission.keyFor(kOddBuffer + kFacePacketBytes) == faceKey(kObjectB, 1u));

  // Next frame the objects pop each other's buffers; each face keeps its object's key.
  CHECK(drawObject(core, kObjectB, kEvenBuffer, kEvenBuffer));
  CHECK(drawObject(core, kObjectA, kOddBuffer, kOddBuffer));
  CHECK(core.emission.keyFor(kOddBuffer + 2u * kFacePacketBytes) == faceKey(kObjectA, 2u));
  CHECK(core.emission.keyFor(kEvenBuffer + 2u * kFacePacketBytes) == faceKey(kObjectB, 2u));
}

void test_faces_outside_a_model_draw_stay_unkeyed() {
  auto game = makeGame();
  Core &core = game->core;
  core.r[4] = kEvenBuffer;
  core.r[6] = kFaceList;
  core.r[8] = kEvenBuffer;
  core.r[29] = 0x801FFF00u;
  core.r[31] = kReturn;
  CHECK(psx::cpu::dispatchGuest(core, crashbash::guest::kMeshFaceEmit, psx::cpu::ExecutionBudget::fromCycles(100000u))
            .returned());
  CHECK(!core.emission.keyFor(kEvenBuffer));
}

void test_a_component_drawn_frame_after_frame_keeps_its_key() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawObject(core, kComponents, kEvenBuffer, kEvenBuffer));
  CHECK(core.emission.keyFor(kEvenBuffer) == faceKey(kComponents, 0u));
  CHECK(drawObject(core, kComponents, kOddBuffer, kOddBuffer));
  CHECK(core.emission.keyFor(kOddBuffer) == faceKey(kComponents, 0u));
}

// FUN_8001D3B0 rebuilding a page re-initialises its buffer's components in place.
void test_a_component_reinitialised_in_its_buffer_gets_a_new_key() {
  auto game = makeGame();
  Core &core = game->core;
  const std::uint32_t second = kComponents + crashbash::guest::kRenderComponentBytes;
  CHECK(drawObject(core, second, kEvenBuffer, kEvenBuffer));
  CHECK(callGuest(core, crashbash::guest::kBufferComponentInit, 0u, 2u, kComponents));
  CHECK(drawObject(core, second, kOddBuffer, kOddBuffer));
  CHECK(core.emission.keyFor(kOddBuffer) == faceKey(second, 0u, 1u));
  CHECK(!(faceKey(second, 0u, 1u) == faceKey(second, 0u)));
  // Past the count it was given, the buffer is not touched.
  CHECK(drawObject(core, second + crashbash::guest::kRenderComponentBytes, kEvenBuffer, kEvenBuffer));
  CHECK(core.emission.keyFor(kEvenBuffer) == faceKey(second + crashbash::guest::kRenderComponentBytes, 0u));
}

// The pool allocator takes the pool's first v0 components along their +0x5C chain.
void test_components_taken_from_the_pool_get_new_keys() {
  auto game = makeGame();
  Core &core = game->core;
  const std::uint32_t second = kComponents + 0x400u;
  const std::uint32_t third = kComponents + 0x800u;
  core.mem_w32(crashbash::guest::kRenderComponentPool, kComponents);
  core.mem_w32(kComponents + crashbash::guest::kRenderComponentNext, second);
  core.mem_w32(second + crashbash::guest::kRenderComponentNext, third);
  CHECK(callGuest(core, crashbash::guest::kEntityComponentAlloc, 0u, 2u, 0u));
  const auto &incarnations = crashbash::frameDriver(core).componentIncarnations();
  CHECK_EQ(incarnations.object(kComponents), incarnationObject(kComponents, 1u));
  CHECK_EQ(incarnations.object(second), incarnationObject(second, 1u));
  CHECK_EQ(incarnations.object(third), incarnationObject(third, 0u));
}

// An animation node popped off the free list carries its component at +8.
void test_an_animation_node_popped_again_gets_a_new_key() {
  auto game = makeGame();
  Core &core = game->core;
  const std::uint32_t component = kNode + crashbash::guest::kAnimationNodeComponent;
  CHECK(drawObject(core, component, kEvenBuffer, kEvenBuffer));
  core.mem_w32(crashbash::guest::kAnimationNodeFreeList, kNode);
  CHECK(callGuest(core, crashbash::guest::kAnimationNodeCreateType0, 0u, 0u, 0u));
  CHECK(drawObject(core, component, kOddBuffer, kOddBuffer));
  CHECK(core.emission.keyFor(kOddBuffer) == faceKey(component, 0u, 1u));
}

} // namespace

int main() {
  RUN(face_list_counts_every_strip);
  RUN(each_face_is_keyed_by_object_and_index);
  RUN(keys_follow_the_object_not_the_packet_buffer);
  RUN(faces_outside_a_model_draw_stay_unkeyed);
  RUN(a_component_drawn_frame_after_frame_keeps_its_key);
  RUN(a_component_reinitialised_in_its_buffer_gets_a_new_key);
  RUN(components_taken_from_the_pool_get_new_keys);
  RUN(an_animation_node_popped_again_gets_a_new_key);
  return pt_summary();
}
