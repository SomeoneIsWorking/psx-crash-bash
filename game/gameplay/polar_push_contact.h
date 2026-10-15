#pragma once

#include <array>
#include <cstdint>

class Core;

namespace crashbash::polar {

struct EffectRecipe {
  std::int16_t directionOffset;
  std::int16_t lateralOffset;
  std::int16_t forwardOffset;
  std::int16_t verticalOffset;
  std::int16_t sideOffset;
  std::int16_t choreographyOffset;
  std::int16_t sineMultiplier;
  std::uint8_t sineShift;
};

// The three contact effects' motion offsets, by ordinal.
inline constexpr std::array<EffectRecipe, 3> kEffectRecipes{{
    {.directionOffset = -0x200,
     .lateralOffset = 0x200,
     .forwardOffset = -0x100,
     .verticalOffset = -0x20,
     .sideOffset = -0x80,
     .choreographyOffset = 0x100,
     .sineMultiplier = 17,
     .sineShift = 11},
    {.directionOffset = 0,
     .lateralOffset = 0x400,
     .forwardOffset = 0x100,
     .verticalOffset = -0x2A,
     .sideOffset = 0x80,
     .choreographyOffset = -0x100,
     .sineMultiplier = 11,
     .sineShift = 10},
    {.directionOffset = -0x400,
     .lateralOffset = 0,
     .forwardOffset = 0x100,
     .verticalOffset = -0x48,
     .sideOffset = -0x80,
     .choreographyOffset = 0x100,
     .sineMultiplier = 11,
     .sineShift = 10},
}};

// Contact predicates shared by the native owner and its tests; callers pass the low 32 bits of
// dx*dx + dz*dz, as the retail body does.
bool isWithinContactDisk(std::int32_t deltaX, std::int32_t deltaZ, std::uint32_t distanceSquared);
std::int32_t contactThreshold(std::uint16_t configurationValue);
bool crossesContactThreshold(std::uint16_t configurationValue, std::int32_t motionSpeed, std::int16_t motionLimit);

// Running totals of the contact traversal, per Core. Diagnostic only; decisions use guest state.
struct ContactCensus {
  std::uint64_t invocations = 0;
  std::uint64_t scanned = 0;
  std::uint64_t active = 0;
  std::uint64_t diskPass = 0;
  std::uint64_t retained = 0;
  std::uint64_t consumed = 0;
  std::uint64_t emittedEffects = 0;
};

void registerPolarPushContactOverride(Core &core);

} // namespace crashbash::polar
