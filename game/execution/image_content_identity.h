#pragma once

#include <cstddef>
#include <cstdint>
#include <lucent/content.h>

namespace crashbash {

// Compact content tag for ImageCatalog: the leading 64 bits of the SHA-256, which admission checks in full first.
inline std::uint64_t imageContentIdentity(const lucent::content::Sha256 &sha) {
  std::uint64_t prefix = 0u;
  for (std::size_t index = 0; index < sizeof(prefix); ++index) {
    prefix = (prefix << 8u) | sha[index];
  }
  return prefix;
}

} // namespace crashbash
