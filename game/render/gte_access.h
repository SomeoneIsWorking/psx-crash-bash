// game/render/gte_access.h — the GTE as the native bodies and their renders use it.
//
// A body runs the guest's own GTE sequence (RTPS, RTPT, AVSZ3, NCLIP) through psxport's GTE, once over the
// live registers when the guest calls it and once per render over registers that hold the saved call's
// transform. `Guard` hands the guest its registers back after a render.
#pragma once

#include <array>
#include <cstdint>

namespace crashbash::render::gte {

// Data registers.
inline constexpr std::uint32_t kVxy0 = 0;
inline constexpr std::uint32_t kVz0 = 1;
inline constexpr std::uint32_t kVxy1 = 2;
inline constexpr std::uint32_t kVz1 = 3;
inline constexpr std::uint32_t kVxy2 = 4;
inline constexpr std::uint32_t kVz2 = 5;
inline constexpr std::uint32_t kOtz = 7;
inline constexpr std::uint32_t kSxy0 = 12;
inline constexpr std::uint32_t kSxy1 = 13;
inline constexpr std::uint32_t kSxy2 = 14;
inline constexpr std::uint32_t kMac0 = 24;

// Control registers: 0..4 the rotation matrix, 5..7 the translation, 31 the overflow flags.
inline constexpr std::uint32_t kRotationEnd = 5;
inline constexpr std::uint32_t kTranslationEnd = 8;
inline constexpr std::uint32_t kFlag = 31;
inline constexpr std::uint32_t kControlRegisters = 32;

// Command words (COP2 opcode and the guest's command field).
inline constexpr std::uint32_t kRtps = 0x4A180001u;
inline constexpr std::uint32_t kRtpt = 0x4A280030u;
inline constexpr std::uint32_t kNclip = 0x4B400006u;
inline constexpr std::uint32_t kAvsz3 = 0x4B58002Du;

using Control = std::array<std::uint32_t, kControlRegisters>;

// The control registers, without clearing the flags.
Control readControl();
// Writes every control register but the flags.
void writeControl(const Control &control);
// The control registers `t` of the way from `from` to `to`: rotation elements and translation move, the
// rest are `to`'s.
Control blendControl(const Control &from, const Control &to, float t);

// The GTE as the render found it, handed back when the render is done.
class Guard {
public:
  Guard();
  ~Guard();
  Guard(const Guard &) = delete;
  Guard &operator=(const Guard &) = delete;

private:
  struct Saved;
  Saved *saved_;
};

} // namespace crashbash::render::gte
