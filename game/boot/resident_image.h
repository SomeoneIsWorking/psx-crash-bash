#pragma once

#include "psx_exe_image.h"

#include <cstddef>
#include <cstdint>
#include <span>

class Core;

namespace crashbash {

// Authenticate the Crash Bash USA executable against the title manifest before publishing its code residency.
psx::cpu::PsxExeLoadResult loadResidentImage(Core &core, std::span<const std::uint8_t> bytes);

} // namespace crashbash