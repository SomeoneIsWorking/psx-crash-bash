#pragma once

#include "core.h"
#include "execution/guest_execution.h"
#include "execution_exit.h"
#include "execution_services.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace crashbash {

// A retail call site: where the call returns and the instructions the retail body spends reaching it.
struct MeasuredCallSite {
  std::uint32_t returnAddress;
  std::uint32_t instructionTicks;
};

// Sets up the guest ABI for a call (return address, argument registers, instruction timing) from the
// retail body's call site, without dispatching it; `psx::cpu::ResumableGuestCall` dispatches.
template <typename... Args> void measuredGuestCallSetup(Core &core, MeasuredCallSite site, Args... args) {
  static_assert(sizeof...(Args) <= 4);
  static_assert((std::is_convertible_v<Args, std::uint32_t> && ...));
  const std::uint32_t values[] = {static_cast<std::uint32_t>(args)..., 0u};
  for (std::size_t index = 0; index < sizeof...(Args); ++index) {
    core.r[4 + index] = values[index];
  }
  core.r[31] = site.returnAddress;
  psx::cpu::accountGuestInstructions(core, site.instructionTicks);
}

template <typename... Args>
std::uint32_t measuredGuestCall(
    Core &core, std::uint32_t target, std::uint32_t returnAddress, std::uint32_t instructionTicks, Args... args) {
  // `measuredGuestCallSetup` publishes `returnAddress` as r[31], which the shared call latches as its
  // return boundary.
  measuredGuestCallSetup(core, MeasuredCallSite{returnAddress, instructionTicks}, args...);
  return runtime::runGuestCallToReturn(core, target, returnAddress, "Crash Bash guest call", std::nullopt);
}

} // namespace crashbash
