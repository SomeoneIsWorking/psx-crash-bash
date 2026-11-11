// game/render/component_incarnation.h — which life of a render component a sample holds.
//
// The guest pools and rebuilds render components in place: a page flip re-initialises a UI layer's
// components, an effect respawns into a freed slot. Each copy of the render template 0x8005A830 into a
// component starts a new incarnation; the copiers are overridden here and count it, so a component's key
// changes exactly when the guest re-initialises it and never otherwise.
#pragma once

#include <cstdint>
#include <unordered_map>

class Core;

namespace crashbash::render {

class ComponentIncarnations {
public:
  // The guest copied the render template into `component`.
  void begin(std::uint32_t component);
  // How many times the guest has re-initialised `component`.
  [[nodiscard]] std::uint32_t generation(std::uint32_t component) const;

private:
  std::unordered_map<std::uint32_t, std::uint32_t> generations_; // main-RAM offset -> generation
};

// Overrides every copier of the render template so each re-initialisation begins an incarnation.
void registerComponentIncarnationOwners(Core &core);

} // namespace crashbash::render
