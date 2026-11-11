// component_motion.h — the render components the real draw drew, sampled at each draw section's entry and written
// blended into its replays: position, angles or rotation, scale and animation pose.
#pragma once

#include "motion_set.h"

#include <array>
#include <cstdint>
#include <vector>

class Core;

namespace crashbash::render {

class ComponentIncarnations;

class ComponentMotion final : public psx::present::MotionSource {
public:
  explicit ComponentMotion(const ComponentIncarnations &incarnations) : incarnations_(incarnations) {}

  // The real draw drew `component` (0x80019A60's a3); it is sampled from the next section entry on.
  void noteDrawn(std::uint32_t component);
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
  static constexpr std::array<std::uint32_t, 20> kWordOffsets{
      0x04u, 0x08u, 0x0Cu,                                    // position
      0x10u, 0x14u,                                           // angles x|y, z|pad
      0x20u, 0x24u, 0x28u,                                    // scale
      0x30u, 0x34u, 0x38u, 0x3Cu, 0x40u, 0x44u, 0x48u, 0x4Cu, // MATRIX
      0x50u,                                                  // scale flag, rotation order, rebuild halfword
      0x70u,                                                  // pose: progress | frame
      0x00u,                                                  // flags
      0x74u,                                                  // frame code | shade
  };

private:
  struct Record {
    std::uint32_t address = 0;
    std::uint32_t object = 0; // incarnation key
    std::uint32_t model = 0;  // +0x6C
    std::array<std::uint32_t, kWordOffsets.size()> words{};
  };
  void writeRecord(psx::present::GuestRedraw &redraw, const Record &from, const Record &to, float t) const;

  const ComponentIncarnations &incarnations_;
  std::vector<std::uint32_t> drawn_;
  std::vector<Record> earlier_;
  std::vector<Record> later_;
};

// Overrides the model draw 0x80019A60 and the root transform 0x8001ACC0 to note each component the real draw draws.
void registerDrawnComponentNotes(Core &core);

} // namespace crashbash::render
