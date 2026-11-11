// component_motion.h — the render components the real draw drew, sampled at each draw section's entry and written
// blended into its replays: position, angles or rotation, scale and animation pose.
#pragma once

#include "motion_set.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

class Core;

namespace crashbash::render {

class ComponentIncarnations;

class ComponentMotion final : public psx::present::MotionSource {
public:
  explicit ComponentMotion(const ComponentIncarnations &incarnations) : incarnations_(incarnations) {}

  // How the draw placed a component, which says what of it a replay blends.
  enum class Kind : std::uint8_t {
    Model,  // 0x80019A60 or a root transform: its transform and pose
    Placed, // a panel or quad callback under the component's transform: its transform
    Panel,  // FUN_8001C690 in screen space: the panel rect of its +0x6C record
    Quad,   // FUN_8001C7FC in screen space: the four corners of its +0x6C record
    Text,   // FUN_8001C448: its transform, or in screen space its position as x|y
  };
  // The real draw drew `component` as `kind`; it is sampled from the next section entry on.
  void noteDrawn(std::uint32_t component, Kind kind);
  // The real draw begins: what it draws is noted afresh.
  void openDraw();

  [[nodiscard]] const char *name() const override {
    return "components";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;
  // Nothing sampled so far pairs with the next sample.
  void forget();

  // Words of a component a sample keeps, by offset from its start.
  static constexpr std::array<std::uint32_t, 23> kWordOffsets{
      0x04u, 0x08u, 0x0Cu,                                    // position
      0x10u, 0x14u,                                           // angles x|y, z|pad
      0x20u, 0x24u, 0x28u,                                    // scale
      0x30u, 0x34u, 0x38u, 0x3Cu, 0x40u, 0x44u, 0x48u, 0x4Cu, // MATRIX
      0x50u,                                                  // scale flag, rotation order, rebuild halfword
      0x70u,                                                  // pose: progress | frame
      0x00u,                                                  // flags
      0x74u,                                                  // frame code | shade
      0x78u, 0x7Cu, 0x84u,                                    // +0x6C record: panel y1, quad corners 2 and 3
  };

private:
  struct Record {
    std::uint32_t address = 0;
    std::uint32_t generation = 0; // ComponentIncarnations
    Kind kind = Kind::Model;
    std::uint32_t model = 0; // +0x6C
    std::array<std::uint32_t, kWordOffsets.size()> words{};
  };
  void writeRecord(psx::present::GuestRedraw &redraw, const Record &from, const Record &to, float t) const;
  // The component's transform: position, scale and its angles or MATRIX.
  void writeTransform(psx::present::GuestRedraw &redraw, const Record &from, const Record &to, float t) const;
  // The screen-space rect or corners of its +0x6C record.
  void writeScreenRecord(psx::present::GuestRedraw &redraw, const Record &from, const Record &to, float t) const;

  const ComponentIncarnations &incarnations_;
  std::vector<std::pair<std::uint32_t, Kind>> drawn_;
  std::vector<Record> earlier_;
  std::vector<Record> later_;
};

// Overrides the model draw 0x80019A60, the root transform 0x8001ACC0 and the text, panel and quad callbacks
// 0x8001C448, 0x8001C690 and 0x8001C7FC to note each component the real draw draws.
void registerDrawnComponentNotes(Core &core);

} // namespace crashbash::render
