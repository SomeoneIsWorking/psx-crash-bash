#include "render/texture_scroll_producer.h"

#include "blend.h"
#include "core.h"
#include "execution/guest_execution.h"
#include "render/ordering_table_slots.h"
#include "state_bytes.h"
#include "state_producer.h"
#include "title/guest.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>

namespace crashbash::render {
namespace {

constexpr std::uint32_t kScrollerTable = 0x00u;
constexpr std::uint32_t kScrollerPosition = 0x08u;
constexpr std::uint32_t kScrollerSource = 0x0Cu;
constexpr std::uint32_t kOwnerTables = 0x08u;
constexpr std::uint32_t kTableEntryBytes = 0x38u;
constexpr std::uint32_t kEntryWidth = 0x1Cu;
constexpr std::uint32_t kEntryHeight = 0x1Eu;
constexpr std::uint32_t kEntryDestination = 0x20u;
constexpr std::uint32_t kMovePacketBytes = 24u;

struct ScrollState {
  std::int32_t position = 0; // 8.8, in [0, height)
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::int16_t srcX = 0;
  std::int16_t srcY = 0;
  std::int16_t dstX = 0;
  std::int16_t dstY = 0;
  std::uint32_t slotIndex = 0;
  std::uint16_t slotTable = 0;
  std::uint16_t unused = 0;

  bool sameStrip(const ScrollState &other) const {
    return width == other.width && height == other.height && srcX == other.srcX && srcY == other.srcY &&
           dstX == other.dstX && dstY == other.dstY;
  }
};

std::int16_t half(Core &core, std::uint32_t address) {
  return static_cast<std::int16_t>(core.mem_r16(address));
}

void textureScroll(Core *core) {
  const std::uint32_t scroller = core->r[4];
  const std::uint32_t owner = core->r[5];
  const std::uint32_t firstMove =
      core->mem_r32(core->mem_r32(guest::kOrderingTablePointer) + guest::kOrderingTablePoolCursor);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kTextureScroll);
  const std::optional<psx::present::OtSlot> slot = linkedSlot(*core, firstMove + kMovePacketBytes);
  if (!slot) {
    return;
  }
  const std::uint32_t entry =
      core->mem_r32(owner + kOwnerTables) + core->mem_r32(scroller + kScrollerTable) * kTableEntryBytes;
  const std::uint32_t source = core->mem_r32(scroller + kScrollerSource);
  const std::uint32_t destination = core->mem_r32(entry + kEntryDestination);
  const ScrollState state{.position = static_cast<std::int32_t>(core->mem_r32(scroller + kScrollerPosition)),
                          .width = half(*core, entry + kEntryWidth),
                          .height = half(*core, entry + kEntryHeight),
                          .srcX = half(*core, source),
                          .srcY = half(*core, source + 2),
                          .dstX = half(*core, destination),
                          .dstY = half(*core, destination + 2),
                          .slotIndex = slot->index,
                          .slotTable = slot->table};
  core->frameStates.save(core->emission.current(), state);
}

psx::present::VramCopy move(int srcX, int srcY, int dstX, int dstY, int width, int height) {
  return psx::present::VramCopy{
      .srcX = srcX, .srcY = srcY, .dstX = dstX, .dstY = dstY, .width = width, .height = height};
}

class TextureScrollRender final : public psx::present::StateProducer {
public:
  void render(const psx::present::StateInterval &states, psx::present::PrimitiveSink &sink) const override {
    const auto to = psx::present::StateReader(states.to).get<ScrollState>();
    const auto from = psx::present::StateReader(states.from).get<ScrollState>();
    const int height = to.height;
    // The guest scrolls by the position's whole pixels; between two frames the strip moves between them.
    const double shown = from.sameStrip(to)
                             ? psx::present::lerpRing(from.position >> 8, to.position >> 8, height, states.t)
                             : static_cast<double>(to.position >> 8);
    const int whole = static_cast<int>(std::floor(shown));
    const auto part = static_cast<float>(shown - whole);
    const psx::present::OtSlot slot{.table = to.slotTable, .index = to.slotIndex};
    // Linked at the bucket head in this order, so the walk meets the lower rows' move first.
    psx::present::VramCopy lower = move(to.srcX, to.srcY, to.dstX, to.dstY + height - whole, to.width, whole);
    lower.place = {.dstY = -part, .height = part};
    sink.emitCopy(slot, lower);
    psx::present::VramCopy upper = move(to.srcX, to.srcY + whole, to.dstX, to.dstY, to.width, height - whole);
    upper.place = {.srcY = part, .height = -part};
    sink.emitCopy(slot, upper);
  }
};

} // namespace

void registerTextureScrollProducer(Core &core, psx::present::StateProducerTable &renders) {
  runtime::registerNativeOverride(core,
                                  runtime::GuestImage::Resident,
                                  guest::kTextureScroll,
                                  "CrashBash::TextureScroll",
                                  textureScroll,
                                  psx::present::Producer{psx::present::Arg::A0});
  renders.install(guest::kTextureScroll, std::make_unique<TextureScrollRender>());
}

} // namespace crashbash::render
