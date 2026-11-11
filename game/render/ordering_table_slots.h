// game/render/ordering_table_slots.h — the guest's two ordering tables named to the OT walk, and the draw base
// carried between them.
#pragma once

#include <cstdint>

class Core;

namespace crashbash::render {

// Both tables share one id, as the table per display buffer does.
inline constexpr std::uint16_t kOrderingTableId = 0;

// Names both tables to the OT walk, so each record entry carries its bucket.
void nameOrderingTables(Core &core);

// Retail's DrawOTag is a DMA still walking `walked` while the next update runs, and an update-time draw (the
// pause page) links through the draw base 0x800569D8 the last present left in it. A walk here is instant, so the
// base moves to the same slice of `next`, the table the coming frame walks.
void followDrawBase(Core &core, std::uint32_t walked, std::uint32_t next);

} // namespace crashbash::render
