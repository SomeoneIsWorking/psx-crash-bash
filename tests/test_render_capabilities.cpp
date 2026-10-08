// Crash Bash presents through the record path with interpolation, and widens it through the canvas.
#include "guest_widescreen_projection.h"
#include "testutil.h"
#include "title_adapter.h"

namespace {

crashbash::TitleAdapter runtime;

void test_record_path_with_interpolation_is_the_only_picture() {
  const RenderCapabilities capabilities = runtime.renderCapabilities();
  CHECK(capabilities.defaultPath == RenderPath::Record);
  CHECK(capabilities.temporalInterpolation);
  CHECK(!capabilities.nativeRenderPath);
  CHECK(!capabilities.supports(RenderPath::Native));
  CHECK(render_path_resolve(RenderPath::Native, capabilities) == RenderPath::Record);
  CHECK(render_path_resolve(RenderPath::Record, capabilities) == RenderPath::Record);
}

void test_widescreen_widens_the_record_canvas_only_when_asked() {
  CHECK(runtime.guestWidescreenProjection() != nullptr);
  // The 512-wide gameplay display: 86 columns per side at 16:9, none at 4:3.
  const auto plan = [](PresentationAspect aspect) {
    return guest_projection_plan({
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
  RUN(record_path_with_interpolation_is_the_only_picture);
  RUN(widescreen_widens_the_record_canvas_only_when_asked);
  return pt_summary();
}
