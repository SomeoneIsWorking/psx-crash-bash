#include "render/component_incarnation.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "execution_services.h"
#include "frame/measured_guest_call.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "runtime/session.h"
#include "title/guest.h"

#include <cstdint>

namespace crashbash::render {
namespace {

using runtime::GuestImage;

ComponentIncarnations &incarnations(Core &core) {
  return session(core).componentIncarnations;
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

constexpr GuestFrameSpill kScratchDrawSpills[] = {
    {18, 0x18}, {16, 0x10}, {31, 0x2C}, {22, 0x28}, {21, 0x24}, {20, 0x20}, {19, 0x1C}, {17, 0x14}};
constexpr std::uint32_t kScratchDrawCodeFlags = 0x03008000u;

// One entry: a fresh copy of the template into the scratch component, then the entry's mesh and model row.
void fillScratchComponent(Core &core, std::uint32_t entry) {
  const std::uint32_t component = guest::kBootScratchComponent;
  const std::uint32_t row = guest::kBootModelRows + core.mem_r32(entry + 4u) * guest::kBootModelRowBytes;
  for (std::uint32_t offset = 0; offset < guest::kRenderTemplateBytes; offset += 4u) {
    core.mem_w32(component + offset, core.mem_r32(guest::kRenderTemplate + offset));
  }
  // Left as the copy loop leaves them: the last chunk's two upper words.
  core.r[9] = core.mem_r32(guest::kRenderTemplate + 0x98u);
  core.r[10] = core.mem_r32(guest::kRenderTemplate + 0x9Cu);
  core.r[8] = core.mem_r32(guest::kRenderTemplate + 0xA4u);
  core.mem_w16(component + 0x74u, core.mem_r16(entry));
  core.mem_w8(component + 0x51u, 0u);
  core.mem_w32(component + 0x6Cu, core.mem_r32(entry + 8u));
  const auto scale = static_cast<std::int32_t>(core.mem_r32(guest::kBootModelScale));
  core.r[3] = static_cast<std::uint32_t>(scale);
  for (std::uint32_t axis = 0; axis < 3u; ++axis) {
    const auto coordinate = static_cast<std::int32_t>(core.mem_r32(row + axis * 4u));
    psx::cpu::multiply(&core, scale, coordinate);
    core.r[7] = core.lo;
    core.mem_w16(component + 0x10u + axis * 2u, static_cast<std::uint16_t>(static_cast<std::int32_t>(core.lo) >> 12));
  }
  core.mem_w32(component, kScratchDrawCodeFlags);
  core.r[2] = core.mem_r16(row + 0xCu);
  core.mem_w16(component + 0x68u, static_cast<std::uint16_t>(core.r[2]));
}

// BOOT 0x8009440C: each entry refills the scratch component and draws it. The key names the entry, so the
// two meshes of one frame are two objects and each keeps its key across frames.
void bootScratchComponentDraw(Core *core) {
  GuestFrame<0x30, 8> frame(core, kScratchDrawSpills);
  constexpr std::uint32_t kDrawGlobals = guest::kDrawContextPointer - 0x48u;
  core->r[4] = kDrawGlobals;
  core->r[3] = core->mem_r32(guest::kDrawContextPointer);
  core->r[2] = core->mem_r32(guest::kBootScratchEntryCount);
  core->r[16] = guest::kBootScratchComponent;
  core->r[20] = core->mem_r32(core->r[3] + 0x18u);
  core->r[18] = 0u;
  psx::cpu::accountGuestInstructions(*core, 19u);
  if (static_cast<std::int32_t>(core->r[2]) <= 0) {
    psx::cpu::accountGuestInstructions(*core, 10u);
    return;
  }
  core->r[22] = guest::kBootModelRows;
  core->r[21] = guest::kRenderTemplate + 0xA0u;
  core->r[19] = kDrawGlobals;
  core->r[17] = guest::kBootScratchEntries;
  psx::cpu::accountGuestInstructions(*core, 7u);
  do {
    fillScratchComponent(*core, core->r[17]);
    incarnations(*core).refill(guest::kBootScratchComponent, core->r[18]);
    core->gte().writeControl(psx::gte::kH, core->r[20]);
    const std::uint32_t context = core->mem_r32(guest::kDrawContextPointer);
    measuredGuestCall(*core,
                      guest::kObjectDraw,
                      0x8009456Cu,
                      161u,
                      guest::kBootScratchComponent,
                      context + 0x74u,
                      kScratchDrawCodeFlags);
    core->gte().writeControl(psx::gte::kH, core->mem_r32(core->mem_r32(guest::kDrawContextPointer) + 0x18u));
    core->r[18] += 1u;
    core->r[17] += guest::kBootScratchEntryBytes;
    core->r[2] =
        static_cast<std::int32_t>(core->r[18]) < static_cast<std::int32_t>(core->mem_r32(guest::kBootScratchEntryCount))
            ? 1u
            : 0u;
    psx::cpu::accountGuestInstructions(*core, 11u);
  } while (core->r[2] != 0u);
  psx::cpu::accountGuestInstructions(*core, 10u);
}

} // namespace

void ComponentIncarnations::begin(std::uint32_t component) {
  ++generations_[component & kComponentOffsetMask];
}

void ComponentIncarnations::refill(std::uint32_t component, std::uint32_t entry) {
  generations_[component & kComponentOffsetMask] = entry;
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
