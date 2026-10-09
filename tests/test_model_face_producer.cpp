// The model producer keys each face packet by the drawing object's incarnation and the face, through the
// shipping overrides.
#include "component_incarnation.h"
#include "core.h"
#include "crashbash_frame_driver.h"
#include "crashbash_guest.h"
#include "face_projection.h"
#include "game.h"
#include "guest_execution.h"
#include "model_face_producer.h"
#include "ordering_table_slots.h"
#include "packet_collector.h"
#include "state_render_check.h"
#include "state_renders.h"
#include "testutil.h"
#include "title_adapter.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
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
constexpr std::uint32_t kVertices = 0x80190000u;

crashbash::TitleAdapter runtime;

// FUN_80019A60 stand-in: calls the face emitter with a0 = a1 (the packet buffer), a1 = the vertices, a2 = the
// face list.
void modelDrawStub(Core &core) {
  const std::uint32_t words[] = {
      0x27BDFFE8u,                     // addiu sp, sp, -24
      0xAFBF0010u,                     // sw ra, 16(sp)
      0x00A02025u,                     // move a0, a1
      0x0C0064EAu,                     // jal 0x800193A8
      0x3C050000u | (kVertices >> 16), // lui a1, vertices
      0x8FBF0010u,                     // lw ra, 16(sp)
      0x03E00008u,                     // jr ra
      0x27BD0018u,                     // addiu sp, sp, 24
  };
  for (std::uint32_t i = 0; i < std::size(words); ++i) {
    core.mem_w32(crashbash::guest::kModelDraw + i * 4u, words[i]);
  }
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
  crashbash::render::nameOrderingTables(core);
  crashbash::render::registerStateRenders(core);
  crashbash::test::warmUpExecutor(core, 0x80018100u);
  return game;
}

std::uint32_t pointWord(int x, int y) {
  return (static_cast<std::uint32_t>(y) << 16) | static_cast<std::uint16_t>(x);
}

// A mesh of three faces over seven vertices: strip 0 is two textured faces at depth 400 sharing a bucket,
// strip 1 a Gouraud face at 600. `culled` flips the second face's winding flag.
void writeVertices(Core &core, int shift, bool culled) {
  struct Vertex {
    int x;
    int y;
    int z;
    std::uint32_t flags;
  };
  const Vertex vertices[] = {{20, 30, 400, 0u},
                             {60, 34, 400, 0u},
                             {30, 60, 400, 0u},
                             {70, 64, 400, culled ? 0u : 0x10000u},
                             {100, 50, 600, 0u},
                             {140, 54, 600, 0u},
                             {110, 90, 600, 0u}};
  std::uint32_t at = kVertices;
  for (const Vertex &vertex : vertices) {
    core.mem_w32(at, pointWord(vertex.x + shift, vertex.y));
    core.mem_w32(at + 4u, vertex.flags | static_cast<std::uint16_t>(vertex.z));
    at += 8u;
  }
}

// The packets the guest copies from its templates each frame: colours, texture words and draw modes, with
// the lengths the face shape leaves.
void writePackets(Core &core, std::uint32_t buffer) {
  for (std::uint32_t face = 0; face < 3u; ++face) {
    const std::uint32_t packet = buffer + face * kFacePacketBytes;
    std::uint32_t words[10] = {};
    if (face < 2u) {
      const std::uint32_t textured[] = {0u,
                                        0x34808080u | (face << 8),
                                        0u,
                                        0x7810u | (face << 4),
                                        0x00A0A0A0u,
                                        0u,
                                        (0x0025u << 16) | 0x1020u,
                                        0x00404040u,
                                        0u,
                                        0x3040u};
      std::copy(std::begin(textured), std::end(textured), words);
    } else {
      const std::uint32_t shaded[] = {8u << 24, 0xE1000200u, 0u, 0x30302010u, 0u, 0x00504030u, 0u, 0x00706050u, 0u, 0u};
      std::copy(std::begin(shaded), std::end(shaded), words);
    }
    for (std::uint32_t word = 0; word < 10u; ++word) {
      core.mem_w32(packet + word * 4u, words[word]);
    }
  }
}

// The guest's state before it draws a model: a cleared table, the vertices it animated and the camera.
void loadScene(Core &core, std::uint32_t buffer, int shift = 0, bool culled = false, int cameraX = 0) {
  crashbash::test::OrderingTableModel(core).begin();
  writeVertices(core, shift, culled);
  writePackets(core, buffer);
  crashbash::test::setCamera(cameraX);
}

bool runModelDraw(Core &core, std::uint32_t object, std::uint32_t buffer) {
  core.r[4] = 0x1000u;
  core.r[5] = buffer;
  core.r[6] = kFaceList;
  core.r[7] = object;
  core.r[29] = 0x801FFF00u;
  core.r[31] = kReturn;
  return psx::cpu::dispatchGuest(core, crashbash::guest::kModelDraw, psx::cpu::ExecutionBudget::fromCycles(100000u))
      .returned();
}

bool drawObject(Core &core, std::uint32_t object, std::uint32_t buffer) {
  loadScene(core, buffer);
  return runModelDraw(core, object, buffer);
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
  const auto list = crashbash::render::readFaceList(game->core, kFaceList);
  CHECK_EQ(crashbash::render::faceCount(list), 3u);
  CHECK_EQ(crashbash::render::faceVertexWords(list), 14u);
  static_assert(meshFaceElement(0x80120003u, 2u) == ((0x120003u << 11) | 2u));
}

void test_each_face_is_keyed_by_object_and_index() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawObject(core, kObjectA, kEvenBuffer));
  for (std::uint32_t face = 0; face < 3u; ++face) {
    CHECK(core.emission.identityFor(kEvenBuffer + face * kFacePacketBytes) == faceKey(kObjectA, face));
  }
  CHECK(!core.emission.identityFor(kEvenBuffer + 3u * kFacePacketBytes));
  CHECK(core.emission.identityFor(kEvenBuffer + 4u) ==
        (RecordKey{crashbash::guest::kModelDraw, incarnationObject(kObjectA, 0u), 0u, 0u}));
}

void test_keys_follow_the_object_not_the_packet_buffer() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawObject(core, kObjectA, kEvenBuffer));
  CHECK(drawObject(core, kObjectB, kOddBuffer));
  CHECK(core.emission.identityFor(kEvenBuffer + kFacePacketBytes) == faceKey(kObjectA, 1u));
  CHECK(core.emission.identityFor(kOddBuffer + kFacePacketBytes) == faceKey(kObjectB, 1u));

  // Next frame the objects pop each other's buffers; each face keeps its object's key.
  CHECK(drawObject(core, kObjectB, kEvenBuffer));
  CHECK(drawObject(core, kObjectA, kOddBuffer));
  CHECK(core.emission.identityFor(kOddBuffer + 2u * kFacePacketBytes) == faceKey(kObjectA, 2u));
  CHECK(core.emission.identityFor(kEvenBuffer + 2u * kFacePacketBytes) == faceKey(kObjectB, 2u));
}

void test_faces_outside_a_model_draw_stay_unkeyed() {
  auto game = makeGame();
  Core &core = game->core;
  loadScene(core, kEvenBuffer);
  core.r[4] = kEvenBuffer;
  core.r[5] = kVertices;
  core.r[6] = kFaceList;
  core.r[29] = 0x801FFF00u;
  core.r[31] = kReturn;
  CHECK(psx::cpu::dispatchGuest(core, crashbash::guest::kMeshFaceEmit, psx::cpu::ExecutionBudget::fromCycles(100000u))
            .returned());
  CHECK(!core.emission.identityFor(kEvenBuffer));
}

void test_a_component_drawn_frame_after_frame_keeps_its_key() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawObject(core, kComponents, kEvenBuffer));
  CHECK(core.emission.identityFor(kEvenBuffer) == faceKey(kComponents, 0u));
  CHECK(drawObject(core, kComponents, kOddBuffer));
  CHECK(core.emission.identityFor(kOddBuffer) == faceKey(kComponents, 0u));
}

// FUN_8001D3B0 rebuilding a page re-initialises its buffer's components in place.
void test_a_component_reinitialised_in_its_buffer_gets_a_new_key() {
  auto game = makeGame();
  Core &core = game->core;
  const std::uint32_t second = kComponents + crashbash::guest::kRenderComponentBytes;
  CHECK(drawObject(core, second, kEvenBuffer));
  CHECK(callGuest(core, crashbash::guest::kBufferComponentInit, 0u, 2u, kComponents));
  CHECK(drawObject(core, second, kOddBuffer));
  CHECK(core.emission.identityFor(kOddBuffer) == faceKey(second, 0u, 1u));
  CHECK(!(faceKey(second, 0u, 1u) == faceKey(second, 0u)));
  // Past the count it was given, the buffer is not touched.
  CHECK(drawObject(core, second + crashbash::guest::kRenderComponentBytes, kEvenBuffer));
  CHECK(core.emission.identityFor(kEvenBuffer) == faceKey(second + crashbash::guest::kRenderComponentBytes, 0u));
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
  CHECK(drawObject(core, component, kEvenBuffer));
  core.mem_w32(crashbash::guest::kAnimationNodeFreeList, kNode);
  CHECK(callGuest(core, crashbash::guest::kAnimationNodeCreateType0, 0u, 0u, 0u));
  CHECK(drawObject(core, component, kOddBuffer));
  CHECK(core.emission.identityFor(kOddBuffer) == faceKey(component, 0u, 1u));
}

// A model draw over the scene above, through the shipping mesh body.
class ModelScene {
public:
  ModelScene() : game(makeGame()) {}

  // `shift` moves the vertices, `cameraX` the camera; `culled` flips the second face's winding flag.
  psx::present::FrameRecord
  draw(std::uint32_t object, std::uint32_t buffer, int shift, bool culled = false, int cameraX = 0) {
    loadScene(game->core, buffer, shift, culled, cameraX);
    if (!runModelDraw(game->core, object, buffer)) {
      std::abort();
    }
    if (afterGuest) {
      afterGuest();
    }
    crashbash::frameDriver(game->core).packetCollector().commit(game->core);
    return crashbash::test::OrderingTableModel(game->core).walk();
  }

  // Overwrites everything a render could read besides its state.
  void scramble(std::uint32_t buffer) {
    Core &core = game->core;
    crashbash::test::OrderingTableModel(core).scramble();
    for (std::uint32_t word = 0; word < 3u * kFacePacketBytes / 4u; ++word) {
      core.mem_w32(buffer + word * 4u, 0xA5A5A5A5u);
    }
    for (std::uint32_t word = 0; word < 14u; ++word) {
      core.mem_w32(kVertices + word * 4u, 0xA5A5A5A5u);
    }
    crashbash::test::setCamera(0x1234);
  }

  std::unique_ptr<Game> game;
  // Runs between the guest's draw and the display's DrawOTag, where the guest can still change the table.
  std::function<void()> afterGuest;
};

void checkModelRender(const std::function<psx::present::FrameRecord(ModelScene &, int)> &draw) {
  ModelScene scene;
  crashbash::test::checkStateRender(
      scene.game->core,
      crashbash::guest::kModelDraw,
      incarnationObject(kObjectA, 0u),
      [&](int shift) {
        return draw(scene, shift);
      },
      [&] {
        scene.scramble(kEvenBuffer);
      });
}

// The vertices the guest animated move; the render re-projects them.
void test_the_render_at_t_reprojects_moved_vertices() {
  checkModelRender([](ModelScene &scene, int shift) {
    return scene.draw(kObjectA, kEvenBuffer, shift);
  });
}

// The camera moves; the render projects through the blended transform.
void test_the_render_at_t_reprojects_through_a_moved_camera() {
  checkModelRender([](ModelScene &scene, int shift) {
    return scene.draw(kObjectA, kEvenBuffer, 0, false, shift * 8);
  });
}

// A culled face has no packet on the walk; a face that comes into view appears at the later state.
void test_a_culled_face_is_not_drawn_and_a_new_face_appears_at_t_one() {
  ModelScene scene;
  Core &core = scene.game->core;
  const auto id = psx::present::ObjectId{crashbash::guest::kModelDraw, incarnationObject(kObjectA, 0u)};
  const psx::present::StateProducer *render = core.stateProducers.find(id.producer);
  CHECK(render != nullptr);
  const psx::present::FrameRecord full = scene.draw(kObjectA, kOddBuffer, 0);
  CHECK_EQ(crashbash::test::primitivesOf(full, id.producer).size(), 3u);
  const psx::present::FrameState all = core.frameStates.collect(full);
  const psx::present::FrameRecord withoutMiddle = scene.draw(kObjectA, kEvenBuffer, 0, true);
  CHECK_EQ(crashbash::test::primitivesOf(withoutMiddle, id.producer).size(), 2u);
  const psx::present::FrameState fewer = core.frameStates.collect(withoutMiddle);

  crashbash::test::CollectedSink grown;
  render->render(*fewer.find(id), *all.find(id), 1.0f, grown);
  CHECK_EQ(grown.drawn.size(), 3u);
  crashbash::test::CollectedSink shrunk;
  render->render(*all.find(id), *fewer.find(id), 1.0f, shrunk);
  CHECK_EQ(shrunk.drawn.size(), 2u);
}

// Unlinks the packet at the head of the farthest bucket in use, as a later draw relinking the table would.
void unlinkFarthestPacket(Core &core) {
  for (std::uint32_t bucket = crashbash::guest::kOrderingTableBuckets - 1u; bucket > 0u; --bucket) {
    const std::uint32_t below = crashbash::test::OrderingTableModel::tableBucket(bucket - 1u);
    const std::uint32_t head = core.mem_r32(crashbash::test::OrderingTableModel::tableBucket(bucket));
    if ((head & psx::gpu::kOtNextAddressMask) != (below & psx::gpu::kOtNextAddressMask)) {
      core.mem_w32(crashbash::test::OrderingTableModel::tableBucket(bucket),
                   core.mem_r32(head | 0x80000000u) & psx::gpu::kOtNextAddressMask);
      return;
    }
  }
}

// A packet another draw relinked away after its own scope ended is not on the walk, so it is not in the state.
void test_a_packet_unlinked_after_its_scope_is_not_saved() {
  ModelScene scene;
  Core &core = scene.game->core;
  const auto id = psx::present::ObjectId{crashbash::guest::kModelDraw, incarnationObject(kObjectA, 0u)};
  scene.afterGuest = [&] {
    unlinkFarthestPacket(core);
  };
  const psx::present::FrameRecord record = scene.draw(kObjectA, kEvenBuffer, 0);
  CHECK_EQ(crashbash::test::primitivesOf(record, id.producer).size(), 2u);
  const psx::present::FrameState state = core.frameStates.collect(record);
  const auto saved = state.find(id);
  CHECK(saved.has_value());
  crashbash::test::CollectedSink drawn;
  core.stateProducers.find(id.producer)->render(*saved, *saved, 1.0f, drawn);
  CHECK_EQ(drawn.drawn.size(), 2u);
}

// A face the guest left unlinked still points into the table of the frame before, which is intact until it is
// cleared; it is not on the walk of the table in use.
void test_a_face_linked_in_the_other_table_is_not_saved() {
  ModelScene scene;
  Core &core = scene.game->core;
  const auto id = psx::present::ObjectId{crashbash::guest::kModelDraw, incarnationObject(kObjectA, 0u)};
  scene.afterGuest = [&] {
    core.mem_w32(crashbash::guest::kOrderingTablePointer, crashbash::guest::kOrderingTableB);
  };
  const psx::present::FrameRecord record = scene.draw(kObjectA, kEvenBuffer, 0);
  CHECK(!core.frameStates.collect(record).find(id).has_value());
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
  RUN(the_render_at_t_reprojects_moved_vertices);
  RUN(the_render_at_t_reprojects_through_a_moved_camera);
  RUN(a_culled_face_is_not_drawn_and_a_new_face_appears_at_t_one);
  RUN(a_packet_unlinked_after_its_scope_is_not_saved);
  RUN(a_face_linked_in_the_other_table_is_not_saved);
  return pt_summary();
}
