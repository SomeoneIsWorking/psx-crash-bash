#pragma once

#include "authenticated_module_image.h"

#include <cstdint>

class Core;

namespace crashbash {

using BootImageReadResult = ModuleImageReadResult;

bool isBootImageRead(std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount);
BootImageReadResult
completeBootImageRead(Core &core, std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount);

} // namespace crashbash
