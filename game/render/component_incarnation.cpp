#include "component_incarnation.h"

#include "core.h"
#include "crashbash_frame_driver.h"
#include "crashbash_guest.h"
#include "guest_execution.h"

namespace crashbash::render {
namespace {

using runtime::GuestImage;

ComponentIncarnations &incarnations(Core &core) {
  return frameDriver(core).componentIncarnations();
}

void beginConsecutive(Core &core, std::uint32_t first, std::uint32_t count) {
  for (std::uint32_t i = 0; i < count; ++i) {
    incarnations(core).begin(first + i * guest::kRenderComponentBytes);
  }
}

// The pool allocators hand out the pool's first v0 components in list order.
void poolAllocation(Core *core, std::uint32_t allocator) {
  std::uint32_t component = core->mem_r32(guest::kRenderComponentPool);
  runtime::callOriginal(*core, GuestImage::Resident, allocator);
  const std::uint32_t taken = core->r[2];
  for (std::uint32_t i = 0; i < taken && component != 0u; ++i) {
    incarnations(*core).begin(component);
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
    incarnations(*core).begin(node + guest::kAnimationNodeComponent);
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
  incarnations(*core).begin(guest::kBootStaticComponent);
}

// Every entry this draws is a fresh copy of the template into one component.
void bootScratchComponentDraw(Core *core) {
  runtime::callOriginal(*core, GuestImage::Boot, guest::kBootScratchComponentDraw);
  incarnations(*core).begin(guest::kBootScratchComponent);
}

} // namespace

void ComponentIncarnations::begin(std::uint32_t component) {
  ++generations_[component & kComponentOffsetMask];
}

std::uint32_t ComponentIncarnations::object(std::uint32_t component) const {
  const auto found = generations_.find(component & kComponentOffsetMask);
  return incarnationObject(component, found == generations_.end() ? 0u : found->second);
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
  runtime::registerNativeOverride(core,
                                  GuestImage::Boot,
                                  guest::kBootScratchComponentDraw,
                                  "CrashBash::BootScratchComponentDraw",
                                  bootScratchComponentDraw);
}

} // namespace crashbash::render
