#pragma once

class Core;

namespace crashbash {

// Installs the title's native behaviour set into one runtime instance.
void registerNativeOwners(Core &core);

} // namespace crashbash
