#pragma once

class Core;

namespace crashbash {

// Native owners for the two BOOT-overlay object callbacks that sample libetc VSync.
void registerBootObjectCallbackOverrides(Core &core);

} // namespace crashbash
