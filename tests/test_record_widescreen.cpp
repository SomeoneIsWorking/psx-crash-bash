// Crash Bash presents through the record path with interpolation, and widens it through the canvas.
#include "guest_widescreen_projection.h"
#include "runtime/runtime.h"
#include "testutil.h"

namespace {

void test_the_record_path_is_interpolated() {
  const crashbash::Runtime runtime;
  CHECK(runtime.interpolates());
}

void test_widescreen_widens_the_record_canvas_only_when_asked() {
  const crashbash::Runtime runtime;
  CHECK(runtime.guestWidescreenProjection() != nullptr);
  // The 512-wide gameplay display: 86 columns per side at 16:9, none at 4:3.
  const auto plan = [](PresentationAspect aspect) {
    return psx::present::guestProjectionPlan({
        .path = RenderPath::Record,
        .requested = aspect,
        .nativePresentation = {512, 234},
        .nativeProjection = {.extent = {512, 234}, .drawWidth = 512},
        .sink = {1920, 1080},
        .vramWidth = 1024,
    });
  };
  const GuestProjectionPlan wide = plan(PresentationAspect::Wide16x9);
  const GuestProjectionPlan standard = plan(PresentationAspect::Standard4x3);
  CHECK(wide.widescreen());
  CHECK_EQ(wide.presentationExtent.width, 684);
  CHECK_EQ(wide.presentationHorizontalMargin, 86);
  CHECK_EQ(wide.guestDrawWidth, 512);
  CHECK(!standard.widescreen());
  CHECK_EQ(standard.presentationHorizontalMargin, 0);
}

} // namespace

int main() {
  RUN(the_record_path_is_interpolated);
  RUN(widescreen_widens_the_record_canvas_only_when_asked);
  return psx::test::summary();
}
