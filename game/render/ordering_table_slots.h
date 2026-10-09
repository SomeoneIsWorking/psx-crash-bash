// game/render/ordering_table_slots.h — which bucket of the guest's ordering table a packet is linked into.
//
// DisplayFrame alternates two tables (crashbash_guest.h) that ClearOTagR leaves with every bucket linked to
// the one below it, and every guest packet is linked in at its bucket's head. A linked packet therefore
// reaches the head word of bucket b - 1 by following its links, which names its bucket, and it is on the
// walk exactly when it is reachable from bucket b's head.
#pragma once

#include "frame_record.h"

#include <cstdint>
#include <optional>

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

// The bucket the packet at `packet` is linked into in the table in use; nullopt when it is not on the walk.
std::optional<psx::present::OtSlot> linkedSlot(Core &core, std::uint32_t packet);

} // namespace crashbash::render
