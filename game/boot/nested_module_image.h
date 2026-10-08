#pragma once

#include "authenticated_module_image.h"

#include <cstdint>
#include <span>

class Core;

namespace crashbash {

// The nested gameplay overlays share load address 0x800B32B4 with the menu module, so publishing one
// retires the previous occupant's coverage. Holds the tracked specifications; never writes guest bytes.
class NestedModuleImage {
public:
  // Every tracked occupant of the nested slot, in manifest order.
  static std::span<const AuthenticatedModuleSpec> specs();
  static std::size_t specCount();

  static bool isNestedModuleRead(std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount);

  // Offers a completed CD read. `Unrelated`: no tracked module. `Published`: authenticated and bound.
  // `Rejected`: matched a specification, but the bytes are not that module.
  static ModuleImageReadResult
  offer(Core &core, std::uint32_t lba, std::uint32_t destination, std::uint32_t sectorCount);
};

} // namespace crashbash
