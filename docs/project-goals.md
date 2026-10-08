# Project goals

## Product architecture

Crash Bash is one native/dynarec hybrid product. Title-owned native overrides execute the behavior
the port deliberately owns; every remaining guest instruction executes on demand through psxport's
pinned Lightrec integration from the user's authenticated USA game image.

Every cold block is offered to Lightrec first. Bounded fallback is permitted only after an explicit JIT
compile/fetch failure, reporting the PC, reason, block/instruction counts, and return to JIT dispatch.
Interpreter-only execution remains a test/diagnostic path. No build,
provisioning, installation, or launch path may emit or compile a static guest-code corpus.

Pixel-exact agreement is not a completion gate. The executable, assets, simulation, input, timing,
audio, and scene semantics remain authoritative; independent comparison is a diagnostic for finding
causes. Presentation is accepted by driving the running product through representative gameplay and
checking that it works and looks correct.

## G001 — A faithful, portable Crash Bash PC port

Crash Bash should run from a user-supplied verified USA disc through the default launcher, preserving
the retail game's observable simulation, input, audio, and presentation behavior.

Success conditions:

- `./run.sh` authenticates the user's game input, builds the intended product with a supported host
  compiler, and launches without maintainer-only RE tools or offline guest translation.
- psxport dynamically translates every non-native guest path through its pinned Lightrec revision.
- Gameplay is dynarec-first, with nonzero translated execution and bounded fallback counts by reason.
  Interpreter-only selection is diagnostic and never the product default.
- Every native override installation stays active (`tools/source_policy.py` counts them). Every call from a native owner to a
  guest implementation use psxport's scoped runtime original-call operation, which bypasses
  only the current override and executes the authenticated guest body through Lightrec.
- The static generator, generated corpus, dispatcher, seeds, and static-only checks are absent before
  dynarec implementation begins and cannot be restored as a bridge.
- Independent emulator, binary, or evidence from a separately built test target, including diagnostics,
  remains available to locate
  the first divergence without retaining the static product as an oracle.

Constraints and non-goals: copyrighted disc and module bytes remain untracked; boot, a logo, a menu,
or a single rendered frame is not representative-gameplay conformance; an interpreter-backed gameplay
mode or a compatibility static product is not shipped.

Contributing state items: S001-S004, S007-S017.

## G002 — Widescreen presentation

Crash Bash should present the guest's own picture with a wider horizontal field of view and unchanged
vertical framing.

Success conditions:

- The 4:3 picture is the GPU device's picture of the guest's GP0 stream (psxport `presentation.md`).
- Wider aspect ratios expand horizontal view without changing simulation, vertical framing, HUD intent,
  visibility policy, or guest RAM.
- Output is judged on the running product; retail state and image comparison locate the cause of a
  visible defect rather than defining the pass mark.

Constraints and non-goals: no host re-layout of guest 2D; a HUD element moves only when a title
producer anchors it. Every widening is gated on the configured aspect.

Contributing state items: S004, S005, S007, S016.

## G003 — Smooth 60 Hz presentation without faster simulation

Crash Bash should present camera and world motion at 60 Hz by blending keyed primitives between
consecutive frame records while leaving the simulation cadence and results unchanged.

Success conditions:

- Disabling interpolation presents exactly the 30 Hz record picture.
- An in-between lies between its neighbouring real frames and never mutates simulation or guest RAM.
- Primitives pair by producer key only (producer, guest object, named element); unkeyed primitives and
  cuts are drawn as the current frame.

Constraints and non-goals: no pairing by draw order, count, position or colour; cuts come from the
guest's own scene state, never a distance threshold.

Contributing state items: S004, S006, S007, S016.

## G004 — Loading removal

Remove storage latency and loading-only waits from every load the game performs, without changing
unrelated scripted timing or faking completion. Loading runs asynchronously and the product goes
straight to the next real presentation.

Success requires each measured load operation to deliver the same payload and terminal state as
retail while omitting its loading-only presentation. Logo screens accept Start/Cross through the
title's recovered cancellation route (or a purpose-built skip establishing the same lifecycle,
resource, and state invariants). Authored transition cutscenes are presentation, not loading, and
remain.

Faster simulation, bypassed lifecycle callbacks, written phase/timer/scene words, and presentation
tricks that hide a wait are not implementations of this goal. Loading removal is suppressed under
oracle comparison.

Contributing state: S020.
