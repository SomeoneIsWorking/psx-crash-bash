#include "render/context_motion.h"

#include "core.h"
#include "execution/guest_execution.h"
#include "runtime/session.h"
#include "title/guest.h"

#include <algorithm>

namespace crashbash::render {
namespace {

void deriveView(Core *core) {
  session(*core).drawReplay.noteSelected(core->r[4]);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kDeriveView);
}

} // namespace

void ContextMotion::noteSelected(std::uint32_t context) {
  if (std::ranges::find(selected_, context) == selected_.end()) {
    selected_.push_back(context);
  }
}

void ContextMotion::openDraw() {
  selected_.clear();
}

void ContextMotion::forget() {
  selected_.clear();
  earlier_.clear();
  later_.clear();
}

void ContextMotion::sample(Core &core) {
  earlier_.swap(later_);
  later_.clear();
  for (const std::uint32_t context : selected_) {
    Record record{.address = context};
    for (std::size_t field = 0; field < kFields.size(); ++field) {
      record.words[field] = core.mem_r32(context + kFields[field].offset);
    }
    later_.push_back(record);
  }
}

psx::present::MotionCount ContextMotion::write(psx::present::GuestRedraw &redraw, float t) const {
  psx::present::MotionCount count{.drawn = later_.size()};
  for (const Record &to : later_) {
    const auto from = std::ranges::find(earlier_, to.address, &Record::address);
    if (from == earlier_.end()) {
      ++count.unpaired;
      continue;
    }
    for (std::size_t field = 0; field < kFields.size(); ++field) {
      redraw.writeWord(to.address + kFields[field].offset,
                       psx::present::blendedWord(kFields[field].motion, from->words[field], to.words[field], t));
    }
  }
  return count;
}

void registerContextViewNotes(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kDeriveView, "CrashBash::DeriveView", deriveView);
}

} // namespace crashbash::render
