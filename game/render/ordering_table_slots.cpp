#include "ordering_table_slots.h"

#include "core.h"
#include "crashbash_guest.h"
#include "ordering_table.h"

namespace crashbash::render {
namespace {

constexpr std::uint32_t kWordBytes = 4u;
// Nodes followed before a chain is called malformed; a bucket never holds this many packets.
constexpr std::uint32_t kChainLimit = 0x10000u;

bool endsChain(std::uint32_t link) {
  return link == psx::gpu::kOtChainEnd || link == psx::gpu::kOtChainEndZero;
}

std::uint32_t nodeAt(std::uint32_t link) {
  return psx::gpu::guestAddressOf(link);
}

// The head word of the bucket below the packet's own: its slot, or none when the chain ends first.
struct Tail {
  bool ended = false;
  std::uint32_t link = 0; // the link field that names it
};

std::optional<Tail> followToTail(Core &core, std::uint32_t packet) {
  std::uint32_t at = packet;
  for (std::uint32_t step = 0; step < kChainLimit; ++step) {
    const std::uint32_t link = core.mem_r32(at) & psx::gpu::kOtNextAddressMask;
    if (endsChain(link)) {
      return Tail{true, link};
    }
    if (core.otTables.slotOf(nodeAt(link))) {
      return Tail{false, link};
    }
    at = nodeAt(link);
  }
  return std::nullopt;
}

} // namespace

void nameOrderingTables(Core &core) {
  for (const std::uint32_t base : {guest::kOrderingTableA, guest::kOrderingTableB}) {
    core.otTables.name(kOrderingTableId, base, guest::kOrderingTableBuckets, kWordBytes, psx::gpu::OtWalk::HighToLow);
  }
}

std::optional<psx::present::OtSlot> linkedSlot(Core &core, std::uint32_t packet) {
  const std::optional<Tail> tail = followToTail(core, packet);
  if (!tail) {
    return std::nullopt;
  }
  std::uint32_t bucket = 0;
  const std::uint32_t table = core.mem_r32(guest::kOrderingTablePointer);
  std::uint32_t head = table;
  if (!tail->ended) {
    // A packet a draw left unlinked still points into the table of the frame before.
    const std::uint32_t below = nodeAt(tail->link);
    if (below < table || below >= table + guest::kOrderingTableBuckets * kWordBytes) {
      return std::nullopt;
    }
    bucket = core.otTables.slotOf(below)->index + 1u;
    head = below + kWordBytes;
  }
  if (bucket >= guest::kOrderingTableBuckets) {
    return std::nullopt;
  }
  const std::uint32_t wanted = packet & psx::gpu::kOtNextAddressMask;
  std::uint32_t node = core.mem_r32(head) & psx::gpu::kOtNextAddressMask;
  for (std::uint32_t step = 0; step < kChainLimit && !endsChain(node) && node != tail->link; ++step) {
    if (node == wanted) {
      return psx::present::OtSlot{kOrderingTableId, bucket};
    }
    node = core.mem_r32(nodeAt(node)) & psx::gpu::kOtNextAddressMask;
  }
  return std::nullopt;
}

} // namespace crashbash::render
