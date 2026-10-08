#include "crashbash_touch_controls.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace crashbash::input {
namespace {

constexpr std::array<TouchPoint, 12> kDirectionPadPath{{
    {-0.34F, -1.0F},
    {0.34F, -1.0F},
    {0.34F, -0.34F},
    {1.0F, -0.34F},
    {1.0F, 0.34F},
    {0.34F, 0.34F},
    {0.34F, 1.0F},
    {-0.34F, 1.0F},
    {-0.34F, 0.34F},
    {-1.0F, 0.34F},
    {-1.0F, -0.34F},
    {-0.34F, -0.34F},
}};
constexpr std::array<TouchPoint, 3> kTrianglePath{{
    {0.0F, -0.82F},
    {0.74F, 0.58F},
    {-0.74F, 0.58F},
}};
constexpr std::array<TouchPoint, 12> kCrossPath{{
    {-0.70F, -0.42F},
    {-0.42F, -0.70F},
    {0.0F, -0.28F},
    {0.42F, -0.70F},
    {0.70F, -0.42F},
    {0.28F, 0.0F},
    {0.70F, 0.42F},
    {0.42F, 0.70F},
    {0.0F, 0.28F},
    {-0.42F, 0.70F},
    {-0.70F, 0.42F},
    {-0.28F, 0.0F},
}};
constexpr std::array<TouchPoint, 4> kSquarePath{{
    {-0.66F, -0.66F},
    {0.66F, -0.66F},
    {0.66F, 0.66F},
    {-0.66F, 0.66F},
}};
constexpr std::array<TouchPoint, 3> kStartPath{{
    {-0.50F, -0.68F},
    {0.70F, 0.0F},
    {-0.50F, 0.68F},
}};
constexpr std::array<TouchPoint, 4> kSelectPath{{
    {-0.72F, -0.30F},
    {0.72F, -0.30F},
    {0.72F, 0.30F},
    {-0.72F, 0.30F},
}};

constexpr std::size_t controlIndex(TouchControl control) {
  return static_cast<std::size_t>(control);
}

constexpr TouchColor kNeutralColor{228, 232, 238, 164};
constexpr TouchColor kTriangleColor{75, 210, 126, 190};
constexpr TouchColor kCircleColor{239, 91, 97, 190};
constexpr TouchColor kCrossColor{80, 151, 241, 190};
constexpr TouchColor kSquareColor{230, 103, 178, 190};

TouchRect centeredRect(TouchPoint center, float width, float height) {
  return {
      .left = center.x - width * 0.5F,
      .top = center.y - height * 0.5F,
      .width = width,
      .height = height,
  };
}

float sanitizedInset(float inset, float extent) {
  return std::clamp(inset, 0.0F, std::max(extent, 0.0F));
}

} // namespace

TouchPoint TouchRect::center() const {
  return {left + width * 0.5F, top + height * 0.5F};
}

bool TouchRect::contains(TouchPoint point, float outset) const {
  return point.x >= left - outset && point.x <= left + width + outset && point.y >= top - outset &&
         point.y <= top + height + outset;
}

std::span<const TouchPoint> glyphPath(TouchGlyph glyph) {
  switch (glyph) {
  case TouchGlyph::DirectionPad:
    return kDirectionPadPath;
  case TouchGlyph::Triangle:
    return kTrianglePath;
  case TouchGlyph::Cross:
    return kCrossPath;
  case TouchGlyph::Square:
    return kSquarePath;
  case TouchGlyph::Start:
    return kStartPath;
  case TouchGlyph::Select:
    return kSelectPath;
  case TouchGlyph::Circle:
  case TouchGlyph::L1:
  case TouchGlyph::L2:
  case TouchGlyph::R1:
  case TouchGlyph::R2:
    return {};
  }
  return {};
}

CrashBashTouchControls::CrashBashTouchControls() {
  rebuildLayout();
}

void CrashBashTouchControls::configure(TouchViewport viewport) {
  cancelAll();
  viewport.width = std::max(viewport.width, 0.0F);
  viewport.height = std::max(viewport.height, 0.0F);
  viewport.pixelsPerDp = std::max(viewport.pixelsPerDp, 0.5F);
  viewport.safeInsets.left = sanitizedInset(viewport.safeInsets.left, viewport.width);
  viewport.safeInsets.right = sanitizedInset(viewport.safeInsets.right, viewport.width);
  viewport.safeInsets.top = sanitizedInset(viewport.safeInsets.top, viewport.height);
  viewport.safeInsets.bottom = sanitizedInset(viewport.safeInsets.bottom, viewport.height);
  viewport_ = viewport;
  rebuildLayout();
}

void CrashBashTouchControls::setEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;
  cancelAll();
}

void CrashBashTouchControls::setPhysicalControllerConnected(bool connected) {
  if (physicalControllerConnected_ == connected) {
    return;
  }
  physicalControllerConnected_ = connected;
  cancelAll();
}

bool CrashBashTouchControls::visible() const {
  return enabled_ && !physicalControllerConnected_ && layoutValid_;
}

std::uint16_t CrashBashTouchControls::activeLowMask() const {
  return static_cast<std::uint16_t>(~pressedHighMask_);
}

std::span<const TouchVisual> CrashBashTouchControls::visuals() const {
  return visuals_;
}

bool CrashBashTouchControls::touchDown(std::int64_t fingerId, TouchPoint point) {
  if (!visible()) {
    return false;
  }
  if (TrackedTouch *existing = findTouch(fingerId)) {
    existing->activeHighMask = buttonsAt(point, existing->domain);
    recomputeOutput();
    return true;
  }

  const bool directionPadCaptured = visuals_[controlIndex(TouchControl::DirectionPad)].bounds.contains(point);
  const TouchDomain domain = directionPadCaptured ? TouchDomain::DirectionPad : TouchDomain::Buttons;
  const std::uint16_t buttons = buttonsAt(point, domain);
  if (buttons == 0 && !directionPadCaptured) {
    return false;
  }
  TrackedTouch *touch = firstFreeTouch();
  if (touch == nullptr) {
    return false;
  }
  *touch = {
      .fingerId = fingerId,
      .activeHighMask = buttons,
      .domain = domain,
      .active = true,
  };
  recomputeOutput();
  return true;
}

bool CrashBashTouchControls::touchMove(std::int64_t fingerId, TouchPoint point) {
  TrackedTouch *touch = findTouch(fingerId);
  if (touch == nullptr) {
    return false;
  }
  touch->activeHighMask = visible() ? buttonsAt(point, touch->domain) : 0;
  recomputeOutput();
  return true;
}

bool CrashBashTouchControls::touchUp(std::int64_t fingerId) {
  TrackedTouch *touch = findTouch(fingerId);
  if (touch == nullptr) {
    return false;
  }
  *touch = {};
  recomputeOutput();
  return true;
}

bool CrashBashTouchControls::touchCancel(std::int64_t fingerId) {
  return touchUp(fingerId);
}

void CrashBashTouchControls::cancelAll() {
  for (TrackedTouch &touch : touches_) {
    touch = {};
  }
  recomputeOutput();
}

std::uint16_t CrashBashTouchControls::buttonsAt(TouchPoint point, TouchDomain domain) const {
  if (!layoutValid_) {
    return 0;
  }

  const TouchRect &directionBounds = visuals_[controlIndex(TouchControl::DirectionPad)].bounds;
  if (domain == TouchDomain::DirectionPad && directionBounds.contains(point)) {
    const TouchPoint center = directionBounds.center();
    const float halfExtent = directionBounds.width * 0.5F;
    const float x = (point.x - center.x) / halfExtent;
    const float y = (point.y - center.y) / halfExtent;
    const float radiusSquared = x * x + y * y;
    if (radiusSquared <= 1.12F && radiusSquared >= 0.035F) {
      constexpr float kAxisThreshold = 0.24F;
      std::uint16_t buttons = 0;
      if (x <= -kAxisThreshold) {
        buttons |= buttonMask(PsxButton::Left);
      } else if (x >= kAxisThreshold) {
        buttons |= buttonMask(PsxButton::Right);
      }
      if (y <= -kAxisThreshold) {
        buttons |= buttonMask(PsxButton::Up);
      } else if (y >= kAxisThreshold) {
        buttons |= buttonMask(PsxButton::Down);
      }
      return buttons;
    }
    return 0;
  }
  if (domain == TouchDomain::DirectionPad) {
    return 0;
  }

  // Expanded hit regions resolve to the closest visual so overlapping slop cannot make a two-button chord.
  const TouchVisual *best = nullptr;
  float bestDistanceSquared = std::numeric_limits<float>::max();
  const float hitOutset = directionBounds.width * 0.07F;
  for (std::size_t index = 1; index < visuals_.size(); ++index) {
    const TouchVisual &visual = visuals_[index];
    if (!visual.bounds.contains(point, hitOutset)) {
      continue;
    }
    const TouchPoint center = visual.bounds.center();
    const float dx = point.x - center.x;
    const float dy = point.y - center.y;
    const float distanceSquared = dx * dx + dy * dy;
    if (distanceSquared < bestDistanceSquared) {
      best = &visual;
      bestDistanceSquared = distanceSquared;
    }
  }
  if (best == nullptr) {
    return 0;
  }

  switch (best->control) {
  case TouchControl::Triangle:
    return buttonMask(PsxButton::Triangle);
  case TouchControl::Circle:
    return buttonMask(PsxButton::Circle);
  case TouchControl::Cross:
    return buttonMask(PsxButton::Cross);
  case TouchControl::Square:
    return buttonMask(PsxButton::Square);
  case TouchControl::L1:
    return buttonMask(PsxButton::L1);
  case TouchControl::L2:
    return buttonMask(PsxButton::L2);
  case TouchControl::R1:
    return buttonMask(PsxButton::R1);
  case TouchControl::R2:
    return buttonMask(PsxButton::R2);
  case TouchControl::Select:
    return buttonMask(PsxButton::Select);
  case TouchControl::Start:
    return buttonMask(PsxButton::Start);
  case TouchControl::DirectionPad:
  case TouchControl::Count:
    return 0;
  }
  return 0;
}

void CrashBashTouchControls::rebuildLayout() {
  const float left = viewport_.safeInsets.left;
  const float top = viewport_.safeInsets.top;
  const float right = viewport_.width - viewport_.safeInsets.right;
  const float bottom = viewport_.height - viewport_.safeInsets.bottom;
  const float usableWidth = right - left;
  const float usableHeight = bottom - top;
  layoutValid_ = usableWidth >= 320.0F && usableHeight >= 180.0F;
  if (!layoutValid_) {
    visuals_ = {};
    return;
  }

  const float scale =
      std::clamp(std::min({viewport_.pixelsPerDp, usableWidth / 640.0F, usableHeight / 360.0F}), 0.5F, 3.0F);
  const float dpadSize = 156.0F * scale;
  const float edgeMargin = 22.0F * scale;
  const TouchPoint dpadCenter{
      left + edgeMargin + dpadSize * 0.5F,
      bottom - edgeMargin - dpadSize * 0.5F,
  };
  visuals_[controlIndex(TouchControl::DirectionPad)] = {
      .control = TouchControl::DirectionPad,
      .glyph = TouchGlyph::DirectionPad,
      .shape = TouchShape::DirectionPad,
      .bounds = centeredRect(dpadCenter, dpadSize, dpadSize),
      .color = kNeutralColor,
  };

  const float faceDiameter = 58.0F * scale;
  const float faceOffset = 48.0F * scale;
  const TouchPoint faceCenter{
      right - edgeMargin - faceDiameter * 0.5F - faceOffset,
      bottom - edgeMargin - faceDiameter * 0.5F - faceOffset,
  };
  visuals_[controlIndex(TouchControl::Triangle)] = {
      .control = TouchControl::Triangle,
      .glyph = TouchGlyph::Triangle,
      .shape = TouchShape::Circle,
      .bounds = centeredRect({faceCenter.x, faceCenter.y - faceOffset}, faceDiameter, faceDiameter),
      .color = kTriangleColor,
  };
  visuals_[controlIndex(TouchControl::Circle)] = {
      .control = TouchControl::Circle,
      .glyph = TouchGlyph::Circle,
      .shape = TouchShape::Circle,
      .bounds = centeredRect({faceCenter.x + faceOffset, faceCenter.y}, faceDiameter, faceDiameter),
      .color = kCircleColor,
  };
  visuals_[controlIndex(TouchControl::Cross)] = {
      .control = TouchControl::Cross,
      .glyph = TouchGlyph::Cross,
      .shape = TouchShape::Circle,
      .bounds = centeredRect({faceCenter.x, faceCenter.y + faceOffset}, faceDiameter, faceDiameter),
      .color = kCrossColor,
  };
  visuals_[controlIndex(TouchControl::Square)] = {
      .control = TouchControl::Square,
      .glyph = TouchGlyph::Square,
      .shape = TouchShape::Circle,
      .bounds = centeredRect({faceCenter.x - faceOffset, faceCenter.y}, faceDiameter, faceDiameter),
      .color = kSquareColor,
  };

  const float shoulderWidth = 74.0F * scale;
  const float shoulderHeight = 40.0F * scale;
  const float shoulderGap = 10.0F * scale;
  const float shoulderY = top + edgeMargin + shoulderHeight * 0.5F;
  const float leftShoulder1X = left + edgeMargin + shoulderWidth * 0.5F;
  const float leftShoulder2X = leftShoulder1X + shoulderWidth + shoulderGap;
  const float rightShoulder1X = right - edgeMargin - shoulderWidth * 0.5F;
  const float rightShoulder2X = rightShoulder1X - shoulderWidth - shoulderGap;
  visuals_[controlIndex(TouchControl::L1)] = {
      .control = TouchControl::L1,
      .glyph = TouchGlyph::L1,
      .shape = TouchShape::RoundedRect,
      .bounds = centeredRect({leftShoulder1X, shoulderY}, shoulderWidth, shoulderHeight),
      .color = kNeutralColor,
      .label = "L1",
  };
  visuals_[controlIndex(TouchControl::L2)] = {
      .control = TouchControl::L2,
      .glyph = TouchGlyph::L2,
      .shape = TouchShape::RoundedRect,
      .bounds = centeredRect({leftShoulder2X, shoulderY}, shoulderWidth, shoulderHeight),
      .color = kNeutralColor,
      .label = "L2",
  };
  visuals_[controlIndex(TouchControl::R1)] = {
      .control = TouchControl::R1,
      .glyph = TouchGlyph::R1,
      .shape = TouchShape::RoundedRect,
      .bounds = centeredRect({rightShoulder1X, shoulderY}, shoulderWidth, shoulderHeight),
      .color = kNeutralColor,
      .label = "R1",
  };
  visuals_[controlIndex(TouchControl::R2)] = {
      .control = TouchControl::R2,
      .glyph = TouchGlyph::R2,
      .shape = TouchShape::RoundedRect,
      .bounds = centeredRect({rightShoulder2X, shoulderY}, shoulderWidth, shoulderHeight),
      .color = kNeutralColor,
      .label = "R2",
  };

  const float utilityWidth = 72.0F * scale;
  const float utilityHeight = 34.0F * scale;
  const float utilityGap = 12.0F * scale;
  const float utilityCenterX = (left + right) * 0.5F;
  const float utilityY = top + edgeMargin + utilityHeight * 0.5F;
  visuals_[controlIndex(TouchControl::Select)] = {
      .control = TouchControl::Select,
      .glyph = TouchGlyph::Select,
      .shape = TouchShape::RoundedRect,
      .bounds =
          centeredRect({utilityCenterX - (utilityWidth + utilityGap) * 0.5F, utilityY}, utilityWidth, utilityHeight),
      .color = kNeutralColor,
      .label = "SELECT",
  };
  visuals_[controlIndex(TouchControl::Start)] = {
      .control = TouchControl::Start,
      .glyph = TouchGlyph::Start,
      .shape = TouchShape::RoundedRect,
      .bounds =
          centeredRect({utilityCenterX + (utilityWidth + utilityGap) * 0.5F, utilityY}, utilityWidth, utilityHeight),
      .color = kNeutralColor,
      .label = "START",
  };
  recomputeOutput();
}

void CrashBashTouchControls::recomputeOutput() {
  pressedHighMask_ = 0;
  for (const TrackedTouch &touch : touches_) {
    if (touch.active) {
      pressedHighMask_ |= touch.activeHighMask;
    }
  }
  constexpr std::uint16_t kDirectionMask = buttonMask(PsxButton::Up) | buttonMask(PsxButton::Right) |
                                           buttonMask(PsxButton::Down) | buttonMask(PsxButton::Left);
  for (TouchVisual &visual : visuals_) {
    std::uint16_t mask = 0;
    switch (visual.control) {
    case TouchControl::DirectionPad:
      mask = kDirectionMask;
      break;
    case TouchControl::Triangle:
      mask = buttonMask(PsxButton::Triangle);
      break;
    case TouchControl::Circle:
      mask = buttonMask(PsxButton::Circle);
      break;
    case TouchControl::Cross:
      mask = buttonMask(PsxButton::Cross);
      break;
    case TouchControl::Square:
      mask = buttonMask(PsxButton::Square);
      break;
    case TouchControl::L1:
      mask = buttonMask(PsxButton::L1);
      break;
    case TouchControl::L2:
      mask = buttonMask(PsxButton::L2);
      break;
    case TouchControl::R1:
      mask = buttonMask(PsxButton::R1);
      break;
    case TouchControl::R2:
      mask = buttonMask(PsxButton::R2);
      break;
    case TouchControl::Select:
      mask = buttonMask(PsxButton::Select);
      break;
    case TouchControl::Start:
      mask = buttonMask(PsxButton::Start);
      break;
    case TouchControl::Count:
      break;
    }
    visual.pressed = (pressedHighMask_ & mask) != 0;
  }
}

CrashBashTouchControls::TrackedTouch *CrashBashTouchControls::findTouch(std::int64_t fingerId) {
  for (TrackedTouch &touch : touches_) {
    if (touch.active && touch.fingerId == fingerId) {
      return &touch;
    }
  }
  return nullptr;
}

CrashBashTouchControls::TrackedTouch *CrashBashTouchControls::firstFreeTouch() {
  for (TrackedTouch &touch : touches_) {
    if (!touch.active) {
      return &touch;
    }
  }
  return nullptr;
}

} // namespace crashbash::input
