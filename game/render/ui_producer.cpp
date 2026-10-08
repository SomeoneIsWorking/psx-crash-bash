#include "ui_producer.h"

#include "component_incarnation.h"
#include "core.h"
#include "crashbash_frame_driver.h"
#include "crashbash_guest.h"
#include "guest_execution.h"

#include <algorithm>
#include <iterator>

namespace crashbash::render {
namespace {

using runtime::GuestImage;

// What a call site says about the packets the called function writes.
struct CallSiteName {
  enum class Kind { None, Element, Owner };
  Kind kind = Kind::None;
  std::uint32_t producer = 0;
  std::uint32_t object = 0;
  std::uint32_t element = 0;
};

CallSiteName element(std::uint32_t index) {
  return {CallSiteName::Kind::Element, 0u, 0u, index};
}

CallSiteName element(PanelPart part) {
  return element(static_cast<std::uint32_t>(part));
}

CallSiteName element(DigitPlace place) {
  return element(static_cast<std::uint32_t>(place));
}

CallSiteName owner(std::uint32_t producer, std::uint32_t object, DigitPlace place = DigitPlace::Units) {
  return {CallSiteName::Kind::Owner, producer, object, static_cast<std::uint32_t>(place)};
}

template <std::size_t N> bool among(const std::uint32_t (&sites)[N], std::uint32_t site) {
  return std::find(std::begin(sites), std::end(sites), site) != std::end(sites);
}

CallSiteName bootSiteName(const Core &core, std::uint32_t site) {
  if (among(guest::kBootHudIconReturns, site)) {
    return owner(guest::kBootHudDraw, core.r[guest::kBootHudIconRegister]);
  }
  const std::uint32_t player = core.r[guest::kBootHudDigitRegister];
  if (site == guest::kBootHudThreeDigitReturns[0] || site == guest::kBootHudTwoDigitReturns[0] ||
      site == guest::kBootHudOneDigitReturn) {
    return owner(guest::kBootHudDraw, player, DigitPlace::Units);
  }
  if (site == guest::kBootHudThreeDigitReturns[1] || site == guest::kBootHudTwoDigitReturns[1]) {
    return owner(guest::kBootHudDraw, player, DigitPlace::Tens);
  }
  if (site == guest::kBootHudThreeDigitReturns[2]) {
    return owner(guest::kBootHudDraw, player, DigitPlace::Hundreds);
  }
  if (among(guest::kBootMenuStringReturns, site) || site == guest::kBootMenuNumberReturn) {
    return owner(guest::kBootMenuPageDraw, core.r[guest::kBootMenuItemRegister]);
  }
  return {};
}

CallSiteName siteName(Core &core) {
  const std::uint32_t site = core.r[31];
  switch (site) {
  case guest::kStringGlyphQuadReturn:
  case guest::kStringGlyphRaisedReturn:
  case guest::kStringGlyphSpriteReturn:
    return element(glyphElement(core.r[guest::kStringCursorRegister] - 1u));
  case guest::kNumberHundredsReturn:
    return element(DigitPlace::Hundreds);
  case guest::kNumberTensReturn:
    return element(DigitPlace::Tens);
  case guest::kNumberUnitsReturn:
    return element(DigitPlace::Units);
  case guest::kPanelBodyReturn:
  case guest::kQuadComponentReturn:
    return element(PanelPart::Body);
  case guest::kPanelLeftReturn:
    return element(PanelPart::Left);
  case guest::kPanelRightReturn:
    return element(PanelPart::Right);
  case guest::kPanelTopReturn:
    return element(PanelPart::Top);
  case guest::kPanelBottomReturn:
    return element(PanelPart::Bottom);
  default:
    break;
  }
  if (runtime::imageHolds(core, GuestImage::Boot, site)) {
    return bootSiteName(core, site);
  }
  return {};
}

// Runs the resident function at `address` under the key its call site names.
void callNamed(Core *core, std::uint32_t address) {
  const CallSiteName name = siteName(*core);
  if (name.kind == CallSiteName::Kind::Owner) {
    const psx::present::EmissionScope::Guard scope(core->emission, name.producer, name.object, name.element);
    runtime::callOriginal(*core, GuestImage::Resident, address);
    return;
  }
  // An element names a part of the object whose scope is open; with none there is nothing to name.
  if (name.kind == CallSiteName::Kind::Element && core->emission.isOpen()) {
    const auto scope = core->emission.element(name.element);
    runtime::callOriginal(*core, GuestImage::Resident, address);
    return;
  }
  runtime::callOriginal(*core, GuestImage::Resident, address);
}

void imageQuad(Core *core) {
  callNamed(core, guest::kImageQuad);
}

void imageSprite(Core *core) {
  callNamed(core, guest::kImageSprite);
}

void shadedQuad(Core *core) {
  callNamed(core, guest::kShadedQuad);
}

void stringDraw(Core *core) {
  callNamed(core, guest::kStringDraw);
}

void numberDraw(Core *core) {
  callNamed(core, guest::kNumberDraw);
}

void componentDraw(Core *core, std::uint32_t address) {
  const auto object = core->emission.instance(frameDriver(*core).componentIncarnations().object(core->r[4]));
  runtime::callOriginal(*core, GuestImage::Resident, address);
}

void textComponentDraw(Core *core) {
  componentDraw(core, guest::kTextComponentDraw);
}

void panelComponentDraw(Core *core) {
  componentDraw(core, guest::kPanelComponentDraw);
}

void quadComponentDraw(Core *core) {
  componentDraw(core, guest::kQuadComponentDraw);
}

} // namespace

void registerUiProducers(Core &core) {
  const psx::present::Producer component{psx::present::Arg::A0};
  runtime::registerNativeOverride(core,
                                  GuestImage::Resident,
                                  guest::kTextComponentDraw,
                                  "CrashBash::TextComponentDraw",
                                  textComponentDraw,
                                  component);
  runtime::registerNativeOverride(core,
                                  GuestImage::Resident,
                                  guest::kPanelComponentDraw,
                                  "CrashBash::PanelComponentDraw",
                                  panelComponentDraw,
                                  component);
  runtime::registerNativeOverride(core,
                                  GuestImage::Resident,
                                  guest::kQuadComponentDraw,
                                  "CrashBash::QuadComponentDraw",
                                  quadComponentDraw,
                                  component);
  runtime::registerNativeOverride(core, GuestImage::Resident, guest::kImageQuad, "CrashBash::ImageQuad", imageQuad);
  runtime::registerNativeOverride(
      core, GuestImage::Resident, guest::kImageSprite, "CrashBash::ImageSprite", imageSprite);
  runtime::registerNativeOverride(core, GuestImage::Resident, guest::kShadedQuad, "CrashBash::ShadedQuad", shadedQuad);
  runtime::registerNativeOverride(core, GuestImage::Resident, guest::kStringDraw, "CrashBash::StringDraw", stringDraw);
  runtime::registerNativeOverride(core, GuestImage::Resident, guest::kNumberDraw, "CrashBash::NumberDraw", numberDraw);
}

} // namespace crashbash::render
