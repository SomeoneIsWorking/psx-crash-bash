#include "gte_access.h"

#include "blend.h"
#include "core.h"
#include "gte_state.h"

#include <cstring>

namespace crashbash::render::gte {
namespace {

constexpr std::uint32_t kControlBase = 32u;

} // namespace

struct Guard::Saved {
  GteRawState raw{};
};

Guard::Guard() : saved_(new Saved) {
  GTE_SaveRawState(&saved_->raw);
}

Guard::~Guard() {
  GTE_RestoreRawState(&saved_->raw);
  delete saved_;
}

Control readControl() {
  GteRawState raw;
  GTE_SaveRawState(&raw);
  Control control;
  std::memcpy(control.data(), raw.reg + kControlBase, sizeof(control));
  return control;
}

void writeControl(const Control &control) {
  for (std::uint32_t reg = 0; reg < kControlRegisters; ++reg) {
    if (reg != kFlag) {
      gte_write_ctrl(reg, control[reg]);
    }
  }
}

Control blendControl(const Control &from, const Control &to, float t) {
  Control blended = to;
  for (std::uint32_t reg = 0; reg < kTranslationEnd; ++reg) {
    if (reg >= kRotationEnd) {
      blended[reg] = static_cast<std::uint32_t>(
          lerpInt(static_cast<std::int32_t>(from[reg]), static_cast<std::int32_t>(to[reg]), t));
      continue;
    }
    blended[reg] = lerpPoint(from[reg], to[reg], t);
  }
  return blended;
}

} // namespace crashbash::render::gte
