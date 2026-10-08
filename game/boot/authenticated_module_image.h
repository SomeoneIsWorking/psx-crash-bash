#pragma once

#include "guest_execution.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

class Core;

namespace crashbash {

struct AuthenticatedModuleSpec {
  runtime::GuestImage image;
  std::string_view name;
  std::uint32_t discLba;
  std::uint32_t loadAddress;
  std::uint32_t sectorCount;
  std::size_t payloadBytes;
  std::string_view sha256;
  std::uint32_t entryPointerOffset;
  std::uint32_t entry;
};

// Only BOOT and MENU carry an entry word; the nested overlays have ASCII name-table headers, so for them the
// payload SHA-256 is the sole witness. Zero `entry` means absent.
constexpr bool hasEntryWitness(const AuthenticatedModuleSpec &spec) {
  return spec.entry != 0u;
}

enum class ModuleImageReadResult { Unrelated, Published, Rejected };

constexpr bool validModuleSpec(const AuthenticatedModuleSpec &spec) {
  constexpr std::size_t kSectorBytes = 2048u;
  constexpr std::uint32_t kRamBytes = 0x200000u;
  const std::uint32_t physical = spec.loadAddress & 0x1fffffffu;
  const std::uint32_t physicalEntry = spec.entry & 0x1fffffffu;
  return !spec.name.empty() && spec.sectorCount > 0u && spec.payloadBytes == spec.sectorCount * kSectorBytes &&
         physical < kRamBytes && spec.payloadBytes <= kRamBytes - physical &&
         spec.payloadBytes >= sizeof(std::uint32_t) && spec.sha256.size() == 64u &&
         (!hasEntryWitness(spec) || (spec.entryPointerOffset <= spec.payloadBytes - sizeof(std::uint32_t) &&
                                     physicalEntry >= physical && physicalEntry < physical + spec.payloadBytes));
}

bool isModuleImageRead(const AuthenticatedModuleSpec &spec,
                       std::uint32_t lba,
                       std::uint32_t destination,
                       std::uint32_t sectorCount);
ModuleImageReadResult completeModuleImageRead(Core &core,
                                              const AuthenticatedModuleSpec &spec,
                                              std::uint32_t lba,
                                              std::uint32_t destination,
                                              std::uint32_t sectorCount);

} // namespace crashbash
