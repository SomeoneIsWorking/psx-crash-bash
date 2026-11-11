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
constexpr std::size_t kPanelBottom = 20;
constexpr std::size_t kQuadCorners = 21;
static_assert(ComponentMotion::kWordOffsets[kAngles] == 0x10u && ComponentMotion::kWordOffsets[kMatrix] == 0x30u);
static_assert(ComponentMotion::kWordOffsets[kFlags] == 0x50u && ComponentMotion::kWordOffsets[kPose] == 0x70u);
static_assert(ComponentMotion::kWordOffsets[kDrawFlags] == 0x00u && ComponentMotion::kWordOffsets[kFrameCode] == 0x74u);
static_assert(ComponentMotion::kWordOffsets[kPanelBottom] == 0x78u &&
              ComponentMotion::kWordOffsets[kQuadCorners] == 0x7Cu);

// The panel and quad callbacks draw in screen space from the +0x6C record while this flag is set, and under the
// component's transform otherwise.
constexpr std::uint32_t kScreenSpace = 0x10000000u;
// A screen-space x whose top bits are 10 is centred (FUN_8001A6D4, FUN_8001A82C); its mode steps.
constexpr std::uint32_t kScreenXMode = 0xC000u;

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

// Both samples draw keyframes of one animation: a pose blends only within it.
bool sameAnimation(std::uint32_t codeFrom, std::uint32_t codeTo) {
  constexpr std::uint32_t kAnimationCode = kFrameKind | kAnimation;
  return (codeFrom & kFrameKind) == kKeyframeKind && (codeFrom & kAnimationCode) == (codeTo & kAnimationCode);
}

// A pose as one phase: keyframe (s16, high) and the u16 progress toward the next (low), as 0x80019A60's weighted
// draw reads +0x70.
constexpr double kKeyframePhase = 65536.0;

double posePhase(std::uint32_t pose) {
  return static_cast<std::int16_t>(pose >> 16) * kKeyframePhase + (pose & 0xFFFFu);
}

std::uint32_t poseAt(double phase) {
  const double frame = std::floor(phase / kKeyframePhase);
  const auto progress = static_cast<std::uint32_t>(phase - frame * kKeyframePhase);
  return static_cast<std::uint32_t>(static_cast<std::int32_t>(frame)) << 16 | (progress & 0xFFFFu);
}

// FUN_8001ACC0(component) turns a root component into the matrix 0x80056A44 its children draw by.
void rootTransform(Core *core) {
  session(*core).drawReplay.noteDrawn(core->r[4], ComponentMotion::Kind::Model);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kRootTransform);
}

void modelDraw(Core *core) {
  session(*core).drawReplay.noteDrawn(core->r[7], ComponentMotion::Kind::Model);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kModelDraw);
}

void noteScreenCallback(Core &core, ComponentMotion::Kind screen) {
  const std::uint32_t component = core.r[4];
  const bool screenSpace = (core.mem_r32(component) & kScreenSpace) != 0u;
  session(core).drawReplay.noteDrawn(component, screenSpace ? screen : ComponentMotion::Kind::Placed);
}

void panelDraw(Core *core) {
  noteScreenCallback(*core, ComponentMotion::Kind::Panel);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kPanelComponentDraw);
}

void textDraw(Core *core) {
  session(*core).drawReplay.noteDrawn(core->r[4], ComponentMotion::Kind::Text);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kTextComponentDraw);
}

void quadDraw(Core *core) {
  noteScreenCallback(*core, ComponentMotion::Kind::Quad);
  runtime::callOriginal(*core, runtime::GuestImage::Resident, guest::kQuadComponentDraw);
}

// The pose `t` of the way between the samples, when both draw one animation and the later is ahead: a weighted pose's
// phase moves straight; a keyframe step is drawn the way the guest draws a weighted pose, from the keyframe the
// phase passed toward the next. A loop wrap or a switch of animation is drawn as now.
// A component's two samples, in kWordOffsets order.
struct WordSamples {
  std::span<const std::uint32_t> from;
  std::span<const std::uint32_t> to;
};

void writePose(psx::present::GuestRedraw &redraw, std::uint32_t component, WordSamples samples, float t) {
  const std::span<const std::uint32_t> from = samples.from;
  const std::span<const std::uint32_t> to = samples.to;
  const std::uint32_t codeFrom = from[kFrameCode] & 0xFFFFu;
  const std::uint32_t codeTo = to[kFrameCode] & 0xFFFFu;
  if (!sameAnimation(codeFrom, codeTo)) {
    return;
  }
  const bool weightedFrom = (from[kDrawFlags] & kWeighted) != 0u;
  const bool weightedTo = (to[kDrawFlags] & kWeighted) != 0u;
  if (weightedFrom && weightedTo) {
    const double phaseFrom = posePhase(from[kPose]);
    const double phaseTo = posePhase(to[kPose]);
    if (phaseTo >= phaseFrom) {
      redraw.writeWord(component + ComponentMotion::kWordOffsets[kPose],
                       poseAt(psx::present::lerpExact(phaseFrom, phaseTo, t)));
    }
    return;
  }
  const std::uint32_t keyFrom = codeFrom & kKeyframe;
  const std::uint32_t keyTo = codeTo & kKeyframe;
  if (weightedFrom || weightedTo || keyTo <= keyFrom) {
    return;
  }
  const double phase = psx::present::lerpExact(keyFrom * kKeyframePhase, keyTo * kKeyframePhase, t);
  const std::uint32_t pose = poseAt(phase);
  const std::uint32_t key = pose >> 16;
  const std::uint32_t progress = std::clamp(pose & 0xFFFFu, 1u, 0xFFFFu);
  redraw.writeHalf(component + ComponentMotion::kWordOffsets[kFrameCode],
                   static_cast<std::uint16_t>((codeFrom & ~kKeyframe) | key));
  redraw.writeWord(component + ComponentMotion::kWordOffsets[kPose], key << 16 | progress);
  redraw.writeWord(component, to[kDrawFlags] | kWeighted);
}

} // namespace

void ComponentMotion::noteDrawn(std::uint32_t component, Kind kind) {
  // BOOT 0x8009440C refills its scratch component from a table inside the draw: the table is what moves.
  if (component != guest::kBootScratchComponent) {
    drawn_.emplace_back(component, kind);
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
  for (const auto &[component, kind] : drawn_) {
    later_.push_back({.address = component, .generation = incarnations_.generation(component), .kind = kind});
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
  switch (to.kind) {
  case Kind::Model:
    writePose(redraw, to.address, {.from = from.words, .to = to.words}, t);
    writeTransform(redraw, from, to, t);
    return;
  case Kind::Placed:
    writeTransform(redraw, from, to, t);
    return;
  case Kind::Text:
    // FUN_8001A82C places screen-space text at position x|y, the x's centring mode in its top bits.
    if ((to.words[kDrawFlags] & kScreenSpace) != 0u &&
        (from.words[kPosition] & kScreenXMode) != (to.words[kPosition] & kScreenXMode)) {
      return;
    }
    writeTransform(redraw, from, to, t);
    return;
  case Kind::Panel:
  case Kind::Quad:
    writeScreenRecord(redraw, from, to, t);
    return;
  }
}

void ComponentMotion::writeScreenRecord(psx::present::GuestRedraw &redraw,
                                        const Record &from,
                                        const Record &to,
                                        float t) const {
  const auto halves = [&](std::uint32_t offset, std::uint32_t earlier, std::uint32_t later) {
    redraw.writeWord(to.address + offset, psx::present::blendedWord(Motion::Halves, earlier, later, t));
  };
  if (to.kind == Kind::Quad) {
    // Corners x|y at +0x6C, +0x74, +0x7C, +0x84.
    halves(kModel, from.model, to.model);
    halves(kWordOffsets[kFrameCode], from.words[kFrameCode], to.words[kFrameCode]);
    halves(kWordOffsets[kQuadCorners], from.words[kQuadCorners], to.words[kQuadCorners]);
    halves(kWordOffsets[kQuadCorners + 1], from.words[kQuadCorners + 1], to.words[kQuadCorners + 1]);
    return;
  }
  // The panel rect: x|y at +0x70, depth|x1 at +0x74, y1 at +0x78.
  if ((from.words[kPose] & kScreenXMode) != (to.words[kPose] & kScreenXMode)) {
    return;
  }
  halves(kWordOffsets[kPose], from.words[kPose], to.words[kPose]);
  halves(kWordOffsets[kFrameCode], from.words[kFrameCode], to.words[kFrameCode]);
  redraw.writeWord(to.address + kWordOffsets[kPanelBottom],
                   psx::present::blendedWord(Motion::Low16, from.words[kPanelBottom], to.words[kPanelBottom], t));
}

void ComponentMotion::writeTransform(psx::present::GuestRedraw &redraw,
                                     const Record &from,
                                     const Record &to,
                                     float t) const {
  const auto put = [&](std::size_t word, std::uint32_t value) {
    redraw.writeWord(to.address + kWordOffsets[word], value);
  };
  for (const std::size_t word : {kPosition, kPosition + 1, kPosition + 2, kScale, kScale + 1, kScale + 2}) {
    put(word, psx::present::blendedWord(Motion::Linear, from.words[word], to.words[word], t));
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
    const bool screen = to.kind == Kind::Panel || to.kind == Kind::Quad;
    if (from == earlier_.end() || from->address != to.address || from->generation != to.generation ||
        from->kind != to.kind || (!screen && from->model != to.model)) {
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
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kTextComponentDraw, "CrashBash::TextComponentDraw", textDraw);
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kPanelComponentDraw, "CrashBash::PanelComponentDraw", panelDraw);
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Resident, guest::kQuadComponentDraw, "CrashBash::QuadComponentDraw", quadDraw);
}

} // namespace crashbash::render
