#include "render/texture_scroll_motion.h"

#include "blend.h"
#include "core.h"
#include "execution/guest_execution.h"
#include "frame_record.h"
#include "runtime/session.h"
#include "title/guest.h"

#include <cmath>

namespace crashbash::render {
namespace {

// FUN_80028F28's scroller: table index, speed, 8.8 position, source pointer; the owner's table of 0x38-byte entries
// holds the strip's width, height and destination pointer.
constexpr std::uint32_t kScrollerTable = 0x00u;
constexpr std::uint32_t kScrollerPosition = 0x08u;
constexpr std::uint32_t kScrollerSource = 0x0Cu;
constexpr std::uint32_t kOwnerTables = 0x08u;
constexpr std::uint32_t kTableEntryBytes = 0x38u;
constexpr std::uint32_t kEntryWidth = 0x1Cu;
constexpr std::uint32_t kEntryHeight = 0x1Eu;
constexpr std::uint32_t kEntryDestination = 0x20u;

// SetDrawMove's packet: tag, flush, GP0(80), source, destination, size.
constexpr std::uint32_t kMovePacketBytes = 24u;
constexpr std::uint32_t kMoveWords = 5u;
constexpr std::uint32_t kMoveFlush = 0x01000000u;
constexpr std::uint32_t kMoveCommand = 0x80000000u;
constexpr std::uint32_t kMoveFlushOffset = 4u;
constexpr std::uint32_t kMoveCommandOffset = 8u;
constexpr std::uint32_t kTagLink = 0x00FFFFFFu;

std::int32_t half(Core &core, std::uint32_t address) {
  return static_cast<std::int16_t>(core.mem_r16(address));
}

void textureScroll(Core *core) {
  const std::uint32_t scroller = core->r[4];
  const std::uint32_t owner = core->r[5];
  const std::uint32_t moves =
      core->mem_r32(core->mem_r32(guest::kOrderingTablePointer) + guest::kOrderingTablePoolCursor);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kTextureScroll);
  session(*core).drawReplay.noteScrolled(scroller, owner, moves);
}

// The DR_MOVE at `packet` as `copy`, or as no command when it copies nothing; `link` is its tag's.
void writeMove(psx::present::GuestRedraw &redraw,
               std::uint32_t packet,
               std::uint32_t link,
               const psx::present::VramCopy &copy) {
  if (copy.height == 0) {
    redraw.writeWord(packet, link);
    return;
  }
  redraw.writeWord(packet, kMoveWords << 24 | link);
  redraw.writeWord(packet + kMoveFlushOffset, kMoveFlush);
  redraw.writeWord(packet + kMoveCommandOffset, kMoveCommand);
  redraw.writeCopy(packet + kMoveCommandOffset, copy);
}

} // namespace

void TextureScrollMotion::noteScrolled(std::uint32_t scroller, std::uint32_t owner, std::uint32_t moves) {
  noted_.push_back({.scroller = scroller, .owner = owner, .moves = moves});
}

void TextureScrollMotion::forget() {
  noted_.clear();
  earlier_.clear();
  later_.clear();
}

void TextureScrollMotion::sample(Core &core) {
  earlier_.swap(later_);
  later_.clear();
  for (const Note &note : noted_) {
    const std::uint32_t entry =
        core.mem_r32(note.owner + kOwnerTables) + core.mem_r32(note.scroller + kScrollerTable) * kTableEntryBytes;
    const std::uint32_t source = core.mem_r32(note.scroller + kScrollerSource);
    const std::uint32_t destination = core.mem_r32(entry + kEntryDestination);
    later_.push_back(
        {.scroller = note.scroller,
         .moves = note.moves,
         .links = {core.mem_r32(note.moves) & kTagLink, core.mem_r32(note.moves + kMovePacketBytes) & kTagLink},
         .position = static_cast<std::int32_t>(core.mem_r32(note.scroller + kScrollerPosition)),
         .width = half(core, entry + kEntryWidth),
         .height = half(core, entry + kEntryHeight),
         .srcX = half(core, source),
         .srcY = half(core, source + 2u),
         .dstX = half(core, destination),
         .dstY = half(core, destination + 2u)});
  }
  noted_.clear();
}

psx::present::MotionCount TextureScrollMotion::write(psx::present::GuestRedraw &redraw, float t) const {
  psx::present::MotionCount count{.drawn = later_.size()};
  for (const Record &to : later_) {
    const Record *from = nullptr;
    for (const Record &earlier : earlier_) {
      if (earlier.sameStrip(to)) {
        from = &earlier;
      }
    }
    if (from == nullptr || to.height <= 0) {
      ++count.unpaired;
      continue;
    }
    // The guest rolls the strip by the position's whole rows; between two ticks it lies between them.
    const double rows = psx::present::lerpRing(from->position >> 8, to.position >> 8, to.height, t);
    const int whole = static_cast<int>(std::floor(rows));
    const auto part = static_cast<float>(rows - whole);
    const bool placed = part > 0.0f;
    // Rows `rows`.. of the source to the top of the strip.
    psx::present::VramCopy upper{.srcX = to.srcX,
                                 .srcY = to.srcY + whole,
                                 .dstX = to.dstX,
                                 .dstY = to.dstY,
                                 .width = to.width,
                                 .height = to.height - whole};
    upper.place = {.srcY = part, .height = -part};
    // Rows ..`rows` below them, at whole rows rounded up so a part of a row is a copy of its own.
    psx::present::VramCopy lower{.srcX = to.srcX,
                                 .srcY = to.srcY,
                                 .dstX = to.dstX,
                                 .dstY = to.dstY + to.height - whole - (placed ? 1 : 0),
                                 .width = to.width,
                                 .height = whole + (placed ? 1 : 0)};
    if (placed) {
      lower.place = {.dstY = 1.0f - part, .height = part - 1.0f};
    }
    writeMove(redraw, to.moves, to.links[0], upper);
    writeMove(redraw, to.moves + kMovePacketBytes, to.links[1], lower);
  }
  return count;
}

void registerTextureScrollNotes(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kTextureScroll, "CrashBash::TextureScroll", textureScroll);
}

} // namespace crashbash::render
