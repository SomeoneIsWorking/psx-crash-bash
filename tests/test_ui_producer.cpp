// The UI producers key text glyphs, panel parts and HUD digits by their element, and render the components and
// BOOT's HUD and menu items from saved state, through the shipping overrides and native bodies, with guest
// stand-ins at the component callbacks and the BOOT call sites.
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
using crashbash::test::j;
using crashbash::test::jal;
using crashbash::test::jr;
using crashbash::test::kRa;
using crashbash::test::kS4;
using crashbash::test::kS5;
using crashbash::test::kT0;
using crashbash::test::kT1;
using crashbash::test::lw;
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
constexpr std::uint32_t kStringMixed = 0x80120200u;
constexpr std::uint32_t kTextureHeader = 0x80140000u;
constexpr std::uint32_t kTextureBase = 0x80141000u;
constexpr std::uint32_t kStackSlot = 0x801FFF10u; // the fifth argument
constexpr std::uint32_t kStackSlot2 = 0x801FFF14u;
constexpr std::uint32_t kHudRecord = 0x8009AD7Cu;
constexpr std::uint32_t kMenuItem = 0x80099000u;

void callThenReturn(Core &core, std::uint32_t site, std::uint32_t callee, std::uint32_t link) {
  place(core, site - 8u, {jal(callee), nop(), jr(link), nop()});
}

constexpr std::uint32_t kA0 = 4u;
constexpr std::uint32_t kA1 = 5u;
constexpr std::uint32_t kA2 = 6u;
constexpr std::uint32_t kA3 = 7u;
constexpr std::uint32_t kT2 = 10u;

void stubGuest(Core &core) {
  // The text component draws the string its record names: x, y, string address, colour.
  place(core,
        guest::kTextComponentDraw,
        {move(kS4, kRa),
         move(kT2, kA0),
         lw(kA0, kT2, 0u),
         lw(kA1, kT2, 4u),
         lw(kA2, kT2, 8u),
         lw(kA3, kT2, 12u),
         jal(guest::kStringDraw),
         nop(),
         jr(kS4),
         nop()});
  // The panel component draws its own record.
  place(core, guest::kPanelComponentDraw, {move(kS4, kRa), jal(guest::kPanelDraw), nop(), jr(kS4), nop()});
  // BOOT call sites, each returning to s5.
  callThenReturn(core, guest::kBootHudIconReturns[2], guest::kImageQuad, kS5);
  callThenReturn(core, guest::kBootHudTwoDigitReturns[0], guest::kImageQuad, kS5);
  callThenReturn(core, guest::kBootHudTwoDigitReturns[1], guest::kImageQuad, kS5);
  callThenReturn(core, guest::kBootHudThreeDigitReturns[2], guest::kImageQuad, kS5);
  callThenReturn(core, guest::kBootMenuStringReturns[0], guest::kStringDraw, kS5);
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

// The texture record at `address`: 8 by 12 with a page, a palette and four texture coordinates.
void writeTextureRecord(Core &core, std::uint32_t address) {
  core.mem_w16(address + 8u, 8u);
  core.mem_w16(address + 10u, 12u);
  core.mem_w8(address + 0x10u, 9u);
  core.mem_w16(address + 0x24u, 0x0025u);
  core.mem_w16(address + 0x26u, 0x7C00u);
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    core.mem_w16(address + 0x28u + corner * 2u, 0x0810u + corner * 8u);
  }
}

// Font 0: A B X Y Z are quads, Q a sprite, R a raised sprite, P draws nothing; every other byte has no glyph.
// Line feeds step by 12.
void writeFont(Core &core) {
  struct Glyph {
    char character;
    std::int16_t texture;
    std::uint8_t flags;
  };
  const Glyph glyphs[] = {
      {'A', 1, 0}, {'B', 2, 0}, {'X', 3, 0}, {'Y', 4, 0}, {'Z', 5, 0}, {'Q', 6, 1}, {'R', 7, 5}, {'P', -1, 0}};
  core.mem_w32(guest::kFontIndex, 0u);
  core.mem_w8(guest::kFontAdvance, 12u);
  for (std::uint32_t character = 0; character < 0x100u; ++character) {
    core.mem_w16(guest::kFontTexture + character * 2u, 0xFFFFu);
  }
  for (const Glyph &glyph : glyphs) {
    core.mem_w8(guest::kFontAdvance + static_cast<std::uint8_t>(glyph.character), 8u);
    core.mem_w8(guest::kFontFlags + static_cast<std::uint8_t>(glyph.character), glyph.flags);
    core.mem_w16(guest::kFontTexture + static_cast<std::uint8_t>(glyph.character) * 2u,
                 static_cast<std::uint16_t>(glyph.texture));
  }
  core.mem_w32(guest::kTextureTablePointer, kTextureHeader);
  core.mem_w32(kTextureHeader + guest::kTextureTableOffset, kTextureBase);
  core.mem_w32(guest::kDigitTextureTable, kTextureBase);
  for (std::uint32_t index = 0; index < 8u; ++index) {
    writeTextureRecord(core, kTextureBase + index * guest::kTextureRecordBytes);
  }
  for (std::uint32_t digit = 0; digit < 3u; ++digit) {
    core.mem_w16(guest::kDigitSets + digit * 2u, static_cast<std::uint16_t>(digit));
  }
}

// `upper` is the image bound over BOOT's addresses.
std::unique_ptr<Game> makeGame(GuestImage upper = GuestImage::Boot) {
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  stubGuest(core);
  writeFont(core);
  writeString(core, kStringA, "AB");
  writeString(core, kStringB, "XYZ");
  writeString(core, kStringMixed, "XQR");
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

std::uint32_t pointWord(int x, int y) {
  return (static_cast<std::uint32_t>(y) << 16) | static_cast<std::uint16_t>(x);
}

// The text component record: x, y, the string and its colour.
void writeTextRecord(Core &core, std::uint32_t component, int x, int y, std::uint32_t string) {
  core.mem_w32(component, static_cast<std::uint32_t>(x));
  core.mem_w32(component + 4u, static_cast<std::uint32_t>(y));
  core.mem_w32(component + 8u, string);
  core.mem_w32(component + 12u, 0x00808080u);
}

bool drawText(Core &core, std::uint32_t component, std::uint32_t string) {
  writeTextRecord(core, component, 20, 30, string);
  return call(core, guest::kTextComponentDraw, {{4u, component}});
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

// A glyph with no texture draws nothing and does not advance the next one's key.
void test_a_glyph_without_a_texture_draws_no_packet() {
  auto game = makeGame();
  Core &core = game->core;
  writeString(core, kStringB, "PY");
  CHECK(drawText(core, kComponentA, kStringB));
  CHECK(core.emission.identityFor(kPackets) == glyphKey(kComponentA, kStringB, 1u));
  CHECK(!core.emission.identityFor(kPackets + kQuadBytes));
}

void writePanelRecord(Core &core, int x, int y, int right, int bottom) {
  const std::uint32_t record[8] = {crashbash::render::kDrawn,
                                   pointWord(x, y),
                                   0x00000010u,
                                   pointWord(right, bottom),
                                   0x00304050u,
                                   0x00506070u,
                                   pointWord(2, 3),
                                   crashbash::render::kDrawn};
  for (std::uint32_t word = 0; word < 8u; ++word) {
    core.mem_w32(kComponentA + word * 4u, record[word]);
  }
}

void test_each_panel_part_is_its_own_element() {
  auto game = makeGame();
  Core &core = game->core;
  writePanelRecord(core, 40, 50, 100, 90);
  CHECK(call(core, guest::kPanelComponentDraw, {{4u, kComponentA}, {5u, kAttributes}}));
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
  CHECK(call(core, guest::kStringDraw, {{4u, 20u}, {5u, 30u}, {6u, kStringA}}));
  CHECK(!core.emission.identityFor(kPackets));
}

RecordKey hudKey(std::uint32_t record, DigitPlace place) {
  return RecordKey{guest::kBootHudDraw, crashbash::render::ownerObject(record, place), 0u, 0u};
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

// A menu item's number names its three digits by element, under one object.
void test_a_menu_item_number_is_keyed_by_item_and_place() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(call(core, guest::kBootMenuNumberReturn - 8u, {{kS4, kMenuItem}}));
  const DigitPlace order[] = {DigitPlace::Hundreds, DigitPlace::Tens, DigitPlace::Units};
  for (std::uint32_t i = 0; i < std::size(order); ++i) {
    CHECK(core.emission.identityFor(kPackets + i * kQuadBytes) == (RecordKey{guest::kBootMenuPageDraw,
                                                                             crashbash::render::ownerObject(kMenuItem),
                                                                             static_cast<std::uint32_t>(order[i]),
                                                                             0u}));
  }
}

// The BOOT sites name nothing while another image holds those addresses.
void test_boot_sites_need_the_boot_image() {
  auto game = makeGame(GuestImage::Dat22510);
  Core &core = game->core;
  CHECK(call(core, guest::kBootHudTwoDigitReturns[0] - 8u, {{kS4, kHudRecord}}));
  CHECK(!core.emission.identityFor(kPackets));
}

// A component draw through the shipping bodies and leaf bodies, over memory a test fills with what the guest
// would have there: a record for the text and panel bodies, corners and colours for the shaded leaf.
class UiScene {
public:
  UiScene() : game(makeGame()), table(game->core) {
    Core &core = game->core;
    place(core, guest::kQuadComponentDraw, {move(kS4, kRa), j(guest::kQuadComponentReturn - 8u), nop()});
    callThenReturn(core, guest::kQuadComponentReturn, guest::kShadedQuad, kS4);
  }

  // "XQR" through the text component: a quad, a sprite and a raised sprite, at one position.
  psx::present::FrameRecord drawText(int shift) {
    return drawTextAt(20 + shift, 30 + shift, 0u);
  }

  // The same string centred on the screen; only its height moves.
  psx::present::FrameRecord drawCentredText(int shift) {
    return drawTextAt(0x8000, 30 + shift, 0u);
  }

  // Centred with an offset, and tinted by the frame counter.
  psx::present::FrameRecord drawTintedText(int shift) {
    core().mem_w32(guest::kTintCounter, 0u);
    core().mem_w16(guest::kTintTable + 0xC00u * 4u + 2u, 0x180u);
    return drawTextAt(0x8000 | 40, 30 + shift, 1u);
  }

  // The panel body and its four borders: five Gouraud quads in 2D, with a draw mode.
  psx::present::FrameRecord drawPanel(int shift) {
    writePanelRecord(core(), 40 + shift, 50 + shift, 100 + shift, 90 + shift);
    return run(guest::kPanelComponentDraw, {{4u, kComponentA}, {5u, kAttributes | 0x21u}});
  }

  // A panel centred on the screen, its width fixed.
  psx::present::FrameRecord drawCentredPanel(int shift) {
    writePanelRecord(core(), 0x8000, 50 + shift, 60, 90 + shift);
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

  // BOOT's HUD icon and a menu item's string and number, each called from its guest call site.
  psx::present::FrameRecord drawHudIcon(int shift) {
    return run(
        guest::kBootHudIconReturns[2] - 8u,
        {{4u, kTextureBase + guest::kTextureRecordBytes}, {5u, pointWord(30 + shift, 40 + shift)}, {16u, kHudRecord}});
  }

  psx::present::FrameRecord drawMenuString(int shift) {
    return run(guest::kBootMenuStringReturns[0] - 8u,
               {{4u, static_cast<std::uint32_t>(20 + shift)},
                {5u, static_cast<std::uint32_t>(30 + shift)},
                {6u, kStringMixed},
                {kS4, kMenuItem}});
  }

  psx::present::FrameRecord drawMenuNumber(int shift) {
    return run(guest::kBootMenuNumberReturn - 8u,
               {{4u, static_cast<std::uint32_t>(0x8000)},
                {5u, static_cast<std::uint32_t>(30 + shift)},
                {6u, 0x00808080u},
                {7u, 0x00404040u},
                {kS4, kMenuItem}});
  }

  psx::present::FrameRecord drawPlacedMenuNumber(int shift) {
    return run(guest::kBootMenuNumberReturn - 8u,
               {{4u, static_cast<std::uint32_t>(50 + shift)},
                {5u, static_cast<std::uint32_t>(30 + shift)},
                {6u, 0x00808080u},
                {7u, 0x00404040u},
                {kS4, kMenuItem}});
  }

  // Overwrites everything a render could read besides its state.
  void scramble() {
    Core &c = core();
    table.scramble();
    const std::uint32_t ranges[][2] = {{kComponentA, 0x40u},
                                       {kStringMixed, 4u},
                                       {guest::kFontAdvance, 0x40u},
                                       {guest::kFontFlags, 0x40u},
                                       {guest::kFontTexture, 0x80u},
                                       {kTextureHeader, 0x10u},
                                       {kTextureBase, 0x40u},
                                       {guest::kDigitSets, 2u},
                                       {guest::kTintCounter, 1u},
                                       {guest::kTintTable + 0xC00u * 4u, 2u},
                                       {guest::kFontIndex, 1u},
                                       {guest::kTextureTablePointer, 1u},
                                       {guest::kDigitTextureTable, 1u}};
    for (const auto &[address, words] : ranges) {
      for (std::uint32_t word = 0; word < words; ++word) {
        c.mem_w32(address + word * 4u, 0xA5A5A5A5u);
      }
    }
    crashbash::test::setCamera(0x1234);
  }

  Core &core() {
    return game->core;
  }

  std::unique_ptr<Game> game;
  crashbash::test::OrderingTableModel table;

private:
  psx::present::FrameRecord drawTextAt(int x, int y, std::uint32_t tint) {
    writeTextRecord(core(), kComponentA, x, y, kStringMixed);
    core().mem_w32(kStackSlot, 0x00404040u);
    core().mem_w32(kStackSlot2, tint);
    return run(guest::kTextComponentDraw, {{4u, kComponentA}});
  }

  psx::present::FrameRecord run(std::uint32_t entry,
                                std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> registers) {
    table.begin();
    // A draw after a scramble finds the guest's data where it was.
    writeFont(core());
    writeString(core(), kStringMixed, "XQR");
    if (!call(game->core, entry, registers)) {
      std::abort();
    }
    crashbash::frameDriver(game->core).packetCollector().commit(game->core);
    return table.walk();
  }

  // Four corners 16 by 10 from (x, y) and their colours, in the 0x30-byte layout the shaded leaf reads.
  void writeCorners(int x, int y, int z = 0) {
    Core &c = core();
    const int offsets[4][2] = {{0, 0}, {16, 0}, {0, 10}, {16, 10}};
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      c.mem_w32(kComponentA + corner * 8u, pointWord(x + offsets[corner][0], y + offsets[corner][1]));
      c.mem_w32(kComponentA + corner * 8u + 4u, static_cast<std::uint32_t>(z));
      c.mem_w32(kComponentA + 0x20u + corner * 4u, 0x00304050u + corner * 0x101010u);
    }
  }
};

void checkUiRender(std::uint32_t producer, std::uint32_t object, psx::present::FrameRecord (UiScene::*draw)(int)) {
  UiScene scene;
  crashbash::test::checkStateRender(
      scene.core(),
      producer,
      object,
      [&](int shift) {
        return (scene.*draw)(shift);
      },
      [&] {
        scene.scramble();
      });
}

void checkComponentRender(std::uint32_t producer, psx::present::FrameRecord (UiScene::*draw)(int)) {
  checkUiRender(producer, incarnationObject(kComponentA, 0u), draw);
}

void test_the_text_render_draws_the_glyphs_the_guest_linked() {
  checkComponentRender(guest::kTextComponentDraw, &UiScene::drawText);
}

// A centred string is laid out again from its saved glyph widths.
void test_the_text_render_centres_the_line_it_was_given() {
  checkComponentRender(guest::kTextComponentDraw, &UiScene::drawCentredText);
}

void test_the_text_render_tints_the_colours_it_was_given() {
  checkComponentRender(guest::kTextComponentDraw, &UiScene::drawTintedText);
}

void test_the_panel_render_draws_the_parts_the_guest_linked() {
  checkComponentRender(guest::kPanelComponentDraw, &UiScene::drawPanel);
}

void test_the_panel_render_centres_the_rectangle_it_was_given() {
  checkComponentRender(guest::kPanelComponentDraw, &UiScene::drawCentredPanel);
}

void test_the_quad_render_draws_the_quad_the_guest_linked() {
  checkComponentRender(guest::kQuadComponentDraw, &UiScene::drawQuad);
}

// A quad projected through the GTE is re-projected through the blended camera.
void test_the_projected_quad_render_reprojects_through_the_moved_camera() {
  checkComponentRender(guest::kQuadComponentDraw, &UiScene::drawProjectedQuad);
}

void test_the_hud_icon_render_draws_the_icon_the_guest_linked() {
  checkUiRender(guest::kBootHudDraw, crashbash::render::ownerObject(kHudRecord), &UiScene::drawHudIcon);
}

void test_the_menu_string_render_draws_the_glyphs_the_guest_linked() {
  checkUiRender(guest::kBootMenuPageDraw, crashbash::render::ownerObject(kMenuItem), &UiScene::drawMenuString);
}

void test_the_menu_number_render_draws_the_digits_the_guest_linked() {
  checkUiRender(guest::kBootMenuPageDraw, crashbash::render::ownerObject(kMenuItem), &UiScene::drawPlacedMenuNumber);
}

void test_the_menu_number_render_centres_the_digits() {
  checkUiRender(guest::kBootMenuPageDraw, crashbash::render::ownerObject(kMenuItem), &UiScene::drawMenuNumber);
}

} // namespace

int main() {
  RUN(each_glyph_is_keyed_by_its_component_and_byte);
  RUN(glyph_keys_follow_the_component_not_the_order);
  RUN(a_new_string_in_a_component_gets_new_keys);
  RUN(a_glyph_without_a_texture_draws_no_packet);
  RUN(each_panel_part_is_its_own_element);
  RUN(a_string_outside_any_owner_stays_unkeyed);
  RUN(hud_digits_are_keyed_by_player_record_and_place);
  RUN(hud_icons_are_keyed_by_their_record);
  RUN(a_menu_item_number_is_keyed_by_item_and_place);
  RUN(boot_sites_need_the_boot_image);
  RUN(the_text_render_draws_the_glyphs_the_guest_linked);
  RUN(the_text_render_centres_the_line_it_was_given);
  RUN(the_text_render_tints_the_colours_it_was_given);
  RUN(the_panel_render_draws_the_parts_the_guest_linked);
  RUN(the_panel_render_centres_the_rectangle_it_was_given);
  RUN(the_quad_render_draws_the_quad_the_guest_linked);
  RUN(the_projected_quad_render_reprojects_through_the_moved_camera);
  RUN(the_hud_icon_render_draws_the_icon_the_guest_linked);
  RUN(the_menu_string_render_draws_the_glyphs_the_guest_linked);
  RUN(the_menu_number_render_draws_the_digits_the_guest_linked);
  RUN(the_menu_number_render_centres_the_digits);
  return pt_summary();
}
