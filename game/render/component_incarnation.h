// game/render/component_incarnation.h — which life of a render component an object key names.
//
// The guest pools and rebuilds render components in place: a page flip re-initialises a UI layer's
// components, an effect respawns into a freed slot. Each copy of the render template 0x8005A830 into a
// component starts a new incarnation; the copiers are overridden here and count it, so the producer's
// object key changes exactly when the guest re-initialises the component and never otherwise.
#pragma once

#include <cstdint>
#include <unordered_map>

class Core;

namespace crashbash::render {

// The generation sits above the main-RAM offset, so it wraps after 2048 re-initialisations of one slot.
inline constexpr std::uint32_t kComponentOffsetBits = 21u;
inline constexpr std::uint32_t kComponentOffsetMask = (1u << kComponentOffsetBits) - 1u;

constexpr std::uint32_t incarnationObject(std::uint32_t component, std::uint32_t generation) {
  return (generation << kComponentOffsetBits) | (component & kComponentOffsetMask);
}

class ComponentIncarnations {
public:
  // The guest copied the render template into `component`.
  void begin(std::uint32_t component);
  // The object key for the component's current incarnation.
  std::uint32_t object(std::uint32_t component) const;

private:
  std::unordered_map<std::uint32_t, std::uint32_t> generations_; // main-RAM offset -> generation
};

// Overrides every copier of the render template so each re-initialisation begins an incarnation.
void registerComponentIncarnationOwners(Core &core);

} // namespace crashbash::render
