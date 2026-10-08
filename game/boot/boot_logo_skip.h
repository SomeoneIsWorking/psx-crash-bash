#pragma once

class Core;

namespace crashbash {

// Adds the PC port's Start-or-Cross skip behavior to the BOOT logo controller, scheduling the same scene
// handoff the retail controller reaches naturally without changing logo phases or timers.
void registerBootLogoSkipOverride(Core &core);

} // namespace crashbash
