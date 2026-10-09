#include "draw_globals.h"

#include "core.h"
#include "crashbash_guest.h"

namespace crashbash::render {

DrawGlobals readDrawGlobals(Core &core) {
  DrawGlobals globals;
  const std::uint32_t environment = core.mem_r32(guest::kDrawEnvironment);
  globals.hasEnvironment = environment != 0u ? 1u : 0u;
  if (environment != 0u) {
    globals.screenScale = static_cast<std::int16_t>(core.mem_r16(environment + guest::kEnvironmentScaleOffset));
  }
  globals.fade = core.mem_r32(guest::kScreenFade);
  globals.otBase = core.mem_r32(guest::kDrawOtBase);
  globals.zBias = static_cast<std::int16_t>(core.mem_r16(guest::kDrawZBias));
  globals.zLimit = static_cast<std::int16_t>(core.mem_r16(guest::kDrawZLimit));
  globals.originX = static_cast<std::int32_t>(core.mem_r32(guest::kDrawOriginX));
  globals.originY = static_cast<std::int32_t>(core.mem_r32(guest::kDrawOriginY));
  return globals;
}

} // namespace crashbash::render
