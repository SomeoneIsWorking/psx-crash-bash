#include "boot_object_callbacks.h"

#include "core.h"
#include "crashbash_guest.h"
#include "execution_services.h"
#include "game.h"
#include "guest_execution.h"
#include "measured_guest_call.h"

#include <cstdint>
#include <lucent/log.h>

namespace crashbash {
namespace {

// kSceneMode gates the update; kAnimationRetired is set by the outer callback when the sequence ends.
constexpr std::uint32_t kSceneMode = 0x8005A648u;
constexpr std::uint32_t kSceneModeAnimated = 0x2Cu;
constexpr std::uint32_t kAnimationRetired = 0x8009E194u;
// Frame heap shared with display_frame.cpp (retail r9-7952).
constexpr std::uint32_t kFrameHeap = 0x8004E0F0u;

constexpr std::uint32_t kResolveTrack = 0x80021DD4u;
constexpr std::uint32_t kBindChannel = 0x8001F604u;
constexpr std::uint32_t kRandomTo = 0x80015590u;
constexpr std::uint32_t kAdvanceChannel = 0x8002147Cu;
constexpr std::uint32_t kSlerp = 0x80020344u;
constexpr std::uint32_t kHeapFree = 0x800110A8u;
constexpr std::uint32_t kRetireObject = 0x800270B0u;
constexpr std::uint32_t kAnimateChannels = 0x8008ADA4u;
constexpr std::uint32_t kObjectUpdate = 0x8008BB48u;

// MIPS div plus the retail break checks for a zero divisor and INT_MIN / -1.
std::int32_t guestDiv(Core &core, std::int32_t numerator, std::int32_t divisor) {
  cpu_div(&core, static_cast<std::uint32_t>(numerator), static_cast<std::uint32_t>(divisor));
  if (divisor == 0) {
    psx::cpu::handleBreak(core, 7168u);
  } else if (divisor == -1 && numerator == INT32_MIN) {
    psx::cpu::handleBreak(core, 6144u);
  }
  return static_cast<std::int32_t>(core.lo);
}

std::int32_t s32(std::uint32_t value) {
  return static_cast<std::int32_t>(value);
}

// Retail VSync(1) sample: the HBlank counter delta; the elided call's instruction cost is still charged.
std::uint32_t hsyncSample(Core &core, std::uint32_t instructionTicks) {
  psx::cpu::accountGuestInstructions(core, instructionTicks);
  return core.game->timing.hSyncCounter();
}

// 0x8008ADA4: per-channel animation advance. `state`: 1 requests a reset, answered with 2 at the end.
// Both retail VSync samples are dead: 0x8008B260/0x8008B264 are overwritten by the load at 0x8008B268.
std::uint32_t
animateChannels(Core &core, std::uint32_t object, std::uint32_t node, std::uint32_t channel, std::uint32_t state) {
  std::uint32_t keyframe = core.mem_r32(channel + 0x30u);
  const std::uint32_t deltas = core.mem_r32(channel + 0x34u);
  static_cast<void>(hsyncSample(core, 21u)); // dead retail sample

  std::uint32_t track = 0;
  const std::uint32_t resolved = measuredGuestCall(core,
                                                   kResolveTrack,
                                                   0x8008AE18u,
                                                   8u,
                                                   core.mem_r32(object + 0x3B8u),
                                                   core.mem_r32(channel + 0x24u),
                                                   core.mem_r32(channel + 4u));
  if (resolved != 0) {
    track = resolved + 0x14u;
  }

  if (track != 0) {
    if (s32(core.mem_r32(channel + 0x2Cu)) <= s32(core.mem_r32(track + 0x10u))) {
      core.mem_w16(deltas + 0x10u, 0u);
      core.mem_w16(deltas + 0x12u, 0u);
      std::int32_t bound = 0;
      std::uint32_t cursor = node;
      while (bound < s32(core.mem_r32(track + 0x14u))) {
        if ((core.mem_r32(cursor) & 0x8000u) == 0u) {
          measuredGuestCall(core, kBindChannel, 0x8008AE80u, 3u, cursor, keyframe, track);

          std::int32_t span = s32(core.mem_r32(track + 0x18u));
          if (span == 0) {
            span = s32(core.mem_r32(channel + 0x20u));
          }

          const std::int32_t fadeIn = s32(core.mem_r32(track + 0x58u));
          if (fadeIn != 0) {
            core.mem_w32(deltas, static_cast<std::uint32_t>(guestDiv(core, 0x1000, fadeIn)));
            core.mem_w16(cursor + 0x76u, 0x1000u);
          }

          const std::int32_t holdEnd = span - s32(core.mem_r32(track + 0x5Cu));
          if (s32(core.mem_r32(track + 0x5Cu)) != 0) {
            core.mem_w32(deltas + 4u, static_cast<std::uint32_t>(guestDiv(core, 0x1000, holdEnd)));
          }

          const std::int32_t rampUp = s32(core.mem_r32(track + 0x60u));
          if (rampUp != 0) {
            core.mem_w32(deltas + 8u, static_cast<std::uint32_t>(guestDiv(core, 0x1000, rampUp)));
            core.mem_w32(cursor + 0x28u, 0u);
            core.mem_w32(cursor + 0x24u, 0u);
            core.mem_w32(cursor + 0x20u, 0u);
          }

          const std::int32_t rampDown = s32(core.mem_r32(track + 0x64u));
          if (rampDown != 0) {
            if (span == rampDown) {
              core.mem_w16(deltas + 0x0Cu, 0u);
              core.mem_w16(deltas + 0x0Eu, 0u);
            } else {
              core.mem_w32(deltas + 0x0Cu, static_cast<std::uint32_t>(guestDiv(core, 0x1000, span - rampDown)));
            }
          }

          if (core.mem_r32(track + 0x68u) != 0) {
            const std::uint32_t jitter = measuredGuestCall(core, kRandomTo, 0x8008AFD8u, 2u, 0x1000u);
            core.mem_w16(cursor + 0x14u, static_cast<std::uint16_t>(jitter));
          }

          ++bound;
          core.mem_w32(channel + 0x2Cu, core.mem_r32(channel + 0x2Cu) + 1u);
        }
        cursor = core.mem_r32(cursor + 0x5Cu);
        keyframe += 0x28u;
      }
    }

    std::uint32_t key = core.mem_r32(channel + 0x30u);
    std::uint32_t cursor = node;
    if (s32(core.mem_r32(track + 0x10u)) > 0) {
      std::int32_t index = 0;
      do {
        if (cursor != 0 && (core.mem_r32(cursor) & 0x8000u) != 0u) {
          if (core.mem_r32(track + 0x58u) != 0 && s32(core.mem_r32(key)) < s32(core.mem_r32(track + 0x58u))) {
            const std::uint32_t faded = core.mem_r16(cursor + 0x76u) - core.mem_r16(deltas);
            core.mem_w16(cursor + 0x76u, static_cast<std::uint16_t>(faded));
            if (static_cast<std::int16_t>(faded) < 0) {
              core.mem_w16(cursor + 0x76u, 0u);
            }
          }
          if (core.mem_r32(track + 0x5Cu) != 0 && s32(core.mem_r32(track + 0x5Cu)) < s32(core.mem_r32(key))) {
            const std::int16_t held =
                static_cast<std::int16_t>(static_cast<std::int16_t>(core.mem_r16(cursor + 0x76u)) +
                                          static_cast<std::int32_t>(core.mem_r16(deltas + 4u)));
            core.mem_w16(cursor + 0x76u, static_cast<std::uint16_t>(held));
            if (held > 0x1000) {
              core.mem_w16(cursor + 0x76u, 0x1000u);
            }
          }
          if (core.mem_r32(track + 0x60u) != 0 && s32(core.mem_r32(key)) <= s32(core.mem_r32(track + 0x60u))) {
            const std::uint32_t weight = core.mem_r32(cursor + 0x20u) + core.mem_r32(deltas + 8u);
            core.mem_w32(cursor + 0x20u, weight);
            if (s32(weight) < 1) {
              core.mem_w32(cursor + 0x20u, 0u);
            }
            core.mem_w32(cursor + 0x28u, core.mem_r32(cursor + 0x20u));
            core.mem_w32(cursor + 0x24u, core.mem_r32(cursor + 0x20u));
          }
          if (core.mem_r32(track + 0x64u) != 0 && s32(core.mem_r32(track + 0x64u)) <= s32(core.mem_r32(key))) {
            const std::uint32_t weight = core.mem_r32(cursor + 0x20u) - core.mem_r32(deltas + 0x0Cu);
            core.mem_w32(cursor + 0x20u, weight);
            if (s32(weight) < 1) {
              core.mem_w32(cursor + 0x20u, 0u);
            }
            core.mem_w32(cursor + 0x28u, core.mem_r32(cursor + 0x20u));
            core.mem_w32(cursor + 0x24u, core.mem_r32(cursor + 0x20u));
          }

          measuredGuestCall(core, kAdvanceChannel, 0x8008B170u, 2u, cursor, track, key);

          if ((core.mem_r32(cursor) & 0x8000u) == 0u && core.mem_r32(track + 0x6Cu) != 0) {
            const std::int32_t remaining = s32(core.mem_r32(channel + 0x2Cu)) - 1;
            core.mem_w32(channel + 0x2Cu, static_cast<std::uint32_t>(remaining));
            if (remaining < 0) {
              core.mem_w32(channel + 0x2Cu, 0u);
            }
            core.mem_w32(key, 0u);
          }
        }
        ++index;
        cursor = core.mem_r32(cursor + 0x5Cu);
        key += 0x28u;
      } while (index < s32(core.mem_r32(track + 0x10u)));
    }
  }

  if (core.mem_r32(state) == 1u) {
    std::uint32_t cursor = node;
    while (cursor != 0) {
      core.mem_w32(cursor + 0x20u, 0x1000u);
      core.mem_w32(cursor + 0x28u, 0x1000u);
      core.mem_w32(cursor + 0x24u, 0x1000u);
      core.mem_w16(cursor + 0x76u, 0u);
      core.mem_w32(cursor, core.mem_r32(cursor) & 0xFFFF7FFFu);
      cursor = core.mem_r32(cursor + 0x5Cu);
      core.mem_w32(channel + 0x2Cu, core.mem_r32(track + 0x14u));
    }
    core.mem_w32(state, 0u);
  }

  // Retail reads track+0x1C even when track is unresolved (0), so it is not guarded.
  if ((s32(core.mem_r32(track + 0x1Cu)) << 8) <= s32(core.mem_r32(object + 0x14u))) {
    core.mem_w32(state, 2u);
  }

  static_cast<void>(hsyncSample(core, 2u)); // dead retail sample
  return core.mem_r32(deltas + 0x24u);
}

// 0x8008BB48: object update callback, slot [0x13] of each object (scene walk 0x8007976C); mode 1 defers to 0x8008ADA4.
// Its VSync samples cannot affect state: the entry return value is unused, the exit one at 0x8008C330 is lost.

// Both heap buffers of every channel go back to the frame arena.
void releaseChannelBuffers(Core &core, std::uint32_t sequence, std::uint32_t firstReturn, std::uint32_t secondReturn) {
  std::uint32_t offset = 0x2Cu;
  for (std::int32_t index = 0; index < s32(core.mem_r32(sequence + 0x3ACu)); ++index) {
    const std::uint32_t keys = core.mem_r32(sequence + offset + 0x30u);
    if (keys != 0) {
      measuredGuestCall(core, kHeapFree, firstReturn, 2u, keys, kFrameHeap);
    }
    const std::uint32_t deltas = core.mem_r32(sequence + offset + 0x34u);
    if (deltas != 0) {
      measuredGuestCall(core, kHeapFree, secondReturn, 2u, deltas, kFrameHeap);
    }
    offset += 0x38u;
  }
}

std::uint32_t objectUpdate(Core &core, std::uint32_t object) {
  const std::uint32_t sample = hsyncSample(core, 16u);

  if (core.mem_r32(kSceneMode) == kSceneModeAnimated) {
    const std::uint32_t retired = core.mem_r32(kAnimationRetired);
    if (retired != 0) {
      return retired;
    }
  }

  // Retail 0x50-byte frame; the callback below takes sp+0x18 as an in/out word.
  core.r[29] -= 0x50u;
  const std::uint32_t stateSlot = core.r[29] + 0x18u;
  core.mem_w32(stateSlot, 0u);

  const std::uint32_t sequence = core.mem_r32(object + 0x58u);
  if (sequence != 0) {
    std::uint32_t node = core.mem_r32(core.mem_r32(object + 0x54u) + 0x6Cu);
    core.mem_w32(sequence + 0x14u, core.mem_r32(sequence + 0x14u) + core.mem_r32(sequence + 0x18u));

    for (std::int32_t channelIndex = 0; channelIndex < s32(core.mem_r32(sequence + 0x3ACu)); ++channelIndex) {
      const std::uint32_t channel = sequence + static_cast<std::uint32_t>(channelIndex) * 0x38u + 0x2Cu;
      const std::uint32_t mode = core.mem_r32(channel);

      if (mode == 0) {
        const std::uint32_t resolved = measuredGuestCall(core,
                                                         kResolveTrack,
                                                         0x8008BCE0u,
                                                         5u,
                                                         core.mem_r32(sequence + 0x3B8u),
                                                         core.mem_r32(channel + 0x24u),
                                                         core.mem_r32(channel + 4u));
        const std::uint32_t track = resolved != 0 ? resolved + 0x24u : 0u;
        if (track != 0) {
          core.mem_w32(channel + 0x10u, track);
          const std::uint32_t key = track + core.mem_r32(channel + 8u) * 0x50u;
          const std::int32_t keyStart = s32(core.mem_r32(key));
          const std::int32_t keyEnd = s32(core.mem_r32(key + 0x50u));
          const std::int32_t span = keyEnd - keyStart;
          const std::int32_t now = s32(core.mem_r32(sequence + 0x14u)) >> 8;

          const std::int32_t lastKey = s32(core.mem_r32(track + (core.mem_r32(channel + 0x0Cu) - 1u) * 0x50u));
          if (lastKey < now || now < s32(core.mem_r32(track))) {
            core.mem_w32(node, core.mem_r32(node) & 0xFFFF7FFFu);
          } else {
            core.mem_w32(node, core.mem_r32(node) | 0x8000u);
          }

          std::int32_t elapsed = 0;
          if (s32(core.mem_r32(sequence + 0x14u)) - keyStart > 0) {
            elapsed = s32(core.mem_r32(sequence + 0x14u)) + keyStart * -0x100;
          }

          if (node != 0 && (core.mem_r32(node) & 0x8000u) != 0u) {
            core.mem_w16(node + 0x52u, 1u);
            if (static_cast<std::int8_t>(core.mem_r8(node + 0x51u)) == 3) {
              // Rotation: slerp the two keyframe quaternions.
              measuredGuestCall(core,
                                kSlerp,
                                0x8008BE10u,
                                7u,
                                key + 0x24u,
                                key + 0x74u,
                                static_cast<std::uint32_t>(guestDiv(core, elapsed << 12, span) >> 8),
                                node + 0x10u);
            } else {
              // Euler: three interpolations at 1/256-tick resolution.
              const std::int32_t fine = span * 0x100;
              const std::int32_t basePitch = s32(core.mem_r32(key + 0x1Cu));
              const std::int32_t pitch = ((basePitch - s32(core.mem_r32(key + 0x6Cu))) * 0x100000 >> 20) * elapsed;
              core.mem_w16(node + 0x52u, 1u);
              core.mem_w16(node + 0x12u,
                           static_cast<std::uint16_t>(static_cast<std::int16_t>(basePitch) -
                                                      static_cast<std::int16_t>(guestDiv(core, pitch, fine))));
              const std::int32_t baseYaw = s32(core.mem_r32(key + 0x18u));
              const std::int32_t yaw = ((baseYaw - s32(core.mem_r32(key + 0x68u))) * 0x100000 >> 20) * elapsed;
              core.mem_w16(node + 0x10u,
                           static_cast<std::uint16_t>(static_cast<std::int16_t>(baseYaw) -
                                                      static_cast<std::int16_t>(guestDiv(core, yaw, fine))));
              const std::int32_t baseRoll = s32(core.mem_r32(key + 0x20u));
              const std::int32_t roll = ((s32(core.mem_r32(key + 0x70u)) - baseRoll) * 0x100000 >> 20) * elapsed;
              core.mem_w16(node + 0x14u,
                           static_cast<std::uint16_t>(static_cast<std::int16_t>(baseRoll) +
                                                      static_cast<std::int16_t>(guestDiv(core, roll, fine))));
            }

            // Translation, scale and blend weight share the 1/256-tick span.
            const std::int32_t fine = span * 0x100;
            const std::int32_t baseX = s32(core.mem_r32(key + 0x0Cu));
            core.mem_w32(node + 4u,
                         static_cast<std::uint32_t>(
                             baseX + guestDiv(core, (s32(core.mem_r32(key + 0x5Cu)) - baseX) * elapsed, fine)));
            const std::int32_t baseZ = s32(core.mem_r32(key + 0x14u));
            core.mem_w32(node + 0x0Cu,
                         static_cast<std::uint32_t>(
                             baseZ + guestDiv(core, (s32(core.mem_r32(key + 0x64u)) - baseZ) * elapsed, fine)));
            const std::int32_t baseY = s32(core.mem_r32(key + 0x10u));
            core.mem_w32(node + 8u,
                         static_cast<std::uint32_t>(
                             baseY + guestDiv(core, (s32(core.mem_r32(key + 0x60u)) - baseY) * elapsed, fine)));

            const std::int32_t scaleX = s32(core.mem_r32(key + 0x40u));
            core.mem_w16(node + 0x52u, 1u);
            core.mem_w32(node + 0x20u,
                         static_cast<std::uint32_t>(
                             scaleX + guestDiv(core, (s32(core.mem_r32(key + 0x90u)) - scaleX) * elapsed, fine)));
            const std::int32_t scaleY = s32(core.mem_r32(key + 0x44u));
            core.mem_w32(node + 0x24u,
                         static_cast<std::uint32_t>(
                             scaleY + guestDiv(core, (s32(core.mem_r32(key + 0x94u)) - scaleY) * elapsed, fine)));
            const std::int32_t scaleZ = s32(core.mem_r32(key + 0x48u));
            const std::int32_t scaleZDelta = (s32(core.mem_r32(key + 0x98u)) - scaleZ) * elapsed;
            const std::uint32_t flags = core.mem_r32(node);
            core.mem_w32(node, flags & 0xBFFFFFFFu);
            core.mem_w32(node + 0x28u, static_cast<std::uint32_t>(scaleZ + guestDiv(core, scaleZDelta, fine)));
            const std::int32_t weight = s32(core.mem_r32(key + 0x4Cu));
            const std::int32_t weightDelta = (s32(core.mem_r32(key + 0x9Cu)) - weight) * elapsed;
            const std::int32_t weightStep = guestDiv(core, weightDelta, fine);
            core.mem_w32(node, (flags & 0xBFFFFFFFu) | 0x40000000u);
            core.mem_w16(node + 0x76u,
                         static_cast<std::uint16_t>(
                             0x1000 - (static_cast<std::int16_t>(weight) + static_cast<std::int16_t>(weightStep))));
          }

          if ((keyEnd << 8) <= s32(core.mem_r32(sequence + 0x14u))) {
            core.mem_w32(channel + 8u, core.mem_r32(channel + 8u) + 1u);
          }

          if ((s32(core.mem_r32(channel + 0x20u)) << 8) <= s32(core.mem_r32(sequence + 0x14u))) {
            if ((core.mem_r32(sequence + 0x20u) & 1u) != 0u) {
              const std::uint32_t loops = core.mem_r32(sequence + 0x24u) + 1u;
              core.mem_w32(sequence + 0x24u, loops);
              if (s32(core.mem_r32(sequence + 0x28u)) < s32(loops)) {
                releaseChannelBuffers(core, sequence, 0x8008C26Cu, 0x8008C284u);
                const std::uint32_t retired = measuredGuestCall(core, kRetireObject, 0x8008C29Cu, 3u, object);
                core.r[29] += 0x50u;
                return retired;
              }
            }
            core.mem_w32(node + 0x20u, 0x1000u);
            core.mem_w32(node + 0x28u, 0x1000u);
            core.mem_w32(node + 0x24u, 0x1000u);
            core.mem_w16(node + 0x76u, 0u);
            core.mem_w16(node + 0x52u, 1u);
            if (core.mem_r32(kSceneMode) == kSceneModeAnimated) {
              core.mem_w32(node, core.mem_r32(node) & 0xFFFF7FFFu);
            }
            core.mem_w32(channel + 8u, 0u);
          }
        }
        node = core.mem_r32(node + 0x5Cu);
      } else if (mode == 1) {
        core.mem_w32(stateSlot,
                     (s32(core.mem_r32(sequence + 0x1Cu)) << 8) <= s32(core.mem_r32(sequence + 0x14u)) ? 1u : 0u);
        node = measuredGuestCall(core, kAnimateChannels, 0x8008BC50u, 4u, sequence, node, channel, stateSlot);
        if (core.mem_r32(object + 0x50u) == 0 && core.mem_r32(stateSlot) == 2u) {
          releaseChannelBuffers(core, sequence, 0x8008BCA8u, 0x8008BCC0u);
          const std::uint32_t retired = measuredGuestCall(core, kRetireObject, 0x8008C29Cu, 3u, object);
          core.r[29] += 0x50u;
          return retired;
        }
      }
    }

    if ((s32(core.mem_r32(sequence + 0x1Cu)) << 8) <= s32(core.mem_r32(sequence + 0x14u))) {
      core.mem_w32(sequence + 0x14u, 0u);
      if (core.mem_r32(kSceneMode) == kSceneModeAnimated) {
        core.mem_w32(kAnimationRetired, 1u);
      }
    }
  }

  static_cast<void>(hsyncSample(core, 2u)); // dead retail sample
  core.r[29] += 0x50u;
  return sample;
}

void bootAnimateChannelsOwned(Core *core) {
  core->r[2] = animateChannels(*core, core->r[4], core->r[5], core->r[6], core->r[7]);
}

void bootObjectUpdateOwned(Core *core) {
  core->r[2] = objectUpdate(*core, core->r[4]);
}

} // namespace

void registerBootObjectCallbackOverrides(Core &core) {
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Boot, kAnimateChannels, "CrashBash::BootAnimateChannels", bootAnimateChannelsOwned);
  runtime::registerNativeOverride(
      core, runtime::GuestImage::Boot, kObjectUpdate, "CrashBash::BootObjectUpdate", bootObjectUpdateOwned);
}

} // namespace crashbash
