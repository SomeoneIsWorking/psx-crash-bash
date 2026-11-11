#include "frame/draw_section.h"

#include "core.h"
#include "guest_redraw.h"
#include "ordering_table.h"
#include "title/guest.h"

namespace crashbash {
namespace {

// 0x80010278 runs the root scene's present on the scratchpad stack.
constexpr std::uint32_t kPresentStack = 0x1F8003FCu;
// DisplayFrame hands DrawOTag the table's last bucket.
constexpr std::uint32_t kOrderingTableHeadOffset = (guest::kOrderingTableBuckets - 1u) * 4u;

} // namespace

std::optional<std::uint32_t> DrawSection::draw(Core &core, psx::present::GuestRedraw &redraw) const {
  const std::uint32_t scene = core.mem_r32(guest::kAppModeVtable);
  const std::uint32_t present = core.mem_r32(scene + guest::kSceneTransitionPresentSlot);
  core.r[29] = kPresentStack;
  if (!redraw.dispatch(present, {})) {
    return std::nullopt;
  }
  return core.mem_r32(guest::kOrderingTablePointer) + kOrderingTableHeadOffset;
}

bool DrawSection::linked(std::uint32_t head) const {
  const std::uint32_t offset = psx::gpu::mainRamOffsetOf(head);
  return offset == psx::gpu::mainRamOffsetOf(guest::kOrderingTableA + kOrderingTableHeadOffset) ||
         offset == psx::gpu::mainRamOffsetOf(guest::kOrderingTableB + kOrderingTableHeadOffset);
}

} // namespace crashbash
