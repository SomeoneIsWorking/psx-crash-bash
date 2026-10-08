#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace crashbash::input {

// Sony digital-pad bits as the active-low word psxport's Pad consumes.
enum class PsxButton : std::uint16_t {
  Select = 0x0001,
  Start = 0x0008,
  Up = 0x0010,
  Right = 0x0020,
  Down = 0x0040,
  Left = 0x0080,
  L2 = 0x0100,
  R2 = 0x0200,
  L1 = 0x0400,
  R1 = 0x0800,
  Triangle = 0x1000,
  Circle = 0x2000,
  Cross = 0x4000,
  Square = 0x8000,
};

constexpr std::uint16_t buttonMask(PsxButton button) {
  return static_cast<std::uint16_t>(button);
}

struct TouchPoint {
  float x = 0.0F;
  float y = 0.0F;
};

struct TouchInsets {
  float left = 0.0F;
  float top = 0.0F;
  float right = 0.0F;
  float bottom = 0.0F;
};

struct TouchViewport {
  float width = 0.0F;
  float height = 0.0F;
  float pixelsPerDp = 1.0F;
  TouchInsets safeInsets{};
};

struct TouchRect {
  float left = 0.0F;
  float top = 0.0F;
  float width = 0.0F;
  float height = 0.0F;

  [[nodiscard]] TouchPoint center() const;
  [[nodiscard]] bool contains(TouchPoint point, float outset = 0.0F) const;
};

enum class TouchControl : std::uint8_t {
  DirectionPad,
  Triangle,
  Circle,
  Cross,
  Square,
  L1,
  L2,
  R1,
  R2,
  Select,
  Start,
  Count,
};

enum class TouchGlyph : std::uint8_t {
  DirectionPad,
  Triangle,
  Circle,
  Cross,
  Square,
  L1,
  L2,
  R1,
  R2,
  Select,
  Start,
};

enum class TouchShape : std::uint8_t {
  Circle,
  RoundedRect,
  DirectionPad,
};

struct TouchColor {
  std::uint8_t red = 255;
  std::uint8_t green = 255;
  std::uint8_t blue = 255;
  std::uint8_t alpha = 255;
};

struct TouchVisual {
  TouchControl control = TouchControl::DirectionPad;
  TouchGlyph glyph = TouchGlyph::DirectionPad;
  TouchShape shape = TouchShape::Circle;
  TouchRect bounds{};
  TouchColor color{};
  std::string_view label{};
  bool pressed = false;
};

// Normalized hand-authored icon vertices, scaled into TouchVisual::bounds; Circle is an analytic ring with no path.
[[nodiscard]] std::span<const TouchPoint> glyphPath(TouchGlyph glyph);

class CrashBashTouchControls {
public:
  static constexpr std::uint16_t kReleasedMask = 0xFFFFU;
  static constexpr std::size_t kControlCount = static_cast<std::size_t>(TouchControl::Count);
  static constexpr std::size_t kMaxTouches = 10;

  CrashBashTouchControls();

  // Rebuilds the landscape layout inside the safe-area insets; touch coordinates share this viewport's pixel space.
  void configure(TouchViewport viewport);
  void setEnabled(bool enabled);
  void setPhysicalControllerConnected(bool connected);

  [[nodiscard]] bool visible() const;
  [[nodiscard]] std::uint16_t activeLowMask() const;
  [[nodiscard]] std::span<const TouchVisual> visuals() const;

  // True only when a control captures the touch. Motion may slide between face buttons or direction-pad diagonals.
  bool touchDown(std::int64_t fingerId, TouchPoint point);
  bool touchMove(std::int64_t fingerId, TouchPoint point);
  bool touchUp(std::int64_t fingerId);
  bool touchCancel(std::int64_t fingerId);
  void cancelAll();

private:
  enum class TouchDomain : std::uint8_t {
    DirectionPad,
    Buttons,
  };

  struct TrackedTouch {
    std::int64_t fingerId = 0;
    std::uint16_t activeHighMask = 0;
    TouchDomain domain = TouchDomain::Buttons;
    bool active = false;
  };

  [[nodiscard]] std::uint16_t buttonsAt(TouchPoint point, TouchDomain domain) const;
  void rebuildLayout();
  void recomputeOutput();
  [[nodiscard]] TrackedTouch *findTouch(std::int64_t fingerId);
  [[nodiscard]] TrackedTouch *firstFreeTouch();

  TouchViewport viewport_{};
  std::array<TouchVisual, kControlCount> visuals_{};
  std::array<TrackedTouch, kMaxTouches> touches_{};
  std::uint16_t pressedHighMask_ = 0;
  bool enabled_ = true;
  bool physicalControllerConnected_ = false;
  bool layoutValid_ = false;
};

} // namespace crashbash::input
