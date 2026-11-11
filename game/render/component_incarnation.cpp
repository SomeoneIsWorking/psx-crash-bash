#include "render/component_incarnation.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "ordering_table.h"
#include "runtime/session.h"
#include "title/guest.h"

#include <cstdint>

namespace crashbash::render {
namespace {

using runtime::GuestImage;

// A replay's copies are rolled back with it, so they begin no life.
void beginLife(Core &core, std::uint32_t component) {
  Session &title = session(core);
  if (!title.drawReplay.frame().replaying()) {
    title.componentIncarnations.begin(component);
  }
}

void beginConsecutive(Core &core, std::uint32_t first, std::uint32_t count) {
  for (std::uint32_t i = 0; i < count; ++i) {
    beginLife(core, first + i * guest::kRenderComponentBytes);
  }
}

// The pool allocators hand out the pool's first v0 components in list order.
void poolAllocation(Core *core, std::uint32_t allocator) {
  std::uint32_t component = core->mem_r32(guest::kRenderComponentPool);
  runtime::callOriginal(*core, GuestImage::Resident, allocator);
  const std::uint32_t taken = core->r[2];
  for (std::uint32_t i = 0; i < taken && component != 0u; ++i) {
    beginLife(*core, component);
    component = core->mem_r32(component + guest::kRenderComponentNext);
  }
}

void entityComponentAlloc(Core *core) {
  poolAllocation(core, guest::kEntityComponentAlloc);
}

void listComponentAlloc(Core *core) {
  poolAllocation(core, guest::kListComponentAlloc);
}

void bufferComponentInit(Core *core) {
  const std::uint32_t count = core->r[5];
  const std::uint32_t buffer = core->r[6];
  runtime::callOriginal(*core, GuestImage::Resident, guest::kBufferComponentInit);
  beginConsecutive(*core, buffer, count);
}

void placedComponents(Core *core, std::uint32_t initialiser) {
  const std::uint32_t descriptor = core->r[4];
  runtime::callOriginal(*core, GuestImage::Resident, initialiser);
  beginConsecutive(*core,
                   core->mem_r32(descriptor + guest::kPlacedComponentArray),
                   core->mem_r32(descriptor + guest::kPlacedComponentCount));
}

void placedComponentAlloc(Core *core) {
  placedComponents(core, guest::kPlacedComponentAlloc);
}

void placedComponentInit(Core *core) {
  placedComponents(core, guest::kPlacedComponentInit);
}

void animationNode(Core *core, std::uint32_t creator) {
  const std::uint32_t node = core->mem_r32(guest::kAnimationNodeFreeList);
  runtime::callOriginal(*core, GuestImage::Resident, creator);
  if (node != 0u) {
    beginLife(*core, node + guest::kAnimationNodeComponent);
  }
}

void animationNodeType0(Core *core) {
  animationNode(core, guest::kAnimationNodeCreateType0);
}

void animationNodeType3(Core *core) {
  animationNode(core, guest::kAnimationNodeCreateType3);
}

void animationNodeType5(Core *core) {
  animationNode(core, guest::kAnimationNodeCreateType5);
}

void bootStaticComponentInit(Core *core) {
  runtime::callOriginal(*core, GuestImage::Boot, guest::kBootStaticComponentInit);
  beginLife(*core, guest::kBootStaticComponent);
}

} // namespace

void ComponentIncarnations::begin(std::uint32_t component) {
  ++generations_[psx::gpu::mainRamOffsetOf(component)];
}

std::uint32_t ComponentIncarnations::generation(std::uint32_t component) const {
  const auto found = generations_.find(psx::gpu::mainRamOffsetOf(component));
  return found == generations_.end() ? 0u : found->second;
}

void registerComponentIncarnationOwners(Core &core) {
  runtime::registerNativeOverride(core,
                                  GuestImage::Resident,
                                  guest::kEntityComponentAlloc,
                                  "CrashBash::EntityComponentAlloc",
                                  entityComponentAlloc);
  runtime::registerNativeOverride(
      core, GuestImage::Resident, guest::kListComponentAlloc, "CrashBash::ListComponentAlloc", listComponentAlloc);
  runtime::registerNativeOverride(
      core, GuestImage::Resident, guest::kBufferComponentInit, "CrashBash::BufferComponentInit", bufferComponentInit);
  runtime::registerNativeOverride(core,
                                  GuestImage::Resident,
                                  guest::kPlacedComponentAlloc,
                                  "CrashBash::PlacedComponentAlloc",
                                  placedComponentAlloc);
  runtime::registerNativeOverride(
      core, GuestImage::Resident, guest::kPlacedComponentInit, "CrashBash::PlacedComponentInit", placedComponentInit);
  runtime::registerNativeOverride(core,
                                  GuestImage::Resident,
                                  guest::kAnimationNodeCreateType0,
                                  "CrashBash::AnimationNodeType0",
                                  animationNodeType0);
  runtime::registerNativeOverride(core,
                                  GuestImage::Resident,
                                  guest::kAnimationNodeCreateType3,
                                  "CrashBash::AnimationNodeType3",
                                  animationNodeType3);
  runtime::registerNativeOverride(core,
                                  GuestImage::Resident,
                                  guest::kAnimationNodeCreateType5,
                                  "CrashBash::AnimationNodeType5",
                                  animationNodeType5);
  runtime::registerNativeOverride(core,
                                  GuestImage::Boot,
                                  guest::kBootStaticComponentInit,
                                  "CrashBash::BootStaticComponentInit",
                                  bootStaticComponentInit);
}

} // namespace crashbash::render
