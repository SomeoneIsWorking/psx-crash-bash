// A frame's ordering table as the guest leaves it, the record the walk makes of it, and the comparison a
// producer's render has to pass against that record.
#pragma once

#include "core.h"
#include "crashbash_guest.h"
#include "frame_composer.h"
#include "frame_record.h"
#include "frame_state.h"
#include "gte_control.h"
#include "guest_call.h"
#include "ordering_table.h"
#include "state_producer.h"
#include "testutil.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

namespace crashbash::test {

// ClearOTagR'd table A with the guest's pointer at it and the draw globals the native bodies read, in guest
// memory; the guest's own bodies then link into it.
class OrderingTableModel {
public:
  static constexpr std::uint32_t kPool = 0x80180000u;
  static constexpr std::uint32_t kEnvironment = 0x801A0000u;
  static constexpr std::int32_t kDisplayWidth = 640;
  static constexpr std::int32_t kDepthBias = 10;

  explicit OrderingTableModel(Core &core) : core_(core) {}

  void begin() {
    for (std::uint32_t bucket = 0; bucket < guest::kOrderingTableBuckets; ++bucket) {
      core_.mem_w32(tableBucket(bucket),
                    bucket == 0u ? psx::gpu::kOtChainEnd : (tableBucket(bucket - 1u) & psx::gpu::kOtNextAddressMask));
    }
    core_.mem_w32(guest::kOrderingTablePointer, guest::kOrderingTableA);
    core_.mem_w32(guest::kOrderingTableA + guest::kOrderingTablePoolCursor, kPool);
    core_.mem_w32(guest::kDrawEnvironment, kEnvironment);
    core_.mem_w16(kEnvironment + guest::kEnvironmentScaleOffset, static_cast<std::uint16_t>(kDisplayWidth));
    core_.mem_w32(guest::kScreenFade, 0u);
    core_.mem_w32(guest::kDrawOtBase, guest::kOrderingTableA);
    core_.mem_w16(guest::kDrawZBias, static_cast<std::uint16_t>(kDepthBias));
    core_.mem_w16(guest::kDrawZLimit, static_cast<std::uint16_t>(guest::kOrderingTableBuckets));
    core_.mem_w32(guest::kDrawOriginX, 0u);
    core_.mem_w32(guest::kDrawOriginY, 0u);
  }

  // The record DrawOTag makes of the table.
  psx::present::FrameRecord walk() {
    psx::gpu::submitOrderingTable(
        core_, core_.gpuDevice, guest::kOrderingTableA + 4u * (guest::kOrderingTableBuckets - 1u));
    return core_.gpuDevice.sealRecord();
  }

  // Scribbles over the pool, the table's lowest buckets and the globals, so a render that reads any of them
  // would draw something else.
  void scramble() {
    for (std::uint32_t word = 0; word < 0x400u; ++word) {
      core_.mem_w32(kPool + word * 4u, 0xA5A5A5A5u);
    }
    for (std::uint32_t bucket = 0; bucket < 0x100u; ++bucket) {
      core_.mem_w32(tableBucket(bucket), 0xA5A5A5A5u);
    }
    for (const std::uint32_t global : {guest::kScreenFade,
                                       guest::kDrawOtBase,
                                       guest::kDrawZBias,
                                       guest::kDrawOriginX,
                                       guest::kDrawOriginY,
                                       kEnvironment + guest::kEnvironmentScaleOffset}) {
      core_.mem_w32(global, 0xA5A5A5A5u);
    }
  }

  static std::uint32_t tableBucket(std::uint32_t bucket) {
    return guest::kOrderingTableA + bucket * 4u;
  }

private:
  Core &core_;
};

// Rotation of 1, a 512 projection plane and a 320 by 240 screen centre; `translationX` moves the camera.
inline void setCamera(std::int32_t translationX) {
  psx::present::GteControl control{};
  control[0] = 0x1000u;
  control[2] = 0x1000u;
  control[4] = 0x1000u;
  control[5] = static_cast<std::uint32_t>(translationX);
  control[24] = 160u << 16;
  control[25] = 120u << 16;
  control[26] = 512u;
  control[29] = 0x155u;
  psx::present::writeGteControl(control);
}

// The executor takes the guest's registers, the GTE's among them, when it is first entered; enter it once, at a
// `jr ra` placed at `idle`, so the state a test sets before a draw is the state the draw sees.
inline void warmUpExecutor(Core &core, std::uint32_t idle) {
  core.mem_w32(idle, 0x03E00008u);
  core.mem_w32(idle + 4u, 0u);
  core.r[31] = 0x80010100u;
  core.r[29] = 0x801FFF00u;
  if (!psx::cpu::dispatchGuest(core, idle, psx::cpu::ExecutionBudget::fromCycles(1000u)).returned()) {
    std::abort();
  }
}

class CollectedSink final : public psx::present::PrimitiveSink {
public:
  void emit(psx::present::OtSlot slot, const psx::present::DrawPrimitive &primitive) override {
    psx::present::DrawPrimitive placed = primitive;
    placed.slot = slot;
    drawn.push_back(placed);
  }
  std::vector<psx::present::DrawPrimitive> drawn;
};

inline std::vector<psx::present::DrawPrimitive> primitivesOf(const psx::present::FrameRecord &record,
                                                             std::uint32_t producer) {
  std::vector<psx::present::DrawPrimitive> found;
  for (const psx::present::RecordEntry &entry : record.entries()) {
    const auto *primitive = std::get_if<psx::present::DrawPrimitive>(&entry);
    if (primitive != nullptr && primitive->key && primitive->key->producer == producer) {
      found.push_back(*primitive);
    }
  }
  return found;
}

// What a render owns of a primitive: the draw environment and the CLUT come from where the composer lands it.
inline psx::present::DrawPrimitive rendered(psx::present::DrawPrimitive primitive) {
  primitive.key.reset();
  primitive.sourceAddress = 0;
  primitive.clutOffset = psx::present::kNoClut;
  primitive.state = psx::present::RecordDrawState{.texPageX = primitive.state.texPageX,
                                                  .texPageY = primitive.state.texPageY,
                                                  .texMode = primitive.state.texMode,
                                                  .blendMode = primitive.state.blendMode};
  return primitive;
}

// Whether the render drew what the record holds, bucket by bucket, in the order the walk drew them.
inline bool sameFrame(std::vector<psx::present::DrawPrimitive> drawn,
                      const std::vector<psx::present::DrawPrimitive> &walked) {
  std::stable_sort(drawn.begin(), drawn.end(), [](const auto &a, const auto &b) {
    return a.slot->index > b.slot->index;
  });
  return drawn.size() == walked.size() &&
         std::equal(drawn.begin(), drawn.end(), walked.begin(), [](const auto &a, const auto &b) {
           return rendered(a) == rendered(b) && a.slot == b.slot;
         });
}

// A producer's render of the object `draw` writes, against the guest's own walk of it. `draw(shift)` runs the
// producer's frame with every vertex `shift` pixels over and returns the record the walk makes.
inline void checkStateRender(Core &core,
                             std::uint32_t producer,
                             std::uint32_t object,
                             const std::function<psx::present::FrameRecord(int)> &draw,
                             const std::function<void()> &scramble) {
  const psx::present::StateProducer *render = core.stateProducers.find(producer);
  CHECK(render != nullptr);
  const psx::present::ObjectId id{producer, object};

  const psx::present::FrameRecord record = draw(0);
  const psx::present::FrameState state = core.frameStates.collect(record);
  const auto saved = state.find(id);
  CHECK(saved.has_value());
  const std::vector<psx::present::DrawPrimitive> walked = primitivesOf(record, producer);
  CHECK(!walked.empty());
  CollectedSink exact;
  render->render(*saved, *saved, 1.0f, exact);
  CHECK(sameFrame(exact.drawn, walked));

  const auto composed = psx::present::composeFrame(record, nullptr, state, 1.0f, core.stateProducers);
  CHECK(composed.has_value());
  CHECK(composed.has_value() && sameFrame(primitivesOf(*composed, producer), walked));

  scramble();
  CollectedSink scrambled;
  render->render(*saved, *saved, 1.0f, scrambled);
  CHECK(sameFrame(scrambled.drawn, walked));

  const psx::present::FrameState earlier = core.frameStates.collect(draw(0));
  const psx::present::FrameState later = core.frameStates.collect(draw(8));
  const psx::present::FrameRecord midway = draw(4);
  const auto from = earlier.find(id);
  const auto to = later.find(id);
  CHECK(from.has_value() && to.has_value());
  if (from && to) {
    CollectedSink between;
    render->render(*from, *to, 0.5f, between);
    const std::vector<psx::present::DrawPrimitive> expected = primitivesOf(midway, producer);
    CHECK(sameFrame(between.drawn, expected));
    CollectedSink atEarlier;
    render->render(*from, *to, 0.0f, atEarlier);
    CHECK(!sameFrame(between.drawn, atEarlier.drawn));
  }
}

} // namespace crashbash::test
