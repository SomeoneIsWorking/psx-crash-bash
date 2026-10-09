# Project state

## Comparison baseline

The unmodified USA PlayStation release of *Crash Bash* on original hardware or a PS1 emulator: retail
game modes, a 4:3 camera, 30 Hz presentation, console CPU execution. The intended product
authenticates the user's game image, executes selected behavior in native title owners, dynamically
translates all remaining MIPS through psxport's pinned Lightrec revision, widens the camera, and adds
60 Hz interpolated presentation without accelerating simulation.

User-visible deltas from that baseline, as the product stands:

- **Picture**: the guest's own GP0 output, rasterized from psxport's frame record at the internal
  resolution; at 1x 4:3 it equals the GPU device's picture.
- **60 fps**: one keyed in-between per logic frame (`fps60`); the 30 Hz picture with it off.
- **Widescreen**: the record canvas widens the display (512 → 684 at 16:9) and the guest's own
  geometry fills the margins; the HUD stays at its 4:3 position (issue 0035).
- **Loading**: load waits and the LOADING card removed (S020).

## Current focus

**S016 through S003, then S013.** Crash Bash runs in parallel with the active Spyro 1 title on the
native/dynarec product; nothing it lands may regress Spyro 1's gates. Finish list, in order:

1. **S016 remainder**: audio/timing coverage from the headless WAV sink, and per-frame frame-time
   evidence on the other released host architectures.
2. **S003 ledger**: complete image publication for every resident and nested module on one
   representative run. The Crashball leg publishes BOOT, MENU, DAT28136 and DAT28241 in one run;
   DAT28272, DAT28382 and DAT22510 have not been seen alongside them.
3. **Issue 0030's remaining half**: the framework-side negative case for a resume whose image
   generation has been retired.
4. **S015**: all 27 overrides by image identity and the 11 original calls under the differential. The
   live-match leg is already a concrete instance: `0x800B4694` is DAT28136's registered callback in
   one image generation and DAT28241's code in the next, at the same address.
5. **S016 / S009-S012**: requalify Battle Mode Crate Crush, Tournament Crate Crush, Polar Push and
   Pogo Painter on the dynarec product. `0x4000` is the menu confirm button, and the arena the loader
   reads is chosen on SELECT BATTLE TYPE (`0x800BA72C`), not on the arena screen.
6. **S013**: the remaining modes. **S014**: audio requalification.
7. Then the record-path presentation gap: HUD anchoring (issue 0035).

## Capability inventory

| ID | Capability / observable outcome | State | Dependencies | Goals |
| --- | --- | --- | --- | --- |
| S001 | The selected USA disc, executable, and measured `CRASHBSH.DAT` modules are reproducibly authenticated and provisioned | verified | — | G001 |
| S002 | The retail boot and loaded-image sequence have a recorded first-frame and menu frontier to re-establish through the dynarec | partial | S001, S003 | G001 |
| S003 | The gameplay product executes every non-native guest path through psxport's pinned Lightrec dynarec with bounded, reason-accounted fallback | partial | S001, shared psxport executor | G001 |
| S004 | Crash Bash graphics are the guest's own GP0 output on the record path and look correct across representative content | partial — `RenderPath::Record` is the only picture path; `recordcheck` mismatched=0 on 1,395/1,395 Polar Push and 3,741/3,741 Crashball presents at 4:3 with fps60 off; looked at in menus and both live matches | S002, S015 | G001, G002, G003 |
| S005 | Wider aspect ratios widen horizontal view without changing vertical framing | partial — `GuestWidescreenProjection` declares the aspect and the record canvas adds 86 columns per side at 16:9 (512 → 684), filled by the guest's own geometry in menus and Polar Push; 4:3 stays the device picture. No guest cull has been found to widen; margins not yet surveyed in every arena | S004 | G002 |
| S019 | Widescreen anchors the UI: edge HUD elements sit at the widened edges or safe area, centred elements stay centred, nothing stretches | missing — the native anchoring policy was deleted with the native renderer; on the record path the HUD stays at its 4:3 position (issue 0035) | S005 | G002 |
| S006 | Camera and world motion render between simulation ticks | partial — the `0x80019A60` producer keys every model face by object and face and the UI producers key text glyphs, panel parts and HUD digits (`docs/codemap.md`, Producers): 99.97% of Polar Push (1,661,862 of 1,662,395) and of Crashball (5,902,090 of 5,904,051), only the static LOADING card left, 0 duplicate keys; in-betweens differ from and lie between their neighbours; a re-initialised component keys as a new object (issue 0037); the four component producers (model faces, text, panels, quads) are state producers whose render runs the native body of `0x800193A8` and the three 2D leaves on inputs blended at t (codemap, State producers); the override differential matches those four bodies on 6,624 sampled Polar Push calls; Polar Push `recordcheck` at 4:3 is mismatched=0 on 1,391/1,391 presents with fps60 off and 1,420/1,420 with it on; BOOT's HUD and menu items still rely on `keyedBlend`, and text/panel placement is blended at the leaf arguments (issue 0039) | S004 | G003 |
| S007 | Deterministic diagnostics compare reached hybrid-product boundaries with independent retail behavior and prove both answers | partial | S001, S003 | G001, G002, G003 |
| S008 | The retail game modes are reachable and playable end to end on the hybrid product | partial | S002, S003, S004, S015 | G001 |
| S009 | The Crashball gameplay scenario reaches a live match and accepts player control | verified | `replays/flow/crashball-control.pad` drives a live match, guest movement words are proven against an idle control leg, 0 fallback | G001 |
| S010 | Battle Mode Crate Crush reaches a live match and accepts player control | verified | `replays/flow/battle-crate-crush-control.pad`, phase-keyed v1, 9,239 of 9,239 frames delivered | G001 |
| S011 | Tournament Mode reaches its first live Crate Crush match and accepts player control | verified | `replays/flow/tournament-crate-crush-control.pad`, phase-keyed v1, 12 segments, 9,240 of 9,240 frames delivered | G001 |
| S012 | Polar Push reaches a visually correct, controllable live match | verified | `replays/flow/polar-push-control.pad` reaches the live DAT22510 match with P1 answering a held direction | G001 |
| S013 | The remaining retail modes are reachable and playable | partial — Pogo Painter plays on Lightrec (`replays/flow/pogo-painter-control.pad` reaches a live DAT28272 match). On a fresh card Battle offers Crate Crush, Polar Push, Pogo Pandemonium and Ballistix; Tank Wars, Crash Dash and Medieval Mayhem show retail's own LOCKED page and need Adventure progress on the card, which is the next route | S008 | G001 |
| S014 | Retail music and sound effects play at the correct rate without premature truncation | partial | S003 | G001 |
| S015 | All 27 native override registrations install by runtime image identity and all 12 original-body calls execute through the dynarec | partial | S003 | G001 |
| S016 | Representative interactive gameplay passes on the native/dynarec product | partial | S003, S004, S005, S006, S007, S008, S014, S015 | G001, G002, G003 |
| S017 | Every static product path is deleted before dynarec implementation and mechanically excluded | verified | — | G001 |
| S018 | Hosted CI truthfully covers applicable Linux, Windows, macOS, and Android product boundaries | partial | S003 | G001 |
| S020 | Crash Bash: load operations complete without loading-only waits or presentation; logos cancel through the recovered route | verified | S003 | G004 |
| S021 | A whole-machine save state restores into any session (other minigame, menu, same match) with image identity, generations and native keys matching the restored RAM | verified | S003, S015 | G001 |

## Capability details

### S001 — Authenticated retail inputs

The USA disc resolves through `SYSTEM.CNF` to the exact `SCUS_945.70` identity, and tracked manifests
identify the resident executable plus all measured `CRASHBSH.DAT` images without tracking copyrighted
bytes.

### S002 — Boot and loaded-image frontier

Recorded runs reach the first presented frame, MENU `0x800B5244`, and the Cross-selected DAT28136
registration/update boundary. The direct native/dynarec player authenticates the resident executable
and the completed 189-sector BOOT and 16-sector MENU reads, preserves untouched BOOT code when MENU
overwrites only part of it, and crosses the former MENU identity fault at `0x800B5244`.

Gap: re-establish the complete reached sequence through pinned Lightrec with nonzero dynamic
execution, image-correct invalidation, bounded exits, and reason-accounted fallback.

### S003 — Pinned-Lightrec gameplay executor

The direct player links pinned Lightrec as its first guest executor, and the asset-free product-link
and selector checks exclude an interpreter-only default. Whole-run translated/fallback accounting
belongs to psxport (`execution_ledger.h`); this title adds only its guest-call turn cap
(`reportGuestCallTurnCap`) and one authenticated-image line per publication, and never re-derives a
framework counter.

The numbered facts that matter:

- `Core::writeGuestMemory` issues one `notifyExecutableWrite` per mapped guest store, so a
  request-count total reads orders of magnitude too large. Quote the revoked-block count, or psxport's
  `invalidation_work` and `invalidations_by_source` lines, never the request count.
- `PSXPORT_VK_HEADLESS` is genuinely never read and that is correct: the renderer is headless because
  a window is the switch. A `NOTHING ever read it` audit line is a true report of a no-op, not
  evidence that the renderer failed to initialise.
- Quote the frame driver's own `dwelling in state 0x… for N frame(s)` line for this title. Counting
  fields without counting distinct states reports the attract cycle as a large advance.
- A refused guest call is the product's one refusal route: the reason is logged and the process
  aborts. `std::abort()` runs no destructor, so the framework's own shutdown telemetry is absent on
  that path (issue 0033); quote a refused run from the error line, not from a run-end report.
- Two execution facts stay unexposed to a port and are named as exact framework changes
  (issue 0033): an admitted interpreter fallback leaves no port-visible site, and "overrides hit" has
  no framework counter at all.

A keyed 4,000-frame run with a real per-frame pad replay leaves the attract cycle, publishes DAT28136,
and presents 99%+ non-black frames at Crashball character select with zero fallback. A 3,740-frame
live-match leg publishes MENU, DAT28136, then DAT28241 at the shared `0x800B32B4` slot, where the same
address `0x800B4694` holds DAT28136's callback in one generation and DAT28241's code in the next.

Gap: representative gameplay, complete image publication, and the two framework counters above.

### S004 — Record-path graphics

The guest draws every frame through its own ordering table; psxport's `GpuDevice` executes it and the
record rasterizer replays the frame record (`RenderPath::Record`, the only path `TitleAdapter`
declares). Evidence (2026-10-07, `PSXPORT_DEBUG=recordcheck`, 4:3, fps60 off): Polar Push replay
1,395/1,395 presents mismatched=0, Crashball replay 3,741/3,741. Producers do not change the picture:
the f1370 Polar Push shot is byte-identical with and without them. The record path also draws what
the deleted native renderer missed (Polar Push's power bars under the portraits).

Gap: survey the remaining modes and transitions on the record path.

### S005 — Widescreen

`TitleAdapter::guestWidescreenProjection` declares the player's aspect (`aspect` in the settings file:
0 = 4:3, 1 = 16:9); the record canvas keeps the guest's 512-wide draw and adds the margins, and every
primitive whose draw area spans the display draws into them. Nothing in guest RAM is changed and 4:3
presents the device picture. In the SELECT NUMBER OF PLAYERS menu and Polar Push the margins are
continuous world geometry.

Gap: survey every arena for margin pop-in; a guest cull that limits the margins would be widened by a
title override of that cull, never by a host heuristic.

The arena the loader reads is `DAT_8009E5DC` (`0x8009E5DC`), written by the scene dispatcher
`FUN_8007F314` from `FUN_800793E8`'s index into the six-byte arena table at `0x8004DDD0` and filled in
`MENU.BIN` by `FUN_800B7EF4` from the SELECT BATTLE TYPE cursor pair. One Left on that screen is Crate
Crush; Tournament is the same mode screen with one more Down and its own flow table
`PTR_PTR_800B8ED4`.

### S006 — Interpolated presentation

`render::registerModelFaceProducer` keys the guest's model packets: the `0x80019A60` producer opens
(object incarnation) and the `0x800193A8` namer names each face by its mesh's face list and index.
`render::registerComponentIncarnationOwners` overrides every copier of the render template, so a
component the guest re-initialises in place (a briefing page flip, a pooled slot handed out again) keys
as a new object. `FrameCut`
declares a cut when the process state, scene (`0x8009F658`) or menu screen (`0x8009F8A4`) changed.
`FramePresenter` presents `keyedBlend(N-1, N, 0.5)` then N per logic frame with `fps60` on.

Evidence (2026-10-07): census over Polar Push to f1370 keyed 1,554,644 of 1,618,751 primitives, every
remaining one under the menu/UI present `0x80010278` or the LOADING card; live-match frames 1,804 of
1,812 keyed; Crashball 5,809,153 of 5,904,051, live match f3400+ 659,995 of 663,055. Duplicate keys 0
in every frame of both runs. Polar Push f1360-f1367 at fps60: 12 consecutive presents all differ (ImageMagick AE 918-1,368)
and every middle present is closer to each neighbour than the neighbours are to each other.

Evidence (2026-10-07, incarnations): Polar Push briefing page flip f1086→f1087, whose `0x800A188C`
sprite stepped 307 px in the in-between, now presents the in-between equal to f1087 (AE 0; before: AE
120 against f1087 and 5,009 against f1086). The f1316→f1317 effect `0x801DD920` is one incarnation
(begun at its f1316 draw), and the guest itself draws it 83-116 px apart in those two frames, so it
still blends. f1360-f1367 at fps60: all 16 presents differ, every in-between closer to each neighbour
than they are to each other. Recordcheck at 4:3 with fps60 off: 1,395 records, mismatched=0. Duplicate
keys 0 in every blended frame of Polar Push (1,076) and Crashball (3,642).

**The retail cadence is 2 FIELDS PER GAME FRAME = 30 fields/s** (issue 0031). Four of five writers
store the literal 2 into the rate global `0x8004E0E0` (`guest::kDisplayFieldsPerFrame`); the fifth
(BOOT `0x80094694`) stores 3 and is not established as per-frame, which is why the denominator is five
writers. `0x800320EC` proves the VSync argument is a field count — `a0 <= 1` takes the no-wait branch
and only `a0 >= 2` waits — so **60 fps is structurally unpaceable on the guest path** and the in-between
presentation is the only route to it. The title is therefore in lerp scope.

Evidence (2026-10-07, UI producers, psxport 7e8d4fb1): `render::registerUiProducers` keys the text,
panel and quad component callbacks and BOOT's HUD and menu page; census Polar Push 1,661,862 of 1,662,395
and Crashball 5,902,090 of 5,904,051 keyed (before: 1,598,087 and 5,809,153), duplicate keys 0,
recordcheck mismatched=0 on all presents at 4:3 with fps60 off. The Crashball panel sliding over
f1240-f1254 sits at x=106, 104, 102 in real f1240, the in-between and real f1241 (issue 0036).

Gap: none known on the flow replays; the unobserved UI paths are listed in issue 0036.

### S007 — Independent diagnostics

The comparison tooling that fed the deleted oracle comparators is gone; the remaining deterministic
judges cover interrupt ordering, module loads, input delivery, menu transition, native ownership, and
graphics attribution with controlled opposite answers.

Gap: migrate every still-useful judge to the shipping native/dynarec boundary and add product-link,
selector, translated-block, override/original-call, invalidation, and representative-gameplay coverage.

### S008 — End-to-end retail mode coverage

Durable bounded scenarios cover live, controllable Crashball, Battle Crate Crush, Tournament Crate
Crush, and Polar Push behavior.

Gap: re-run that frontier through pinned Lightrec and cover every remaining retail mode family.

### S013 — Remaining modes

The control-channel-only `arena <id> [battle|tournament]` command (`game/debug/dev_arena.cpp`, Tomba! 2
`dev_warp` pattern) enters a match at a frame boundary through the menu's own flow step on the located
battle flow run and the guest's own accepts.

### S014 — Audio playback

The measured host sink sustained 44,097 samples/s against the 44,100 Hz target with zero dropped
fields, while the bounded pre/post WAV remained byte-identical.

Gap: requalify hardware listening, longer gameplay, and every title audio path through
pinned-Lightrec gameplay.

### S015 — Runtime overrides and original calls

The title sources register 27 native overrides through the single image-qualified
`runtime::registerNativeOverride` boundary and route 11 calls through the single scoped
`runtime::callOriginal` boundary; `tools/source_policy.py` reports both denominators and
proves the forbidden old paths are detected.

`game/execution/guest_execution.{h,cpp}` supplies the per-Core adapter. Native owners can register
before their module is resident. The authenticated loader supplies the logical image, shared catalog
identity/generation, and complete physical range; registration publication and original calls reject a
mismatched residency. Original calls use shared scoped suppression, and replacement or unbind removes
the old generation's native keys. The context owns no independent guest image catalog.

`game/entry/player_entry.cpp` composes `TitleAdapter` with the heap-owned `Game`, authenticates the
resident executable against `titles/crashbash/executable.json`, binds per-Core hardware owners, and
enters `native_boot_run`. Failed authentication preserves guest state; successful replacement retires
the prior resident generation.

Gap: qualify all 27 installations and 11 original calls on real loaded-image and gameplay routes. The
shared memory-card path also needs OS user-data configuration; its scratch fallback is not a
releasable save location.

### S016 — Representative gameplay

Partial capability: an **interactive live Crashball match** on the dynarec-first product, and Pogo
Painter live in DAT28272.

How the match is reached, from the guest's own code and nothing else: Cross taps every 32 frames
through frame 1192 leave the attract cycle and reach the menu, but **Cross alone then cycles the
roster instead of confirming**; the route needs the 800-frame Circle hold at frames 1200-1999, then
Cross at 2000 publishes DAT28241 and starts the match, then Cross at 2560 leaves the briefing.

Player movement is proven against a control leg, not asserted: two runs of the same replay differing
only in 180 frames of direction input, with whole-RAM dumps at eight frames. The parsed and active-high
pad words move under Left and Right and stay at rest in the control; 159 of 524,288 scanned words move
monotonically with a sign flip on the commanded axis; the shared start frame differs in 0 words.

Citation status, stated precisely: `0x8005133C` is fully attributable; `0x80063A92` and the movement
words are reached through a base register no `lui` materialises, so **the individual store instruction
is not yet attributed and is not claimed**. The one materialised base in that band is `0x80056998` at
BOOT `0x800825B4` / `0x80082624`.

Frame time over the match leg is ~10-11 ms per frame unpaced; the worst outlier is a one-off module
instantiation. These are unpaced host CPU costs, not a gameplay rate claim: the title still runs two
display fields per game frame. The `audio` phase slot reads 0.00 **by construction** — the per-field
SPU advance is inside the game-logic span because `GpuPerf`'s phases are a partition, and a nested
bracket would double-charge the same wall time.

The per-frame profiler channel is **`PSXPORT_DEBUG`**, not `LUCENT_DEBUG`, and the title brackets
`Game::perf.frameBegin()/frameEnd()` in the frame driver; without both brackets the healthy framework
profiler emits no line at all, which reads exactly like a clean measurement of absence.

Still missing for `verified`: audio/timing coverage from the headless WAV sink, per-frame frame-time
evidence on each other released host architecture, and requalification of Battle, Tournament and
Polar Push on the dynarec product.

### S017 — Break-first static-path removal

The tracked offline emitter integration, seed file, generated registry installer, and static-only
verifiers are deleted. The ignored generated corpus, prior static build tree, and retained static
product binaries are absent. `tools/source_policy.py` rejects the old files, generated directory,
static dispatch markers, and any change to the override/original-call source boundary.

### S018 — Platform CI coverage

`.github/workflows/ci.yml` configures a Linux x86-64 native adapter job with full history, read-only
permissions, pinned actions, and an explicit timeout. It resolves the framework with
`tools/psxport_fetch.py` (a clone of psxport `main` in CI, which has no sibling checkout), uses that
checkout's shared Linux setup action, and runs `tools/verify.py`. The thin title verifier selects the
real `crashbash_title_adapter_test` artifact and every `crashbash_` CTest; shared `ConsumerVerifier`
owns build, style/test execution, and linked execution-boundary checks.

| Platform | Applicability | Current CI evidence and exact gap |
| --- | --- | --- |
| Linux x86-64 | applicable desktop target | Native adapter build, complete title tests/style, and execution-boundary inspection are configured through the shared verifier; hosted result and packaged gameplay remain unverified. |
| Windows x86-64 | applicable portable-PC target | Missing: no supported Windows native build, runtime test, first-run setup, or package boundary exists. |
| macOS arm64 | applicable portable-PC target | Missing: no Apple-Silicon native build, runtime test, first-run setup, or application package exists. |
| Android arm64 | applicable mobile target | Android metadata and setup sources exist, but the Lightrec native runtime, shared `android-port` build path, Gradle/NDK APK build, and install/runtime test are missing. |

Gap: the former green Android metadata/selftest job was removed because it did not build or install an
APK. Add Android and desktop jobs only when they drive the actual redistributable platform boundary
without game assets.

### S020 — Crash Bash loading removal

Verified. The loading-only wait is gone, and the guest's own LOADING card no longer reaches the
display. The transition clock the handoff runs on is authored and is kept.

**Where the wait was.** Retail's load pump `FUN_8001231C` advances the queue one step per call and
returns 0. Seven of its eight callers are load-completion loops that spend no frames; `0x8001039C`,
the main per-frame call, is the entire wait — three cooldown frames per queued read on top of a disc
read that completes synchronously here. The `kLoadPump` override therefore runs retail's pump to
quiescence at that one site and gives every other caller exactly retail's single step.

**The two helpers stay with the loop callers.** `0x80010AE8(&DAT_8004E0F0)` drains frame-heap work a
read completion enqueues and `0x8002BAE8` publishes the pending-completion record; retail calls both
only inside the seven loops. Calling them from the per-frame site is not a shortcut — a drain that also
emptied the queue from the loop callers left the Polar Push briefing refusing Cross with no read ever
requested.

**What the LOADING card turned out to be.** The card is not a scene of its own: `0x800A00DC` IS the
game's transition scene, and the card is drawn by the resident `FUN_80018B08`, which BOOT reaches only
from the two handoff screen presents (`0x80094248` and `0x80094188`). Those two sites are the whole of
the card's presentation, and `game/disc/loading_card_skip.{h,cpp}` retires the card there while every
other caller — the menu screens draw their own panels through it — runs the guest's own body.

**Phase-keyed replays.** `replays/flow/polar-push-control.pad` is phase-keyed v1 (1,394 frames, 13
segments, keyed through `crashbash::InputPhase`, unit-tested in `tests/test_input_phase.cpp`), so it
survives the collapse of the handoff the way an absolute recording does not. `crashball-control.pad` is
still absolute from boot and is checked against S020 rather than assumed.

## Dynamic migration acceptance

The first Crash Bash dynamic milestone must prove all of the following together:

- the exact authenticated resident and loaded images execute through the pinned psxport/Lightrec
  integration with nonzero translated-block execution;
- product link and configuration inspection excludes an interpreter-only default, with every fallback
  reached only after an explicit JIT rejection and bounded by reason-accounted counters;
- all 35 native override registrations are keyed by
  complete runtime image identity and address;
- all 11 original-body calls use a scoped original call that suppresses only the current
  override, enters the original guest body through Lightrec, and returns with correct guest state;
- loaded-image replacement at the shared `0x800B32B4` slot invalidates affected translated blocks and
  cannot reuse an override or block under the wrong image identity;
- bounded executor exits preserve VSync/frame, interrupt, exception, host-work, and thread-exit
  ownership without unwinding through translated host frames;
- the existing independently controlled boot, Cross-menu, Crashball, Battle Crate Crush, Tournament
  Crate Crush, and Polar Push scenarios reach their recorded boundaries without a guest-VSync
  violation, wrong-image dispatch, or missing native owner.

Boot, logos, menus, and a first frame are checkpoints only. S016 requires representative interactive
gameplay with observable movement, correct rendering, audio/timing coverage, and declared frame-time
evidence on each released host architecture. The static execution path is already absent and cannot be
used to obtain new evidence; comparison comes from the independent emulator, binary analysis, or a
separately built test target.

## Retained verified title facts

These facts survive the execution-engine migration because they describe the retail binary, user-visible
behavior, or native subsystem contracts rather than the retired translation method.

### Authenticated images and overlay identity

- USA `SYSTEM.CNF` boots `SCUS_945.70`. The PS-X EXE has SHA-256
  `fd5727a18feb2a2d5a6359a55966f0266284d1e50f64ee9b8a127a97091bd516`, entry `0x8002E7B0`, load
  address `0x80010000`, text size `0x69000`.
- BOOT is at LBA 35799. MENU, DAT28272, DAT28241, DAT28136, DAT28382, and DAT22510 are authenticated
  alternatives in the reused nested load region beginning at `0x800B32B4`; their tracked manifests
  remain the identity authority.
- Polar Push DAT22510 is `CRASHBSH.DAT + 0x02B81000`, 71 sectors, size `0x23800`, SHA-256
  `7477dd74ccd80f8f1b8ce63265f8acf130e36a97f1e9182767383556e8ffc7e1`, loaded at `0x800B32B4`.
- DAT28382 installs initializer `0x800BB370`. DAT22510 uses callback `0x800CBA64`. The measured Cross
  path registers DAT28136 at `0x800B4E1C`, replaces application callback `0x80093038` with
  `0x800B4694`, and then executes that update.
- DAT28136 is the 42-sector image at `CRASHBSH.DAT + 0x0367E000`. DAT28241 is the 31-sector image
  from LBA 28241. DAT28272 is the 38-sector image from LBA 28272; its code registers a behavior
  vtable at `0x8005AA70`, while its name table identifies animation states rather than a second menu
  phase.

### Boot, device, and frame ownership

- ResetCallback `0x80031A80` calls setjmp `0x8003ACEC` and resumes at `0x80031AE8`; the independent
  oracle observes `0x80031AE8` (`v0=1`, `sp=0x80068B14`) to `0x80031B58` with the same stack and
  `ra=0x80031AF8`.
- The finite native boot owner begins from the measured one-shot `0x8002718C` / `0x80010158` prefix;
  repeating process work remains owned by the frame driver.
- The IRQ2 callback is `0x8003F5F0`, draining through `0x8003E14C`. The VBlank/SIO chain registers
  class 2 element `0x8006D984`; verifier `0x8003B1BC` tests I_STAT bit 0 and handler `0x8003B224`
  drives SIO0, per-byte I_STAT bit 7, and timer 2.
- Native ownership boundaries include memory-card startup `0x800486DC`, libcd command/sync
  `0x8003EBF8 -> 0x8003E6B0`, TOC readiness `0x800349AC -> 0x8003584C`, file-read start
  `0x80027790 -> 0x8003470C`, controller handshake `0x80034B8C`, and disc/license state machine
  `0x8002D4F4`. BOOT object callbacks `0x8008ADA4` and `0x8008BB48` are native frame owners.
- The card-event callback is `0x8004718C`; retail code constructs its address at `0x80047280` rather
  than storing a discoverable pointer. The libmcrd device-table walk at `0x8004799C` returns zero for
  both "no such device" and "request started", so publishing the `bu` device and its completion event
  are independently required.
- The disc owner performs real CHD reads and may complete them synchronously, but it must not fake
  completion, manufacture a guest VSync clock, or weaken frame/watchdog ownership.

### Input and representative flow scenarios

- The pad chain changes packet `0x80077FBC` from `41 5A FF FF` to `41 5A F7 FF`, parsed P1 `0x80063A92`
  from `FFFF` to `FFF7`, and active-high P1 `0x8005133C` from 0 to 8; P2 `0x80051394` remains 0.
  Direct-buffer injection is not an accepted path.
- The active MENU table `0x800B8E28` updates through `0x800B3CA8`. Rising-edge input `0x80051380`
  accepts Cross `0x4000` at `0x800B3D88-0x800B3D8C` and schedules table `0x800B8E50` through
  `0x8009F8A8`. START is not a substitute for this transition. `kAppModeVtable` (`0x8004E0DC`) is NOT
  the mode selector — the scene record at `0x8009F658` is.
- Portal-selection byte `0x8005A677` changes from `0xFF` to `0x00` before the DAT28241 load. Guest
  state `0x8004E0B8` and application mode `0x80078C90` remain the measured transition witnesses.
- The menu pad bits are the game's own active-high PSX set: Up `0x0010`, Right `0x0020`, Down
  `0x0040`, Left `0x0080`, back `0x1000`, confirm `0x4000`.
- The retained scenarios are the minimum set for dynarec requalification, not a claim that every
  retail mode is covered: `replays/flow/crashball-control.pad` (3,740 frames, Left held 3560-3619 and
  Right 3620-3739 in a live DAT28241 match), `battle-crate-crush-control.pad`,
  `tournament-crate-crush-control.pad`, and `polar-push-control.pad`.

### Guest graphics and producer contracts

- Model draws converge on `0x80019A60(frame code, model data, flags, object)` from the standard
  (`0x80019F1C`) and alternate (`0x8001DD50`) object draw callbacks; it draws a 0x3000 code as a flat
  sprite through `0x80029D28` and a mesh through `0x800193A8`.
- `0x80019094` pops a 10-word-per-face packet buffer from a free list per (model frame, display parity);
  `0x800193A8` writes face i at `buffer + i * 40` from `{flags, count}` strip pairs ended by count
  0xFF, culled or not. Objects drawing one model frame take buffers in call order, so the object, not
  the buffer, is the producer key.
- `0x800274FC` and `0x800276C4` are the two heap-pool allocators; their live base/end ranges are the
  packet-pool windows the producer census attributes 2D packets through.
- `0x80018B08` is viewport/camera/render-list setup and the LOADING card draw.