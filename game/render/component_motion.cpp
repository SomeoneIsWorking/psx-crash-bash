#include "render/component_motion.h"

#include "blend.h"
#include "core.h"
#include "execution/guest_execution.h"
#include "render/component_incarnation.h"
#include "runtime/session.h"
#include "title/guest.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace crashbash::render {
namespace {

using psx::present::Motion;

constexpr std::size_t kPosition = 0;
constexpr std::size_t kAngles = 3;
constexpr std::size_t kScale = 5;
constexpr std::size_t kMatrix = 8;
constexpr std::size_t kFlags = 16;
constexpr std::size_t kPose = 17;
constexpr std::size_t kDrawFlags = 18;
constexpr std::size_t kFrameCode = 19;
static_assert(ComponentMotion::kWordOffsets[kAngles] == 0x10u && ComponentMotion::kWordOffsets[kMatrix] == 0x30u);
static_assert(ComponentMotion::kWordOffsets[kFlags] == 0x50u && ComponentMotion::kWordOffsets[kPose] == 0x70u);
static_assert(ComponentMotion::kWordOffsets[kDrawFlags] == 0x00u && ComponentMotion::kWordOffsets[kFrameCode] == 0x74u);

// 0x80019A60's frame codes: kind 0x4000 is keyframe `code & 0x7F` of animation `code & 0xF80`. With the draw flag's
// sign bit set it draws frame +0x72 weighted toward the next by +0x70.
constexpr std::uint32_t kFrameKind = 0x7000u;
constexpr std::uint32_t kKeyframeKind = 0x4000u;
constexpr std::uint32_t kAnimation = 0x0F80u;
constexpr std::uint32_t kKeyframe = 0x007Fu;
constexpr std::uint32_t kWeighted = 0x80000000u;

constexpr std::uint32_t kModel = 0x6Cu;
// 0x80019A60 reads the component's rotation from the draw context, not its own, for this rotation order (+0x51).
constexpr std::uint32_t kContextRotation = 4u;
// FUN_8001965C rebuilds +0x30 from the angles at +0x10 while this halfword is set, and clears it.
constexpr std::uint32_t kRebuild = 0x52u;

std::uint32_t rotationOrder(std::uint32_t flags) {
  return (flags >> 8) & 0xFFu;
}

std::uint32_t rebuild(std::uint32_t flags) {
  return flags >> 16;
}

// 0x80019A60's keyframe pose: frame (s16, high) and the u16 progress toward the next (low), blended as one phase.
struct PoseStep {
  std::uint32_t from;
  std::uint32_t to;
};

std::uint32_t blendPose(PoseStep step, float t) {
  const std::uint32_t from = step.from;
  const std::uint32_t to = step.to;
  const auto frameFrom = static_cast<std::int16_t>(from >> 16);
  const auto frameTo = static_cast<std::int16_t>(to >> 16);
  const std::uint32_t progressFrom = from & 0xFFFFu;
  const std::uint32_t progressTo = to & 0xFFFFu;
  const bool advanced = frameTo == frameFrom ? progressTo >= progressFrom : frameTo == frameFrom + 1;
  if (!advanced) {
    return to;
  }
  constexpr double kFrame = 65536.0;
  const double phase = psx::present::lerpExact(frameFrom * kFrame + progressFrom, frameTo * kFrame + progressTo, t);
  const double frame = std::floor(phase / kFrame);
  const auto progress = static_cast<std::uint32_t>(phase - frame * kFrame);
  return static_cast<std::uint32_t>(static_cast<std::int32_t>(frame)) << 16 | (progress & 0xFFFFu);
}

// FUN_8001ACC0(component) turns a root component into the matrix 0x80056A44 its children draw by.
void rootTransform(Core *core) {
  session(*core).drawReplay.noteDrawn(core->r[4]);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kRootTransform);
}

void modelDraw(Core *core) {
  session(*core).drawReplay.noteDrawn(core->r[7]);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kModelDraw);
}

// A keyframe step of one animation is drawn the way the guest draws a weighted pose: from frame k toward k + 1.
bool writeKeyframeStep(psx::present::GuestRedraw &redraw,
                       std::uint32_t component,
                       std::span<const std::uint32_t> from,
                       std::span<const std::uint32_t> to,
                       float t) {
  const std::uint32_t codeFrom = from[kFrameCode] & 0xFFFFu;
  const std::uint32_t codeTo = to[kFrameCode] & 0xFFFFu;
  const bool stepped = (codeFrom & kFrameKind) == kKeyframeKind && (codeTo & kFrameKind) == kKeyframeKind &&
                       (codeFrom & kAnimation) == (codeTo & kAnimation) &&
                       (codeTo & kKeyframe) == (codeFrom & kKeyframe) + 1u;
  if (!stepped || ((from[kDrawFlags] | to[kDrawFlags]) & kWeighted) != 0u) {
    return false;
  }
  constexpr float kProgress = 65536.0f;
  const auto progress = static_cast<std::uint32_t>(std::clamp(t * kProgress, 1.0f, kProgress - 1.0f));
  redraw.writeHalf(component + ComponentMotion::kWordOffsets[kFrameCode], static_cast<std::uint16_t>(codeFrom));
  redraw.writeWord(component + ComponentMotion::kWordOffsets[kPose], (codeFrom & kKeyframe) << 16 | progress);
  redraw.writeWord(component, to[kDrawFlags] | kWeighted);
  return true;
}

} // namespace

void ComponentMotion::noteDrawn(std::uint32_t component) {
  // BOOT 0x8009440C refills its scratch component from a table inside the draw: the table is what moves.
  if (component != guest::kBootScratchComponent) {
    drawn_.push_back(component);
  }
}

void ComponentMotion::openDraw() {
  drawn_.clear();
}

void ComponentMotion::forget() {
  drawn_.clear();
  earlier_.clear();
  later_.clear();
}

void ComponentMotion::sample(Core &core) {
  earlier_.swap(later_);
  later_.clear();
  for (const std::uint32_t component : drawn_) {
    later_.push_back({.address = component, .object = incarnations_.object(component)});
  }
  std::ranges::sort(later_, {}, &Record::address);
  const auto [repeated, tail] = std::ranges::unique(later_, {}, &Record::address);
  later_.erase(repeated, tail);
  for (Record &record : later_) {
    const std::uint32_t component = record.address;
    record.model = core.mem_r32(component + kModel);
    for (std::size_t word = 0; word < kWordOffsets.size(); ++word) {
      record.words[word] = core.mem_r32(component + kWordOffsets[word]);
    }
  }
}

void ComponentMotion::writeRecord(psx::present::GuestRedraw &redraw,
                                  const Record &from,
                                  const Record &to,
                                  float t) const {
  const auto put = [&](std::size_t word, std::uint32_t value) {
    redraw.writeWord(to.address + kWordOffsets[word], value);
  };
  for (const std::size_t word : {kPosition, kPosition + 1, kPosition + 2, kScale, kScale + 1, kScale + 2}) {
    put(word, psx::present::blendedWord(Motion::Linear, from.words[word], to.words[word], t));
  }
  if (!writeKeyframeStep(redraw, to.address, from.words, to.words, t)) {
    put(kPose, blendPose({.from = from.words[kPose], .to = to.words[kPose]}, t));
  }
  if (rotationOrder(to.words[kFlags]) == kContextRotation) {
    return;
  }
  if (from.words[kAngles] != to.words[kAngles] || from.words[kAngles + 1] != to.words[kAngles + 1]) {
    // The draw turns the component by its angles: blend them and have it rebuild the rotation from them.
    put(kAngles, psx::present::blendedWord(Motion::Angle12x2, from.words[kAngles], to.words[kAngles], t));
    put(kAngles + 1, psx::present::blendedWord(Motion::Angle12Low, from.words[kAngles + 1], to.words[kAngles + 1], t));
    redraw.writeHalf(to.address + kRebuild, 1u);
    return;
  }
  // A sample taken while a rebuild was pending holds the rotation of the angles before.
  if (rebuild(from.words[kFlags]) != 0u || rebuild(to.words[kFlags]) != 0u) {
    return;
  }
  std::array<std::uint32_t, psx::present::TransformRule::kWords> matrix{};
  const psx::present::MotionRule::Samples samples{
      .address = to.address,
      .from = std::span(from.words).subspan(kMatrix, matrix.size()),
      .to = std::span(to.words).subspan(kMatrix, matrix.size()),
  };
  psx::present::TransformRule().blend(samples, t, matrix);
  for (std::size_t word = 0; word < matrix.size(); ++word) {
    put(kMatrix + word, matrix[word]);
  }
}

psx::present::MotionCount ComponentMotion::write(psx::present::GuestRedraw &redraw, float t) const {
  psx::present::MotionCount count{.drawn = later_.size()};
  for (const Record &to : later_) {
    const auto from = std::ranges::lower_bound(earlier_, to.address, {}, &Record::address);
    if (from == earlier_.end() || from->address != to.address || from->object != to.object || from->model != to.model) {
      ++count.unpaired;
      continue;
    }
    writeRecord(redraw, *from, to, t);
  }

  return count;
}

void registerDrawnComponentNotes(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kModelDraw, "CrashBash::ModelDraw", modelDraw);
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kRootTransform, "CrashBash::RootTransform", rootTransform);
}

} // namespace crashbash::render
