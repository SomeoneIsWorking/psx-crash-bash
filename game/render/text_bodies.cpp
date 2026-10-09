#include "text_bodies.h"

#include "core.h"
#include "crashbash_guest.h"
#include "placement_blend.h"
#include "ui_producer.h"
#include <lucent/log.h>

#include <algorithm>
#include <cstdlib>

namespace crashbash::render {
namespace {

constexpr std::uint32_t kLineFeed = 10u;
constexpr std::uint32_t kLayoutBase = 0x140u; // half the 640-column layout
constexpr std::uint32_t kLayoutColumns = 640u;
constexpr std::uint32_t kLongestString = 0x2000u;
constexpr std::uint32_t kFlagSprite = 1u;
constexpr std::uint32_t kFlagCombining = 2u;
constexpr std::uint32_t kFlagRaised = 4u;
constexpr std::int32_t kRaise = 7;
constexpr std::uint32_t kSpriteColour = 0x808080u;
constexpr std::uint32_t kTintReciprocal = 0x66666667u;
// The digit draw's three columns, in 640-column units from the first, and the width it centres.
constexpr std::int32_t kDigitCentreWidth = 0x1D880;
constexpr std::int32_t kDigitTens = 0x9D80;
constexpr std::int32_t kDigitUnits = 0x13B00;
constexpr std::uint32_t kDigitCount = 3u;

// The result registers FUN_80024008 and FUN_80024214 leave.
struct Registers {
  std::int32_t v0 = 0;
  std::int32_t v1 = 0;
};

std::int32_t halved(std::int32_t value) {
  return (value + static_cast<std::int32_t>(static_cast<std::uint32_t>(value) >> 31)) >> 1;
}

const GlyphEntry &glyphOf(const StringBody &body, std::uint8_t character) {
  const auto found = std::find_if(body.glyphs.begin(), body.glyphs.end(), [&](const GlyphEntry &glyph) {
    return glyph.character == character;
  });
  if (found == body.glyphs.end()) {
    lucent::error("text-bodies", "string at 0x{:08X} has no glyph entry for byte 0x{:02X}", body.address, character);
    std::abort();
  }
  return *found;
}

// FUN_80024008: where the line starting at `at` begins on screen. A centred line is centred on its width, an
// offset centred line is placed that far from the line's centre.
std::int32_t lineStart(const StringBody &body, const DrawGlobals &globals, std::size_t at, Registers &registers) {
  const std::int32_t x = body.x;
  if (!isCentred(body.x)) {
    registers = {x, x & 0xC000};
    return x;
  }
  std::int32_t width = 0;
  for (; at < body.text.size(); ++at) {
    const std::uint8_t character = body.text[at];
    if (character == kLineFeed || character == '\r') {
      break;
    }
    const GlyphEntry &glyph = glyphOf(body, character);
    if ((glyph.flags & kFlagCombining) == 0u) {
      width += glyph.advance;
    }
  }
  const std::int32_t scaled = layoutDivide(width * static_cast<std::int32_t>(kLayoutColumns), globals.screenScale);
  const std::int32_t start = centreOffset(body.x) != 0u
                                 ? static_cast<std::int32_t>(centreOffset(body.x)) - halved(scaled)
                                 : static_cast<std::int32_t>(kLayoutBase) - halved(scaled);
  registers.v0 = static_cast<std::int16_t>(start);
  registers.v1 = centreOffset(body.x) != 0u ? static_cast<std::int32_t>(static_cast<std::uint32_t>(scaled) >> 31)
                                            : static_cast<std::int32_t>(kLayoutBase);
  return registers.v0;
}

std::int32_t highWord(std::int32_t value) {
  return static_cast<std::int32_t>((static_cast<std::int64_t>(value) * static_cast<std::int32_t>(kTintReciprocal)) >>
                                   32);
}

// How far a channel moves for the product `x` of its distance and the scale.
std::int32_t tintStep(std::int32_t x) {
  return (highWord(x) >> 3) - (x >> 31);
}

// FUN_80024214: the first colour moves each channel toward white, the second toward white in red and green and
// toward black in blue, by the scale. Returns the registers the guest leaves.
Registers tint(std::uint32_t &first, std::uint32_t &second, std::int32_t scaleEntry) {
  const std::int32_t scale = (scaleEntry * 5) >> 10;
  Registers registers;
  std::uint32_t *colours[2] = {&first, &second};
  for (std::uint32_t which = 0; which < 2u; ++which) {
    std::uint32_t moved = *colours[which] & 0xFF000000u;
    for (std::uint32_t channel = 0; channel < 3u; ++channel) {
      const auto value = static_cast<std::int32_t>((*colours[which] >> (channel * 8u)) & 0xFFu);
      const std::int32_t distance = (which == 1u && channel == 2u) ? -value : 0xFF - value;
      const auto x =
          static_cast<std::int32_t>(static_cast<std::uint32_t>(distance) * static_cast<std::uint32_t>(scale));
      moved |= (static_cast<std::uint32_t>(value + tintStep(x)) & 0xFFu) << (channel * 8u);
      if (which == 1u && channel == 0u) {
        registers.v1 = x >> 31;
      }
      if (which == 1u && channel == 2u) {
        registers.v0 = tintStep(x);
      }
    }
    *colours[which] = moved;
  }
  return registers;
}

std::uint32_t pack(std::int32_t y, std::int32_t x) {
  return (static_cast<std::uint32_t>(y) << 16) | (static_cast<std::uint32_t>(x) & 0xFFFFu);
}

LeafCall glyphCall(const StringBody &body,
                   const GlyphEntry &glyph,
                   std::uint32_t position,
                   std::uint32_t colour,
                   std::uint32_t colour2) {
  LeafCall call;
  call.kind = (glyph.flags & kFlagSprite) == 0u ? LeafKind::Quad : LeafKind::Sprite;
  call.textureAddress =
      body.textureBase + (static_cast<std::uint32_t>(glyph.texture) & 0xFFFu) * guest::kTextureRecordBytes;
  call.position = position;
  if (call.kind == LeafKind::Quad) {
    call.colours = {colour, colour, colour2, colour2};
  } else {
    call.colours[0] = kSpriteColour;
  }
  return call;
}

} // namespace

StringBody readStringBody(Core &core) {
  const std::uint32_t sp = core.r[29];
  StringBody body;
  body.x = static_cast<std::int16_t>(core.r[4]);
  body.y = static_cast<std::int16_t>(core.r[5]);
  body.address = core.r[6];
  body.colour = core.r[7];
  body.colour2 = core.mem_r32(sp + 0x10u);
  body.tinted = core.mem_r32(sp + 0x14u);
  body.fontIndex = core.mem_r32(guest::kFontIndex);
  if (body.tinted != 0u) {
    const std::uint32_t index = (((core.mem_r32(guest::kTintCounter) << 7) & 0x7FFu) - 0x400u) & 0xFFFu;
    body.tintScale = static_cast<std::int16_t>(core.mem_r16(guest::kTintTable + index * 4u + 2u));
  }
  const std::uint32_t row = body.fontIndex * 0x100u;
  body.lineHeight = static_cast<std::int8_t>(core.mem_r8(guest::kFontAdvance + row));
  for (std::uint32_t at = 0;; ++at) {
    if (at == kLongestString) {
      lucent::error("text-bodies", "string at 0x{:08X} has no terminator in {} bytes", body.address, kLongestString);
      std::abort();
    }
    const std::uint8_t character = core.mem_r8(body.address + at);
    if (character == 0u) {
      break;
    }
    body.text.push_back(character);
  }
  bool draws = false;
  for (const std::uint8_t character : body.text) {
    if (std::any_of(body.glyphs.begin(), body.glyphs.end(), [&](const GlyphEntry &glyph) {
          return glyph.character == character;
        })) {
      continue;
    }
    GlyphEntry glyph;
    glyph.character = character;
    glyph.advance = static_cast<std::int8_t>(core.mem_r8(guest::kFontAdvance + row + character));
    glyph.flags = core.mem_r8(guest::kFontFlags + row + character);
    glyph.texture = static_cast<std::int16_t>(core.mem_r16(guest::kFontTexture + row * 2u + character * 2u));
    draws = draws || glyph.texture != -1;
    body.glyphs.push_back(glyph);
  }
  if (draws) {
    body.textureBase = core.mem_r32(core.mem_r32(guest::kTextureTablePointer) + guest::kTextureTableOffset);
  }
  return body;
}

StringOutcome runString(const StringBody &body, const DrawGlobals &globals, LeafPort &port) {
  StringOutcome outcome;
  outcome.colour = body.colour;
  outcome.colour2 = body.colour2;
  Registers registers;
  std::int32_t start = lineStart(body, globals, 0, registers);
  if (body.tinted != 0u) {
    registers = tint(outcome.colour, outcome.colour2, body.tintScale);
  }
  std::uint32_t position = pack(body.y, 0);
  std::int32_t cursor = 0;
  std::size_t at = 0;
  const auto byteAt = [&](std::size_t index) -> std::uint32_t {
    return index < body.text.size() ? body.text[index] : 0u;
  };
  while (byteAt(at) != 0u) {
    const std::uint32_t character = byteAt(at);
    const std::size_t glyphAt = at++;
    if (character == kLineFeed || character == '\r') {
      cursor = 0;
      std::int32_t step = body.lineHeight;
      if (character == '\r') {
        std::int32_t lines = (static_cast<std::int32_t>(byteAt(at++)) - '0') * 10 - '0';
        lines += static_cast<std::int32_t>(byteAt(at++));
        if (lines >= 0x33) {
          lines = -(lines - 0x32);
        }
        step = lines;
      }
      const std::int32_t line = static_cast<std::int16_t>(position >> 16) + step * 2;
      position = pack(line, static_cast<std::int32_t>(position & 0xFFFFu));
      start = lineStart(body, globals, at, registers);
      continue;
    }
    const GlyphEntry &glyph = glyphOf(body, static_cast<std::uint8_t>(character));
    if ((glyph.flags & kFlagCombining) != 0u) {
      cursor -= glyph.advance;
    }
    if (glyph.texture != -1) {
      const std::int32_t offset = layoutDivide(cursor * static_cast<std::int32_t>(kLayoutColumns), globals.screenScale);
      position = pack(static_cast<std::int16_t>(position >> 16), start + offset);
      const auto element = glyphElement(body.address + static_cast<std::uint32_t>(glyphAt));
      const bool raised = (glyph.flags & kFlagSprite) != 0u && (glyph.flags & kFlagRaised) != 0u;
      const std::uint32_t drawAt = raised ? pack(static_cast<std::int16_t>(position >> 16) - kRaise,
                                                 static_cast<std::int32_t>(position & 0xFFFFu))
                                          : position;
      const LeafExecution done = port.draw(glyphCall(body, glyph, drawAt, outcome.colour, outcome.colour2), element);
      if (done.setsSecond) {
        registers.v1 = static_cast<std::int32_t>(done.second);
      }
    } else {
      registers.v1 = static_cast<std::int32_t>(body.fontIndex << 9);
    }
    registers.v0 = glyph.advance;
    cursor += glyph.advance;
  }
  outcome.result.v0 = static_cast<std::uint32_t>(registers.v0);
  outcome.result.v1 = static_cast<std::uint32_t>(registers.v1);
  return outcome;
}

StringBody blendString(const StringBody &from, const StringBody &to, float t) {
  StringBody body = to;
  body.x = blendCoordinate(from.x, to.x, t);
  body.y = blendHalf(from.y, to.y, t);
  return body;
}

namespace {

struct StringFixed {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint32_t colour = 0;
  std::uint32_t colour2 = 0;
  std::uint32_t tinted = 0;
  std::int32_t tintScale = 0;
  std::uint32_t fontIndex = 0;
  std::int32_t lineHeight = 0;
  std::uint32_t textureBase = 0;
  std::uint32_t address = 0;
  std::uint32_t textLength = 0;
  std::uint32_t glyphCount = 0;
};

} // namespace

void writeString(psx::present::StateWriter &writer, const StringBody &body) {
  writer.put(StringFixed{body.x,
                         body.y,
                         body.colour,
                         body.colour2,
                         body.tinted,
                         body.tintScale,
                         body.fontIndex,
                         body.lineHeight,
                         body.textureBase,
                         body.address,
                         static_cast<std::uint32_t>(body.text.size()),
                         static_cast<std::uint32_t>(body.glyphs.size())});
  writer.putAll(std::span<const std::uint8_t>(body.text));
  writer.putAll(std::span<const GlyphEntry>(body.glyphs));
}

StringBody readString(psx::present::StateReader &reader) {
  const auto fixed = reader.get<StringFixed>();
  StringBody body;
  body.x = fixed.x;
  body.y = fixed.y;
  body.colour = fixed.colour;
  body.colour2 = fixed.colour2;
  body.tinted = fixed.tinted;
  body.tintScale = fixed.tintScale;
  body.fontIndex = fixed.fontIndex;
  body.lineHeight = fixed.lineHeight;
  body.textureBase = fixed.textureBase;
  body.address = fixed.address;
  body.text = reader.getAll<std::uint8_t>(fixed.textLength);
  body.glyphs = reader.getAll<GlyphEntry>(fixed.glyphCount);
  return body;
}

NumberBody readNumberBody(Core &core) {
  NumberBody body;
  body.x = static_cast<std::int16_t>(core.r[4]);
  body.y = static_cast<std::int16_t>(core.r[5]);
  body.colour = core.r[6];
  body.colour2 = core.r[7];
  const std::uint32_t set = guest::kDigitSets + core.mem_r32(core.r[29] + 0x10u) * guest::kDigitSetBytes;
  for (std::uint32_t digit = 0; digit < kDigitCount; ++digit) {
    body.digits[digit] = core.mem_r16(set + digit * 2u);
  }
  body.textureBase = core.mem_r32(guest::kDigitTextureTable);
  return body;
}

BodyResult runNumber(const NumberBody &body, const DrawGlobals &globals, LeafPort &port) {
  std::int32_t x = body.x;
  if (isCentred(body.x)) {
    x = static_cast<std::int32_t>(kLayoutBase) - halved(layoutDivide(kDigitCentreWidth, globals.screenScale));
  }
  const std::int32_t columns[kDigitCount] = {0, kDigitTens, kDigitUnits};
  const DigitPlace places[kDigitCount] = {DigitPlace::Hundreds, DigitPlace::Tens, DigitPlace::Units};
  BodyResult result;
  for (std::uint32_t digit = 0; digit < kDigitCount; ++digit) {
    LeafCall call;
    call.kind = LeafKind::Quad;
    call.textureAddress = body.textureBase + (body.digits[digit] & 0xFFFu) * guest::kTextureRecordBytes;
    const std::int32_t column = digit == 0u ? 0 : layoutDivide(columns[digit], globals.screenScale);
    call.position = pack(body.y, x + column);
    call.colours = {body.colour, body.colour, body.colour2, body.colour2};
    result.take(port.draw(call, static_cast<std::uint32_t>(places[digit])));
  }
  return result;
}

NumberBody blendNumber(const NumberBody &from, const NumberBody &to, float t) {
  NumberBody body = to;
  body.x = blendCoordinate(from.x, to.x, t);
  body.y = blendHalf(from.y, to.y, t);
  return body;
}

void writeNumber(psx::present::StateWriter &writer, const NumberBody &body) {
  writer.put(body);
}

NumberBody readNumber(psx::present::StateReader &reader) {
  return reader.get<NumberBody>();
}

} // namespace crashbash::render
