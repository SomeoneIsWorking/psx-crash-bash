// The UI producers key text glyphs, panel parts and HUD digits by their element, through the shipping
// overrides, with guest stand-ins placed at the real call sites.
#include "component_incarnation.h"
#include "core.h"
#include "crashbash_guest.h"
#include "game.h"
#include "guest_execution.h"
#include "testutil.h"
#include "title_adapter.h"
#include "ui_producer.h"

#include <initializer_list>
#include <memory>
#include <utility>

namespace {

namespace guest = crashbash::guest;
using crashbash::render::DigitPlace;
using crashbash::render::glyphElement;
using crashbash::render::incarnationObject;
using crashbash::render::PanelPart;
using psx::present::RecordKey;

constexpr GuestAddressRange kResidentRange{0x18000u, 0x2A000u};
constexpr GuestAddressRange kBootRange{0x78C90u, 0xA7490u};
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr std::uint32_t kPackets = 0x80140000u;
constexpr std::uint32_t kComponentA = 0x80150000u;
constexpr std::uint32_t kComponentB = 0x801500A8u;
constexpr std::uint32_t kStringA = 0x80120000u;
constexpr std::uint32_t kStringB = 0x80120100u;
constexpr std::uint32_t kHudRecord = 0x8009AD7Cu;
constexpr std::uint32_t kMenuItem = 0x80099000u;

constexpr std::uint32_t kT0 = 8u;
constexpr std::uint32_t kS2 = 18u;
constexpr std::uint32_t kS3 = 19u;
constexpr std::uint32_t kS4 = 20u;
constexpr std::uint32_t kS5 = 21u;
constexpr std::uint32_t kT9 = 25u;
constexpr std::uint32_t kRa = 31u;

constexpr std::uint32_t nop() {
  return 0u;
}
constexpr std::uint32_t j(std::uint32_t target) {
  return 0x08000000u | ((target >> 2) & 0x3FFFFFFu);
}
constexpr std::uint32_t jal(std::uint32_t target) {
  return 0x0C000000u | ((target >> 2) & 0x3FFFFFFu);
}
constexpr std::uint32_t jr(std::uint32_t rs) {
  return (rs << 21) | 8u;
}
constexpr std::uint32_t move(std::uint32_t rd, std::uint32_t rs) {
  return (rs << 21) | (rd << 11) | 0x21u;
}
constexpr std::uint32_t addiu(std::uint32_t rt, std::uint32_t rs, std::uint32_t imm) {
  return 0x24000000u | (rs << 21) | (rt << 16) | (imm & 0xFFFFu);
}
constexpr std::uint32_t lbu(std::uint32_t rt, std::uint32_t rs) {
  return 0x90000000u | (rs << 21) | (rt << 16);
}
constexpr std::uint32_t sw(std::uint32_t rt, std::uint32_t rs, std::uint32_t offset = 0u) {
  return 0xAC000000u | (rs << 21) | (rt << 16) | (offset & 0xFFFFu);
}
constexpr std::uint32_t beqz(std::uint32_t rs, std::uint32_t pc, std::uint32_t target) {
  return 0x10000000u | (rs << 21) | (((target - (pc + 4u)) >> 2) & 0xFFFFu);
}

void place(Core &core, std::uint32_t address, std::initializer_list<std::uint32_t> words) {
  for (const std::uint32_t word : words) {
    core.mem_w32(address, word);
    address += 4u;
  }
}

// A call at `site - 8` that returns to `site`, then jumps on.
void callThenJump(Core &core, std::uint32_t site, std::uint32_t callee, std::uint32_t next) {
  place(core, site - 8u, {jal(callee), nop(), j(next), nop()});
}

void callThenReturn(Core &core, std::uint32_t site, std::uint32_t callee, std::uint32_t link) {
  place(core, site - 8u, {jal(callee), nop(), jr(link), nop()});
}

void stubGuest(Core &core) {
  // Each 2D leaf writes the command word of one packet at t9.
  for (const std::uint32_t leaf : {guest::kImageQuad, guest::kImageSprite, guest::kShadedQuad}) {
    place(core, leaf, {sw(0u, kT9, 4u), jr(kRa), addiu(kT9, kT9, 4u)});
  }
  // FUN_800243A0: s2 walks the string; each glyph's quad returns to 0x80024688.
  place(core, guest::kStringDraw, {move(kS3, kRa), j(0x80024680u), addiu(kS2, 6u, 1u)});
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
  return game;
}

bool call(Core &core, std::uint32_t entry, std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> registers) {
  core.r[kT9] = kPackets;
  core.r[kS5] = kReturn;
  core.r[29] = 0x801FFF00u;
  core.r[kRa] = kReturn;
  for (const auto &[index, value] : registers) {
    core.r[index] = value;
  }
  return psx::cpu::dispatchGuest(core, entry, psx::cpu::ExecutionBudget::fromCycles(100000u)).returned();
}

bool drawText(Core &core, std::uint32_t component, std::uint32_t string, std::uint32_t packets) {
  return call(core, guest::kTextComponentDraw, {{4u, component}, {6u, string}, {kT9, packets}});
}

RecordKey glyphKey(std::uint32_t component, std::uint32_t string, std::uint32_t glyph) {
  return RecordKey{guest::kTextComponentDraw, incarnationObject(component, 0u), glyphElement(string + glyph), 0u};
}

void test_each_glyph_is_keyed_by_its_component_and_byte() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawText(core, kComponentA, kStringB, kPackets));
  for (std::uint32_t glyph = 0; glyph < 3u; ++glyph) {
    CHECK(core.emission.identityFor(kPackets + glyph * 4u) == glyphKey(kComponentA, kStringB, glyph));
  }
  CHECK(!core.emission.identityFor(kPackets + 12u));
}

// Two components showing one string, drawn in either order, keep distinct keys of their own.
void test_glyph_keys_follow_the_component_not_the_order() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawText(core, kComponentA, kStringA, kPackets));
  CHECK(drawText(core, kComponentB, kStringA, kPackets + 0x100u));
  CHECK(core.emission.identityFor(kPackets + 4u) == glyphKey(kComponentA, kStringA, 1u));
  CHECK(core.emission.identityFor(kPackets + 0x104u) == glyphKey(kComponentB, kStringA, 1u));
  CHECK(drawText(core, kComponentB, kStringA, kPackets));
  CHECK(drawText(core, kComponentA, kStringA, kPackets + 0x100u));
  CHECK(core.emission.identityFor(kPackets + 4u) == glyphKey(kComponentB, kStringA, 1u));
  CHECK(core.emission.identityFor(kPackets + 0x104u) == glyphKey(kComponentA, kStringA, 1u));
}

// A component switched to another string names other glyphs, so nothing pairs across the switch.
void test_a_new_string_in_a_component_gets_new_keys() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(drawText(core, kComponentA, kStringB, kPackets));
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
        core.emission.identityFor(kPackets + i * 4u) ==
        (RecordKey{
            guest::kPanelComponentDraw, incarnationObject(kComponentA, 0u), static_cast<std::uint32_t>(parts[i]), 0u}));
  }
}

// Outside a component the string renderer has no object to name glyphs of.
void test_a_string_outside_any_owner_stays_unkeyed() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(call(core, guest::kStringDraw, {{6u, kStringA}}));
  CHECK(!core.emission.identityFor(kPackets));
}

RecordKey hudKey(std::uint32_t record, DigitPlace place) {
  return RecordKey{guest::kBootHudDraw, record, static_cast<std::uint32_t>(place), 0u};
}

void test_hud_digits_are_keyed_by_player_record_and_place() {
  auto game = makeGame();
  Core &core = game->core;
  CHECK(call(core, guest::kBootHudTwoDigitReturns[0] - 8u, {{kS4, kHudRecord}}));
  CHECK(call(core, guest::kBootHudTwoDigitReturns[1] - 8u, {{kS4, kHudRecord}, {kT9, kPackets + 4u}}));
  CHECK(call(core, guest::kBootHudThreeDigitReturns[2] - 8u, {{kS4, kHudRecord}, {kT9, kPackets + 8u}}));
  CHECK(core.emission.identityFor(kPackets) == hudKey(kHudRecord, DigitPlace::Units));
  CHECK(core.emission.identityFor(kPackets + 4u) == hudKey(kHudRecord, DigitPlace::Tens));
  CHECK(core.emission.identityFor(kPackets + 8u) == hudKey(kHudRecord, DigitPlace::Hundreds));
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
    CHECK(core.emission.identityFor(kPackets + i * 4u) ==
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
  return pt_summary();
}
