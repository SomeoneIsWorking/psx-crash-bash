#include "crashbash_touch_controls.h"

#include <cstdlib>
#include <iostream>

namespace {

using crashbash::input::buttonMask;
using crashbash::input::CrashBashTouchControls;
using crashbash::input::glyphPath;
using crashbash::input::PsxButton;
using crashbash::input::TouchControl;
using crashbash::input::TouchPoint;
using crashbash::input::TouchViewport;

#define CHECK(condition)                                                                                               \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      std::cerr << "CHECK failed at " << __FILE__ << ':' << __LINE__ << ": " #condition << '\n';                       \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (false)

const crashbash::input::TouchVisual &visual(const CrashBashTouchControls &controls, TouchControl control) {
  return controls.visuals()[static_cast<std::size_t>(control)];
}

bool pressed(std::uint16_t activeLowMask, PsxButton button) {
  return (activeLowMask & buttonMask(button)) == 0;
}

CrashBashTouchControls configuredControls() {
  CrashBashTouchControls controls;
  controls.configure(TouchViewport{
      .width = 1280.0F,
      .height = 720.0F,
      .pixelsPerDp = 2.0F,
      .safeInsets = {.left = 42.0F, .top = 18.0F, .right = 34.0F, .bottom = 24.0F},
  });
  return controls;
}

void test_layout_stays_inside_safe_area() {
  const CrashBashTouchControls controls = configuredControls();
  CHECK(controls.visible());
  CHECK(controls.visuals().size() == CrashBashTouchControls::kControlCount);
  for (const auto &item : controls.visuals()) {
    CHECK(item.bounds.left >= 42.0F);
    CHECK(item.bounds.top >= 18.0F);
    CHECK(item.bounds.left + item.bounds.width <= 1246.0F);
    CHECK(item.bounds.top + item.bounds.height <= 696.0F);
  }
}

void test_direction_pad_supports_diagonal_and_dead_zone() {
  CrashBashTouchControls controls = configuredControls();
  const auto bounds = visual(controls, TouchControl::DirectionPad).bounds;
  const TouchPoint center = bounds.center();
  const float offset = bounds.width * 0.32F;

  CHECK(controls.touchDown(1, center));
  CHECK(controls.activeLowMask() == CrashBashTouchControls::kReleasedMask);
  CHECK(controls.touchMove(1, {center.x + offset, center.y - offset}));
  CHECK(pressed(controls.activeLowMask(), PsxButton::Right));
  CHECK(pressed(controls.activeLowMask(), PsxButton::Up));
  CHECK(!pressed(controls.activeLowMask(), PsxButton::Left));
  CHECK(!pressed(controls.activeLowMask(), PsxButton::Down));
  CHECK(controls.touchMove(1, visual(controls, TouchControl::Cross).bounds.center()));
  CHECK(controls.activeLowMask() == CrashBashTouchControls::kReleasedMask);
  CHECK(controls.touchUp(1));
  CHECK(!controls.touchDown(2, {640.0F, 360.0F}));
}

void test_every_authored_button_maps_to_its_psx_bit() {
  struct ExpectedMapping {
    TouchControl control;
    PsxButton button;
  };
  constexpr ExpectedMapping kMappings[] = {
      {TouchControl::Triangle, PsxButton::Triangle},
      {TouchControl::Circle, PsxButton::Circle},
      {TouchControl::Cross, PsxButton::Cross},
      {TouchControl::Square, PsxButton::Square},
      {TouchControl::L1, PsxButton::L1},
      {TouchControl::L2, PsxButton::L2},
      {TouchControl::R1, PsxButton::R1},
      {TouchControl::R2, PsxButton::R2},
      {TouchControl::Select, PsxButton::Select},
      {TouchControl::Start, PsxButton::Start},
  };

  CrashBashTouchControls controls = configuredControls();
  std::int64_t fingerId = 100;
  for (const ExpectedMapping &mapping : kMappings) {
    CHECK(controls.touchDown(fingerId, visual(controls, mapping.control).bounds.center()));
    CHECK(pressed(controls.activeLowMask(), mapping.button));
    CHECK(controls.touchUp(fingerId));
    CHECK(controls.activeLowMask() == CrashBashTouchControls::kReleasedMask);
    ++fingerId;
  }
}

void test_multitouch_uses_the_shared_active_low_pad_contract() {
  CrashBashTouchControls controls = configuredControls();
  const TouchPoint cross = visual(controls, TouchControl::Cross).bounds.center();
  const TouchPoint square = visual(controls, TouchControl::Square).bounds.center();

  CHECK(controls.touchDown(10, cross));
  CHECK(controls.touchDown(11, square));
  CHECK(pressed(controls.activeLowMask(), PsxButton::Cross));
  CHECK(pressed(controls.activeLowMask(), PsxButton::Square));
  CHECK(visual(controls, TouchControl::Cross).pressed);
  CHECK(visual(controls, TouchControl::Square).pressed);

  CHECK(controls.touchUp(10));
  CHECK(!pressed(controls.activeLowMask(), PsxButton::Cross));
  CHECK(pressed(controls.activeLowMask(), PsxButton::Square));
  CHECK(controls.touchCancel(11));
  CHECK(controls.activeLowMask() == CrashBashTouchControls::kReleasedMask);
}

void test_motion_can_slide_between_face_buttons_without_a_stuck_press() {
  CrashBashTouchControls controls = configuredControls();
  CHECK(controls.touchDown(20, visual(controls, TouchControl::Cross).bounds.center()));
  CHECK(pressed(controls.activeLowMask(), PsxButton::Cross));

  CHECK(controls.touchMove(20, visual(controls, TouchControl::Circle).bounds.center()));
  CHECK(!pressed(controls.activeLowMask(), PsxButton::Cross));
  CHECK(pressed(controls.activeLowMask(), PsxButton::Circle));

  CHECK(controls.touchMove(20, {640.0F, 360.0F}));
  CHECK(controls.activeLowMask() == CrashBashTouchControls::kReleasedMask);
  CHECK(controls.touchUp(20));
}

void test_physical_controller_hides_and_releases_touch_input() {
  CrashBashTouchControls controls = configuredControls();
  CHECK(controls.touchDown(30, visual(controls, TouchControl::Start).bounds.center()));
  CHECK(pressed(controls.activeLowMask(), PsxButton::Start));

  controls.setPhysicalControllerConnected(true);
  CHECK(!controls.visible());
  CHECK(controls.activeLowMask() == CrashBashTouchControls::kReleasedMask);
  CHECK(!controls.touchDown(31, visual(controls, TouchControl::Cross).bounds.center()));

  controls.setPhysicalControllerConnected(false);
  CHECK(controls.visible());
  CHECK(controls.touchDown(32, visual(controls, TouchControl::Cross).bounds.center()));
  controls.setEnabled(false);
  CHECK(!controls.visible());
  CHECK(controls.activeLowMask() == CrashBashTouchControls::kReleasedMask);
}

void test_invalid_or_changed_layout_cannot_leave_buttons_held() {
  CrashBashTouchControls controls = configuredControls();
  CHECK(controls.touchDown(40, visual(controls, TouchControl::L1).bounds.center()));
  CHECK(pressed(controls.activeLowMask(), PsxButton::L1));

  controls.configure(TouchViewport{.width = 200.0F, .height = 100.0F, .pixelsPerDp = 2.0F});
  CHECK(!controls.visible());
  CHECK(controls.activeLowMask() == CrashBashTouchControls::kReleasedMask);
  CHECK(!controls.touchMove(40, {10.0F, 10.0F}));
}

void test_hand_authored_vector_glyphs_are_available() {
  using crashbash::input::TouchGlyph;
  CHECK(glyphPath(TouchGlyph::DirectionPad).size() == 12);
  CHECK(glyphPath(TouchGlyph::Triangle).size() == 3);
  CHECK(glyphPath(TouchGlyph::Cross).size() == 12);
  CHECK(glyphPath(TouchGlyph::Square).size() == 4);
  CHECK(glyphPath(TouchGlyph::Circle).empty()); // rendered as an analytic ring
  CHECK(glyphPath(TouchGlyph::Start).size() == 3);
  CHECK(glyphPath(TouchGlyph::Select).size() == 4);
}

} // namespace

int main() {
  test_layout_stays_inside_safe_area();
  test_direction_pad_supports_diagonal_and_dead_zone();
  test_every_authored_button_maps_to_its_psx_bit();
  test_multitouch_uses_the_shared_active_low_pad_contract();
  test_motion_can_slide_between_face_buttons_without_a_stuck_press();
  test_physical_controller_hides_and_releases_touch_input();
  test_invalid_or_changed_layout_cannot_leave_buttons_held();
  test_hand_authored_vector_glyphs_are_available();
  std::cout << "crashbash touch controls: 8/8 tests passed\n";
  return 0;
}
