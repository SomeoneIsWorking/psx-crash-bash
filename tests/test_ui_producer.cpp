// The UI producers key text glyphs, panel parts and HUD digits by their element, through the shipping
// overrides, with guest stand-ins placed at the real call sites.
#include "component_incarnation.h"
#include "core.h"
#include "crashbash_frame_driver.h"
#include "crashbash_guest.h"
#include "game.h"
#include "guest_execution.h"
#include "guest_stub.h"
#include "leaf_packets.h"
#include "ordering_table_slots.h"
#include "packet_collector.h"
#include "state_render_check.h"
#include "state_renders.h"
#include "testutil.h"
#include "title_adapter.h"
#include "ui_producer.h"

#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

namespace {

namespace guest = crashbash::guest;
using crashbash::render::DigitPlace;
using crashbash::render::glyphElement;
using crashbash::render::incarnationObject;
using crashbash::render::PanelPart;
using crashbash::test::addiu;
using crashbash::test::beqz;
using crashbash::test::j;
using crashbash::test::jal;
using crashbash::test::jr;
using crashbash::test::kRa;
using crashbash::test::kS2;
using crashbash::test::kS3;
using crashbash::test::kS4;
using crashbash::test::kS5;
using crashbash::test::kT0;
using crashbash::test::kT1;
using crashbash::test::lbu;
using crashbash::test::move;
using crashbash::test::nop;
using crashbash::test::place;
using psx::present::RecordKey;

constexpr GuestAddressRange kResidentRange{0x18000u, 0x2A000u};
constexpr GuestAddressRange kBootRange{0x78C90u, 0xA7490u};
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr std::uint32_t kPackets = crashbash::test::OrderingTableModel::kPool;
constexpr std::uint32_t kQuadBytes = 13u * 4u;
constexpr std::uint32_t kShadedBytes = 11u * 4u;
constexpr std::uint32_t kAttributes = crashbash::render::kDrawn | crashbash::render::kFlatLayout;
constexpr std::uint32_t kGlyphBucket = 3u;
constexpr std::uint32_t kComponentA = 0x80150000u;
constexpr std::uint32_t kComponentB = 0x801500A8u;
constexpr std::uint32_t kStringA = 0x80120000u;
constexpr std::uint32_t kStringB = 0x80120100u;
constexpr std::uint32_t kHudRecord = 0x8009AD7Cu;
constexpr std::uint32_t kMenuItem = 0x80099000u;

// A call at `site - 8` that returns to `site`, then jumps on.
void callThenJump(Core &core, std::uint32_t site, std::uint32_t callee, std::uint32_t next) {
  place(core, site - 8u, {jal(callee), nop(), j(next), nop()});
}

void callThenReturn(Core &core, std::uint32_t site, std::uint32_t callee, std::uint32_t link) {
  place(core, site - 8u, {jal(callee), nop(), jr(link), nop()});
}

void stubGuest(Core &core) {
  // FUN_800243A0: s2 walks the string; each glyph's quad returns to 0x80024688.
  place(core, guest::kStringDraw, {move(kS3, kRa), j(0x80024680u), addiu(kS2, kT1, 1u)});
  place(core,
        0x80024680u,
        {jal(guest::kImageQuad),
         nop(),
         lbu(kT0, kS2),
         nop(),
         beqz(kT0, 0x80024690u, 0x800246A4u),
         addiu(kS2, kS2, 1u),
         j(0x80024680u),
         nop()});
  place(core, 0x800246A4u, {jr(kS3), nop()});
  // FUN_800248A0: hundreds, tens, units.
  place(core, guest::kNumberDraw, {move(kS3, kRa), j(guest::kNumberHundredsReturn - 8u), nop()});
  callThenJump(core, guest::kNumberHundredsReturn, guest::kImageQuad, guest::kNumberTensReturn - 8u);
  callThenJump(core, guest::kNumberTensReturn, guest::kImageQuad, guest::kNumberUnitsReturn - 8u);
  callThenReturn(core, guest::kNumberUnitsReturn, guest::kImageQuad, kS3);
  // FUN_8001A6D4: the body, then FUN_8001A43C's four borders.
  place(core, 0x8001A6D4u, {move(kS3, kRa), j(guest::kPanelBodyReturn - 8u), nop()});
  callThenJump(core, guest::kPanelBodyReturn, guest::kShadedQuad, guest::kPanelLeftReturn - 8u);
  callThenJump(core, guest::kPanelLeftReturn, guest::kShadedQuad, guest::kPanelRightReturn - 8u);
  callThenJump(core, guest::kPanelRightReturn, guest::kShadedQuad, guest::kPanelTopReturn - 8u);
  callThenJump(core, guest::kPanelTopReturn, guest::kShadedQuad, guest::kPanelBottomReturn - 8u);
  callThenReturn(core, guest::kPanelBottomReturn, guest::kShadedQuad, kS3);
  // The component callbacks.
  place(core, guest::kTextComponentDraw, {move(kS4, kRa), jal(guest::kStringDraw), nop(), jr(kS4), nop()});
  place(core, guest::kPanelComponentDraw, {move(kS4, kRa), jal(0x8001A6D4u), nop(), jr(kS4), nop()});
  // BOOT call sites, each returning to s5.
  callThenReturn(core, guest::kBootHudIconReturns[2], guest::kImageQuad, kS5);
  callThenReturn(core, guest::kBootHudTwoDigitReturns[0], guest::kImageQuad, kS5);
  callThenReturn(core, guest::kBootHudTwoDigitReturns[1], guest::kImageQuad, kS5);
  callThenReturn(core, guest::kBootHudThreeDigitReturns[2], guest::kImageQuad, kS5);
  callThenReturn(core, guest::kBootMenuNumberReturn, guest::kNumberDraw, kS5);
}

void writeString(Core &core, std::uint32_t address, const char *text) {
  for (std::uint32_t i = 0;; ++i) {
    core.mem_w8(address + i, static_cast<std::uint8_t>(text[i]));
    if (text[i] == '\0') {
      return;
    }
  }
}

crashbash::TitleAdapter runtime;

using crashbash::runtime::GuestImage;

// `upper` is the image bound over BOOT's addresses.
std::unique_ptr<Game> makeGame(GuestImage upper = GuestImage::Boot) {
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  stubGuest(core);
  writeString(core, kStringA, "AB");
  writeString(core, kStringB, "XYZ");
  auto *execution = static_cast<crashbash::runtime::GuestExecution *>(core.gameCtx);
  execution->bindAuthenticatedImage(
      GuestImage::Resident, core.imageCatalog().activate("synthetic-resident", kResidentRange, 1u), kResidentRange);
  execution->bindAuthenticatedImage(upper, core.imageCatalog().activate("synthetic-upper", kBootRange, 1u), kBootRange);
  crashbash::render::registerUiProducers(core);
  crashbash::render::nameOrderingTables(core);
  crashbash::render::registerStateRenders(core);
  crashbash::test::OrderingTableModel(core).begin();
  crashbash::test::warmUpExecutor(core, 0x80018100u);
  return game;
}

bool call(Core &core, std::uint32_t entry, std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> registers) {
  // The leaves' arguments: a texture record or corners, attributes or a position, a bucket and a colour.
  core.r[4] = kComponentA;
  core.r[5] = kAttributes;
  core.r[6] = kGlyphBucket;
  core.r[7] = 0x00808080u;
  core.r[kS5] = kReturn;
  core.r[29] = 0x801FFF00u;
  core.r[kRa] = kReturn;
  for (const auto &[index, value] : registers) {
    core.r[index] = value;
  }
  return psx::cpu::dispatchGuest(core, entry, psx::cpu::ExecutionBudget::fromCycles(100000u)).returned();
}

bool drawText(Core &core, std::uint32_t component, std::uint32_t string) {
  return call(core, guest::kTextComponentDraw, {{4u, component}, {kT1, string}});
}

RecordKey glyphKey(std::uint32_t component, std::uint32_t string, std::uint32_t glyph) {
  return RecordKey{guest::kTextComponentDraw, incarnationObject(component, 0u), glyphElement(string + glyph), 0u};
}

void test_each_glyph_is_keyed_by_its_component_and_byte() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawText(core, kComponentA, kStringB));
  for (std::uint32_t glyph = 0; glyph < 3u; ++glyph) {
    CHECK(core.emission.identityFor(kPackets + glyph * kQuadBytes) == glyphKey(kComponentA, kStringB, glyph));
  }
  CHECK(!core.emission.identityFor(kPackets + 3u * kQuadBytes));
}

// Two components showing one string, drawn in either order, keep distinct keys of their own.
void test_glyph_keys_follow_the_component_not_the_order() {
  auto game = makeGame();
  Core &core = game->core;
  const auto packet = [](std::uint32_t index) {
    return kPackets + index * kQuadBytes;
  };
  CHECK(drawText(core, kComponentA, kStringA));
  CHECK(drawText(core, kComponentB, kStringA));
  CHECK(core.emission.identityFor(packet(1u)) == glyphKey(kComponentA, kStringA, 1u));
  CHECK(core.emission.identityFor(packet(3u)) == glyphKey(kComponentB, kStringA, 1u));
  CHECK(drawText(core, kComponentB, kStringA));
  CHECK(drawText(core, kComponentA, kStringA));
  CHECK(core.emission.identityFor(packet(5u)) == glyphKey(kComponentB, kStringA, 1u));
  CHECK(core.emission.identityFor(packet(7u)) == glyphKey(kComponentA, kStringA, 1u));
}

// A component switched to another string names other glyphs, so nothing pairs across the switch.
void test_a_new_string_in_a_component_gets_new_keys() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawText(core, kComponentA, kStringB));
  CHECK(!(glyphKey(kComponentA, kStringB, 0u) == glyphKey(kComponentA, kStringA, 0u)));
  CHECK(core.emission.identityFor(kPackets) == glyphKey(kComponentA, kStringB, 0u));
}

void test_each_panel_part_is_its_own_element() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(call(core, guest::kPanelComponentDraw, {{4u, kComponentA}}));
  const PanelPart parts[] = {PanelPart::Body, PanelPart::Left, PanelPart::Right, PanelPart::Top, PanelPart::Bottom};
  for (std::uint32_t i = 0; i < std::size(parts); ++i) {
    CHECK(
        core.emission.identityFor(kPackets + i * kShadedBytes) ==
        (RecordKey{
            guest::kPanelComponentDraw, incarnationObject(kComponentA, 0u), static_cast<std::uint32_t>(parts[i]), 0u}));
  }
}

// Outside a component the string renderer has no object to name glyphs of.
void test_a_string_outside_any_owner_stays_unkeyed() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(call(core, guest::kStringDraw, {{kT1, kStringA}}));
  CHECK(!core.emission.identityFor(kPackets));
}

RecordKey hudKey(std::uint32_t record, DigitPlace place) {
  return RecordKey{guest::kBootHudDraw, record, static_cast<std::uint32_t>(place), 0u};
}

void test_hud_digits_are_keyed_by_player_record_and_place() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(call(core, guest::kBootHudTwoDigitReturns[0] - 8u, {{kS4, kHudRecord}}));
  CHECK(call(core, guest::kBootHudTwoDigitReturns[1] - 8u, {{kS4, kHudRecord}}));
  CHECK(call(core, guest::kBootHudThreeDigitReturns[2] - 8u, {{kS4, kHudRecord}}));
  CHECK(core.emission.identityFor(kPackets) == hudKey(kHudRecord, DigitPlace::Units));
  CHECK(core.emission.identityFor(kPackets + kQuadBytes) == hudKey(kHudRecord, DigitPlace::Tens));
  CHECK(core.emission.identityFor(kPackets + 2u * kQuadBytes) == hudKey(kHudRecord, DigitPlace::Hundreds));
}

void test_hud_icons_are_keyed_by_their_record() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(call(core, guest::kBootHudIconReturns[2] - 8u, {{16u, 0x8009AD5Cu}}));
  CHECK(core.emission.identityFor(kPackets) == hudKey(0x8009AD5Cu, DigitPlace::Units));
}

void test_a_menu_item_number_is_keyed_by_item_and_place() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(call(core, guest::kBootMenuNumberReturn - 8u, {{kS4, kMenuItem}}));
  const DigitPlace order[] = {DigitPlace::Hundreds, DigitPlace::Tens, DigitPlace::Units};
  for (std::uint32_t i = 0; i < std::size(order); ++i) {
    CHECK(core.emission.identityFor(kPackets + i * kQuadBytes) ==
          (RecordKey{guest::kBootMenuPageDraw, kMenuItem, static_cast<std::uint32_t>(order[i]), 0u}));
  }
}

// The BOOT sites name nothing while another image holds those addresses.
void test_boot_sites_need_the_boot_image() {
  auto game = makeGame(GuestImage::Dat22510);
  Core &core = game->core;
  CHECK(call(core, guest::kBootHudTwoDigitReturns[0] - 8u, {{kS4, kHudRecord}}));
  CHECK(!core.emission.identityFor(kPackets));
}

std::uint32_t pointWord(int x, int y) {
  return (static_cast<std::uint32_t>(y) << 16) | static_cast<std::uint16_t>(x);
}

// A component draw through the shipping leaf bodies, over component memory a test fills with what the guest
// would have there: a texture record for the quad leaves, corners and colours for the shaded one.
class UiScene {
public:
  UiScene() : game(makeGame()), table(game->core) {
    Core &core = game->core;
    place(core, guest::kQuadComponentDraw, {move(kS4, kRa), j(guest::kQuadComponentReturn - 8u), nop()});
    callThenReturn(core, guest::kQuadComponentReturn, guest::kShadedQuad, kS4);
  }

  // "XYZ" through the text component: three textured quads, one per glyph, at one position.
  psx::present::FrameRecord drawText(int shift) {
    writeTexture();
    return run(guest::kTextComponentDraw,
               {{4u, kComponentA}, {5u, pointWord(20 + shift, 30 + shift)}, {kT1, kStringB}});
  }

  // The panel body and its four borders: five Gouraud quads in 2D, with a draw mode.
  psx::present::FrameRecord drawPanel(int shift) {
    writeCorners(40 + shift, 50 + shift);
    return run(guest::kPanelComponentDraw, {{4u, kComponentA}, {5u, kAttributes | 0x21u}});
  }

  // One Gouraud quad projected through the GTE; the camera moves it.
  psx::present::FrameRecord drawProjectedQuad(int shift) {
    writeCorners(-40, -30, 400);
    crashbash::test::setCamera(shift * 8);
    return run(guest::kQuadComponentDraw, {{4u, kComponentA}, {5u, crashbash::render::kDrawn}});
  }

  psx::present::FrameRecord drawQuad(int shift) {
    writeCorners(60 + shift, 70 + shift);
    return run(guest::kQuadComponentDraw, {{4u, kComponentA}, {5u, kAttributes}});
  }

  // Overwrites everything a render could read besides its state.
  void scramble() {
    Core &core = game->core;
    table.scramble();
    for (std::uint32_t word = 0; word < 0x40u; ++word) {
      core.mem_w32(kComponentA + word * 4u, 0xA5A5A5A5u);
    }
    crashbash::test::setCamera(0x1234);
  }

  std::unique_ptr<Game> game;
  crashbash::test::OrderingTableModel table;

private:
  psx::present::FrameRecord run(std::uint32_t entry,
                                std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> registers) {
    table.begin();
    if (!call(game->core, entry, registers)) {
      std::abort();
    }
    crashbash::frameDriver(game->core).packetCollector().commit(game->core);
    return table.walk();
  }

  // A 8 by 12 texture record with a page, a palette and four texture coordinates.
  void writeTexture() {
    Core &core = game->core;
    core.mem_w16(kComponentA + 8u, 8u);
    core.mem_w16(kComponentA + 10u, 12u);
    core.mem_w8(kComponentA + 0x10u, 9u);
    core.mem_w16(kComponentA + 0x24u, 0x0025u);
    core.mem_w16(kComponentA + 0x26u, 0x7C00u);
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      core.mem_w16(kComponentA + 0x28u + corner * 2u, 0x0810u + corner * 8u);
    }
  }

  // Four corners 16 by 10 from (x, y) and their colours, in the 0x30-byte layout the shaded leaf reads.
  void writeCorners(int x, int y, int z = 0) {
    Core &core = game->core;
    const int offsets[4][2] = {{0, 0}, {16, 0}, {0, 10}, {16, 10}};
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      core.mem_w32(kComponentA + corner * 8u, pointWord(x + offsets[corner][0], y + offsets[corner][1]));
      core.mem_w32(kComponentA + corner * 8u + 4u, static_cast<std::uint32_t>(z));
      core.mem_w32(kComponentA + 0x20u + corner * 4u, 0x00304050u + corner * 0x101010u);
    }
  }
};

void checkUiRender(std::uint32_t producer, psx::present::FrameRecord (UiScene::*draw)(int)) {
  UiScene scene;
  crashbash::test::checkStateRender(
      scene.game->core,
      producer,
      incarnationObject(kComponentA, 0u),
      [&](int shift) {
        return (scene.*draw)(shift);
      },
      [&] {
        scene.scramble();
      });
}

void test_the_text_render_draws_the_glyphs_the_guest_linked() {
  checkUiRender(guest::kTextComponentDraw, &UiScene::drawText);
}

void test_the_panel_render_draws_the_parts_the_guest_linked() {
  checkUiRender(guest::kPanelComponentDraw, &UiScene::drawPanel);
}

void test_the_quad_render_draws_the_quad_the_guest_linked() {
  checkUiRender(guest::kQuadComponentDraw, &UiScene::drawQuad);
}

// A quad projected through the GTE is re-projected through the blended camera.
void test_the_projected_quad_render_reprojects_through_the_moved_camera() {
  checkUiRender(guest::kQuadComponentDraw, &UiScene::drawProjectedQuad);
}

} // namespace

int main() {
  RUN(each_glyph_is_keyed_by_its_component_and_byte);
  RUN(glyph_keys_follow_the_component_not_the_order);
  RUN(a_new_string_in_a_component_gets_new_keys);
  RUN(each_panel_part_is_its_own_element);
  RUN(a_string_outside_any_owner_stays_unkeyed);
  RUN(hud_digits_are_keyed_by_player_record_and_place);
  RUN(hud_icons_are_keyed_by_their_record);
  RUN(a_menu_item_number_is_keyed_by_item_and_place);
  RUN(boot_sites_need_the_boot_image);
  RUN(the_text_render_draws_the_glyphs_the_guest_linked);
  RUN(the_panel_render_draws_the_parts_the_guest_linked);
  RUN(the_quad_render_draws_the_quad_the_guest_linked);
  RUN(the_projected_quad_render_reprojects_through_the_moved_camera);
  return pt_summary();
}
