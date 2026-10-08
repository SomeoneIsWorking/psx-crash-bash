#pragma once

#include "authenticated_module_image.h"

#include <cstdint>

class Core;

namespace crashbash {

using MenuImageReadResult = ModuleImageReadResult;

const AuthenticatedModuleSpec &menuImageSpec();
bool isMenuImageRead(std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount);
MenuImageReadResult
completeMenuImageRead(Core &core, std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount);

} // namespace crashbash
