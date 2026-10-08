#include "polar_push_contact.h"

#include "core.h"
#include "crashbash_frame_driver.h"
#include "execution_services.h"
#include "game.h"
#include "guest_execution.h"
#include "measured_guest_call.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace crashbash::polar {
namespace {

constexpr std::uint32_t kContactUpdate = 0x800C0888u;
constexpr std::uint32_t kArenaMode = 0x8005A648u;
constexpr std::uint32_t kPolarPushArenaMode = 0x13u;
constexpr std::uint32_t kContactTable = 0x800D6010u;
constexpr std::uint32_t kPlayerSlots = 0x8009D52Cu;
constexpr std::uint32_t kPlayerSlotSize = 0x6Cu;
constexpr std::uint32_t kConfigurationTablePointer = 0x8009D6F4u;
constexpr std::uint32_t kSineTable = 0x80068BD4u;

constexpr std::uint32_t kAbsolute = 0x80032F3Cu;
constexpr std::uint32_t kAngle = 0x8001463Cu;
constexpr std::uint32_t kSquareRoot = 0x80032490u;
constexpr std::uint32_t kRandomTo = 0x80015590u;
constexpr std::uint32_t kSpawnEffect = 0x80022A3Cu;
constexpr std::uint32_t kEmitEvent = 0x80022660u;

constexpr std::uint32_t kContactActive = 0x8000u;
constexpr std::uint32_t kContactRadius = 330u;
constexpr std::uint32_t kContactRadiusExclusive = kContactRadius + 1u;
constexpr std::uint32_t kContactRadiusSquaredExclusive = 108900u;
constexpr std::uint32_t kEffectKind = 0x1604u;
constexpr std::uint32_t kEffectVariant = 0x11u;

std::int32_t signedWord(std::uint32_t value);

struct ContactView {
  Core &core;
  std::uint32_t address;

  std::uint32_t flags() const {
    return core.mem_r32(address);
  }
  void clearActive() const {
    core.mem_w32(address, flags() & ~kContactActive);
  }
  std::int32_t x() const {
    return signedWord(core.mem_r32(address + 4u));
  }
  std::int32_t y() const {
    return signedWord(core.mem_r32(address + 8u));
  }
  std::int32_t z() const {
    return signedWord(core.mem_r32(address + 12u));
  }
};

struct PlayerSlotView {
  Core &core;
  std::uint32_t address;

  std::uint32_t entity() const {
    return core.mem_r32(address + 4u);
  }
  std::uint32_t motion() const {
    return core.mem_r32(address + 0x18u);
  }
  std::uint16_t configurationIndex() const {
    return core.mem_r16(address + 0x2Eu);
  }
};

struct MotionView {
  Core &core;
  std::uint32_t address;

  std::int32_t x() const {
    return signedWord(core.mem_r32(address + 0x20u));
  }
  std::int32_t z() const {
    return signedWord(core.mem_r32(address + 0x40u));
  }
  std::int16_t limit() const {
    return static_cast<std::int16_t>(core.mem_r16(address + 0x4Eu));
  }
  void setX(std::int32_t value) const {
    core.mem_w32(address + 0x20u, static_cast<std::uint32_t>(value));
  }
  void setZ(std::int32_t value) const {
    core.mem_w32(address + 0x40u, static_cast<std::uint32_t>(value));
  }
};

struct EffectResultView {
  Core &core;
  std::uint32_t address;

  std::uint32_t record() const {
    return core.mem_r32(address + 0x58u);
  }
  void markChoreography(std::uint32_t angle) const {
    if (static_cast<std::uint32_t>(angle - 0x400u) > 0x800u) {
      const std::uint32_t owner = core.mem_r32(address + 0x54u);
      if (owner != 0u) {
        const std::uint32_t choreography = core.mem_r32(owner + 0x6Cu);
        if (choreography != 0u) {
          core.mem_w16(choreography + 0x68u, 0x200u);
        }
      }
    }
  }
};

std::int32_t signedWord(std::uint32_t value) {
  return std::bit_cast<std::int32_t>(value);
}

std::int32_t lowProductShift(std::int32_t left, std::int32_t right, std::uint32_t shift) {
  const std::uint32_t low = static_cast<std::uint32_t>(static_cast<std::int64_t>(left) * right);
  return signedWord(low) >> shift;
}

std::uint32_t lowProduct(std::int32_t left, std::int32_t right) {
  return static_cast<std::uint32_t>(static_cast<std::int64_t>(left) * right);
}

void ticks(Core &core, std::uint32_t count) {
  psx::cpu::accountGuestInstructions(core, count);
}

std::uint32_t deadVsyncSample(Core &core, std::uint32_t stackOffset) {
  // Local root-counter sample kept only to retain the retail stack byte pattern; the retail result is never read.
  ticks(core, 2u);
  const std::uint32_t sample = core.game->timing.hSyncCounter();
  core.mem_w32(core.r[29] + stackOffset, sample);
  return sample;
}

std::int32_t signedSine(Core &core, std::uint32_t angle) {
  return static_cast<std::int16_t>(core.mem_r16(kSineTable + 2u + ((angle & 0xFFFu) * 4u)));
}

std::uint32_t guestAbsolute(Core &core, std::int32_t value, std::uint32_t returnAddress, std::uint32_t callTicks) {
  return measuredGuestCall(core, kAbsolute, returnAddress, callTicks, static_cast<std::uint32_t>(value));
}

std::uint32_t guestAngle(Core &core, std::int32_t x, std::uint32_t returnAddress, std::uint32_t callTicks) {
  // The second retail call leaves a1 untouched, so only a0 is passed.
  return measuredGuestCall(core, kAngle, returnAddress, callTicks, static_cast<std::uint32_t>(x));
}

std::uint32_t
guestAngle(Core &core, std::int32_t x, std::int32_t z, std::uint32_t returnAddress, std::uint32_t callTicks) {
  return measuredGuestCall(
      core, kAngle, returnAddress, callTicks, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
}

std::uint32_t guestSquareRoot(Core &core, std::uint32_t value, std::uint32_t returnAddress, std::uint32_t callTicks) {
  return measuredGuestCall(core, kSquareRoot, returnAddress, callTicks, value);
}

std::uint32_t spawnEffect(Core &core, const ContactView &contact, std::uint32_t returnAddress) {
  core.mem_w32(core.r[29] + 16u, kEffectVariant);
  return measuredGuestCall(core,
                           kSpawnEffect,
                           returnAddress,
                           7u,
                           static_cast<std::uint32_t>(contact.x()),
                           static_cast<std::uint32_t>(contact.y() - 0x80),
                           static_cast<std::uint32_t>(contact.z()),
                           kEffectKind);
}

void emitEvent(Core &core, std::uint32_t event, std::uint32_t returnAddress) {
  core.mem_w32(core.r[29] + 16u, 0x1E00u);
  core.mem_w32(core.r[29] + 20u, 0u);
  static_cast<void>(measuredGuestCall(core, kEmitEvent, returnAddress, 8u, event, 0u, 0u, 0x1000u));
}

void configureEffectRecord(Core &core, std::uint32_t record, std::uint32_t angle, const EffectRecipe &recipe) {
  core.mem_w8(record + 0x0Du, 30u);
  core.mem_w16(record + 0x0Eu,
               static_cast<std::uint16_t>(lowProductShift(
                   signedSine(core, angle + recipe.directionOffset), recipe.sineMultiplier, recipe.sineShift)));
  core.mem_w16(record + 0x16u, static_cast<std::uint16_t>(recipe.forwardOffset));
  core.mem_w16(record + 0x18u, static_cast<std::uint16_t>(recipe.verticalOffset));
  core.mem_w16(record + 0x1Au, 3u);
  core.mem_w16(record + 0x20u, static_cast<std::uint16_t>(recipe.sideOffset));
  core.mem_w16(record + 0x22u,
               static_cast<std::uint16_t>(lowProductShift(
                   signedSine(core, angle + recipe.lateralOffset), recipe.sineMultiplier, recipe.sineShift)));
  core.mem_w16(record + 0x2Au, static_cast<std::uint16_t>(recipe.choreographyOffset));
}

void configureFirstEffect(Core &core, std::uint32_t result, std::uint32_t angle) {
  if (result == 0u) {
    return;
  }
  const EffectResultView effect{core, result};
  const std::uint32_t record = effect.record();
  if (record != 0) {
    const std::uint32_t random = measuredGuestCall(core, kRandomTo, 0x800C0B44u, 2u, 3u);
    configureEffectRecord(core, record, angle, effectRecipe(0));
    if (random == 0u) {
      emitEvent(core, 0x46Au, 0x800C0BE8u);
    }
    if (random == 1u) {
      emitEvent(core, 0x46Bu, 0x800C0C14u);
    }
    if (random == 2u) {
      emitEvent(core, 0x46Cu, 0x800C0C40u);
    }
  }
  effect.markChoreography(angle);
}

void configureEffect(Core &core, std::uint32_t result, std::uint32_t angle, std::uint32_t ordinal) {
  if (result == 0u) {
    return;
  }
  const EffectResultView effect{core, result};
  const std::uint32_t record = effect.record();
  if (record != 0) {
    configureEffectRecord(core, record, angle, effectRecipe(ordinal));
  }
  effect.markChoreography(angle);
}

void restoreRegisters(Core &core) {
  core.r[31] = core.mem_r32(core.r[29] + 68u);
  core.r[30] = core.mem_r32(core.r[29] + 64u);
  core.r[23] = core.mem_r32(core.r[29] + 60u);
  core.r[22] = core.mem_r32(core.r[29] + 56u);
  core.r[21] = core.mem_r32(core.r[29] + 52u);
  core.r[20] = core.mem_r32(core.r[29] + 48u);
  core.r[19] = core.mem_r32(core.r[29] + 44u);
  core.r[18] = core.mem_r32(core.r[29] + 40u);
  core.r[17] = core.mem_r32(core.r[29] + 36u);
  core.r[16] = core.mem_r32(core.r[29] + 32u);
  core.r[29] += 72u;
  ticks(core, 12u);
}

void reportCensus(const ContactCensus &totals, const ContactCensus &call) {
  lucent::debug("crashbash-polar",
                "contact update #{}: scanned={} active={} disk-pass={} retained={} consumed={} effects={} "
                "(total scanned={} active={} disk-pass={} retained={} consumed={} effects={})",
                totals.invocations,
                call.scanned,
                call.active,
                call.diskPass,
                call.retained,
                call.consumed,
                call.emittedEffects,
                totals.scanned,
                totals.active,
                totals.diskPass,
                totals.retained,
                totals.consumed,
                totals.emittedEffects);
}

void polarPushArenaContactUpdate(Core *core) {
  ContactCensus &gCensus = frameDriver(*core).polarContactCensus();
  ContactCensus call{};
  ++gCensus.invocations;
  core->r[29] -= 72u;
  core->mem_w32(core->r[29] + 60u, core->r[23]);
  const std::uint32_t position = core->r[4];
  core->r[23] = position;
  core->mem_w32(core->r[29] + 56u, core->r[22]);
  const std::uint32_t playerIndex = core->r[5];
  core->r[22] = playerIndex;
  core->mem_w32(core->r[29] + 48u, core->r[20]);
  core->r[20] = 0;
  core->mem_w32(core->r[29] + 44u, core->r[19]);
  std::uint32_t contactAddress = core->mem_r32(kContactTable);
  core->r[19] = contactAddress;
  core->mem_w32(core->r[29] + 68u, core->r[31]);
  core->mem_w32(core->r[29] + 64u, core->r[30]);
  core->mem_w32(core->r[29] + 52u, core->r[21]);
  core->mem_w32(core->r[29] + 40u, core->r[18]);
  core->mem_w32(core->r[29] + 36u, core->r[17]);
  core->mem_w32(core->r[29] + 32u, core->r[16]);
  ticks(*core, 20u);

  if (core->mem_r16(kArenaMode) != kPolarPushArenaMode) {
    core->r[2] = 0;
    ticks(*core, 1u);
    restoreRegisters(*core);
    reportCensus(gCensus, call);
    return;
  }

  static_cast<void>(deadVsyncSample(*core, 24u));
  core->r[21] = kSineTable;
  core->r[30] = 256u;
  ticks(*core, 4u);

  while (contactAddress != 0) {
    ++call.scanned;
    ++gCensus.scanned;
    const ContactView contact{*core, contactAddress};
    ticks(*core, 2u);
    if ((contact.flags() & kContactActive) == 0u) {
      ticks(*core, 5u);
      contactAddress = core->mem_r32(kContactTable + (++core->r[20] * 4u));
      core->r[19] = contactAddress;
      ticks(*core, 6u);
      continue;
    }

    ++call.active;
    ++gCensus.active;
    const std::int32_t deltaX = contact.x() - signedWord(core->mem_r32(position));
    const std::int32_t deltaZ = contact.z() - signedWord(core->mem_r32(position + 8u));
    const std::uint32_t absX = guestAbsolute(*core, deltaX, 0x800C0930u, 9u);
    if (signedWord(absX) >= static_cast<std::int32_t>(kContactRadiusExclusive)) {
      ticks(*core, 3u);
      contactAddress = core->mem_r32(kContactTable + (++core->r[20] * 4u));
      core->r[19] = contactAddress;
      ticks(*core, 6u);
      continue;
    }
    const std::uint32_t absZ = guestAbsolute(*core, deltaZ, 0x800C0944u, 2u);
    const std::uint32_t distanceSquared = lowProduct(deltaX, deltaX) + lowProduct(deltaZ, deltaZ);
    ticks(*core, 3u);
    if (!isWithinContactDisk(deltaX, deltaZ, distanceSquared)) {
      ticks(*core, 11u);
      contactAddress = core->mem_r32(kContactTable + (++core->r[20] * 4u));
      core->r[19] = contactAddress;
      ticks(*core, 6u);
      continue;
    }
    static_cast<void>(absZ); // The helper result is deliberately consumed by the retail branch above.

    ++call.diskPass;
    ++gCensus.diskPass;
    const std::uint32_t collisionAngle = guestAngle(*core, deltaX, deltaZ, 0x800C0988u, 3u) & 0xFFFu;
    const PlayerSlotView player{*core, kPlayerSlots + playerIndex * kPlayerSlotSize};
    const std::uint32_t entity = player.entity();
    const std::int32_t impulse = static_cast<std::int32_t>(kContactRadius) -
                                 (signedWord(guestSquareRoot(*core, distanceSquared, 0x800C09B8u, 12u)) >> 6);
    core->mem_w32(
        entity + 0x10u,
        static_cast<std::uint32_t>(signedWord(core->mem_r32(entity + 0x10u)) -
                                   lowProductShift(impulse, signedSine(*core, collisionAngle - 0x400u), 12u)));
    core->mem_w32(entity + 0x18u,
                  static_cast<std::uint32_t>(signedWord(core->mem_r32(entity + 0x18u)) -
                                             lowProductShift(impulse, signedSine(*core, collisionAngle), 12u)));
    const std::uint32_t motionAngle =
        guestAngle(*core, signedWord(core->mem_r32(entity + 0x10u)), 0x800C0A24u, 27u) & 0xFFFu;
    const std::uint32_t motionAddress = player.motion();
    ticks(*core, 4u);
    if (motionAddress == 0) {
      ++call.retained;
      ++gCensus.retained;
      static_cast<void>(deadVsyncSample(*core, 28u));
      core->r[2] = 1u;
      ticks(*core, 3u);
      restoreRegisters(*core);
      reportCensus(gCensus, call);
      return;
    }

    const MotionView motion{*core, motionAddress};
    const std::uint32_t motionLengthSquared = lowProduct(motion.x(), motion.x()) + lowProduct(motion.z(), motion.z());
    const std::int32_t motionSpeed = signedWord(guestSquareRoot(*core, motionLengthSquared, 0x800C0A5Cu, 10u)) >> 6;
    motion.setX(-lowProductShift(motionSpeed, signedSine(*core, 0x400u - motionAngle), 13u));
    motion.setZ(-lowProductShift(motionSpeed, signedSine(*core, -motionAngle), 13u));
    const std::uint32_t configurationTable = core->mem_r32(kConfigurationTablePointer);
    const std::uint16_t configurationValue = core->mem_r16(configurationTable + player.configurationIndex() * 6u);
    ticks(*core, 42u);
    if (!crossesContactThreshold(configurationValue, motionSpeed, motion.limit())) {
      ++call.retained;
      ++gCensus.retained;
      static_cast<void>(deadVsyncSample(*core, 28u));
      core->r[2] = 1u;
      ticks(*core, 3u);
      restoreRegisters(*core);
      reportCensus(gCensus, call);
      return;
    }

    const std::uint32_t first = spawnEffect(*core, contact, 0x800C0B20u);
    configureFirstEffect(*core, first, motionAngle);
    const std::uint32_t second = spawnEffect(*core, contact, 0x800C0C80u);
    configureEffect(*core, second, motionAngle, 1u);
    const std::uint32_t third = spawnEffect(*core, contact, 0x800C0D58u);
    configureEffect(*core, third, motionAngle, 2u);
    contact.clearActive();
    ticks(*core, 4u);
    ++call.consumed;
    ++gCensus.consumed;
    call.emittedEffects += (first != 0) + (second != 0) + (third != 0);
    gCensus.emittedEffects += (first != 0) + (second != 0) + (third != 0);
    static_cast<void>(deadVsyncSample(*core, 28u));
    core->r[2] = 1u;
    ticks(*core, 3u);
    restoreRegisters(*core);
    reportCensus(gCensus, call);
    return;
  }

  static_cast<void>(deadVsyncSample(*core, 28u));
  core->r[2] = 0;
  ticks(*core, 1u);
  restoreRegisters(*core);
  reportCensus(gCensus, call);
}

} // namespace

bool isWithinContactDisk(std::int32_t deltaX, std::int32_t deltaZ, std::uint32_t distanceSquared) {
  const auto absolute = [](std::int32_t value) {
    return value == INT32_MIN ? INT32_MIN : std::abs(value);
  };
  return absolute(deltaX) < static_cast<std::int32_t>(kContactRadiusExclusive) &&
         absolute(deltaZ) < static_cast<std::int32_t>(kContactRadiusExclusive) &&
         signedWord(distanceSquared) < static_cast<std::int32_t>(kContactRadiusSquaredExclusive);
}

std::int32_t contactThreshold(std::uint16_t configurationValue) {
  return signedWord(static_cast<std::uint32_t>(configurationValue * 6u) << 14u) >> 8u;
}

bool crossesContactThreshold(std::uint16_t configurationValue, std::int32_t motionSpeed, std::int16_t motionLimit) {
  return contactThreshold(configurationValue) < signedWord(lowProduct(motionSpeed, motionLimit));
}

const EffectRecipe &effectRecipe(std::uint32_t ordinal) {
  static constexpr std::array<EffectRecipe, 3> kRecipes{{
      {.directionOffset = -0x200,
       .lateralOffset = 0x200,
       .forwardOffset = -0x100,
       .verticalOffset = -0x20,
       .sideOffset = -0x80,
       .choreographyOffset = 0x100,
       .sineMultiplier = 17,
       .sineShift = 11},
      {.directionOffset = 0,
       .lateralOffset = 0x400,
       .forwardOffset = 0x100,
       .verticalOffset = -0x2A,
       .sideOffset = 0x80,
       .choreographyOffset = -0x100,
       .sineMultiplier = 11,
       .sineShift = 10},
      {.directionOffset = -0x400,
       .lateralOffset = 0,
       .forwardOffset = 0x100,
       .verticalOffset = -0x48,
       .sideOffset = -0x80,
       .choreographyOffset = 0x100,
       .sineMultiplier = 11,
       .sineShift = 10},
  }};
  if (ordinal >= kRecipes.size()) {
    lucent::error("crashbash-polar", "invalid Polar Push effect recipe {}", ordinal);
    std::abort();
  }
  return kRecipes[ordinal];
}

void registerPolarPushContactOverride(Core &core) {
  runtime::registerNativeOverride(core,
                                  runtime::GuestImage::Dat22510,
                                  kContactUpdate,
                                  "CrashBash::PolarPushArenaContactUpdate",
                                  polarPushArenaContactUpdate);
}

} // namespace crashbash::polar
