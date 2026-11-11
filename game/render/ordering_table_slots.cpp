#include "render/ordering_table_slots.h"

#include "core.h"
#include "ordering_table.h"
#include "title/guest.h"

namespace crashbash::render {
namespace {

constexpr std::uint32_t kWordBytes = 4u;

} // namespace

void nameOrderingTables(Core &core) {
  for (const std::uint32_t base : {guest::kOrderingTableA, guest::kOrderingTableB}) {
    core.otTables.name(kOrderingTableId, base, guest::kOrderingTableBuckets, kWordBytes, psx::gpu::OtWalk::HighToLow);
  }
}

void followDrawBase(Core &core, std::uint32_t walked, std::uint32_t next) {
  const std::uint32_t base = core.mem_r32(guest::kDrawOtBase);
  if (base >= walked && base - walked < guest::kOrderingTableBuckets * kWordBytes) {
    core.mem_w32(guest::kDrawOtBase, next + (base - walked));
  }
}

} // namespace crashbash::render
