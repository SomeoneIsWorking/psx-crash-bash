# Presentation — Crash Bash

How Crash Bash's guest draws reach the screen and how its 60 fps in-betweens are made: the game's own
draw section replayed with blended inputs (psxport `docs/plans/replay-interpolation.md`, `ReplayFrame`).
The RE map behind it is `docs/plans/replay-interpolation.md`; owners are in `docs/codemap.md`.

## Guest draw → presentation

`FrameDriver::stepFrame` runs the process state's update, then `finishUpdateSlice` runs its present
`0x80010278` (scratchpad stack, root scene present, `DisplayFrame`). `DisplayFrame`
(`registerDisplayFrameOverride`) hands the table to the guest's own DrawOTag and carries the draw base
`0x800569D8` into the table walked next (`ordering_table_slots.cpp:followDrawBase`) → `psxport:GpuDevice`
executes it into the frame record → `FramePresenter` seals it, asks `Runtime::sealedFrameIsCut`
(`FrameCut`: process state, mode scene or menu screen changed) and presents the in-betweens
`Runtime::replayFrame` captured, then the record.

- **Widescreen**: `Runtime::guestWidescreenProjection` declares the player's aspect; the record canvas
  adds the margins and the guest's own primitives fill them. The HUD stays where the guest draws it
  (issue 0035).
- **A frame that presented nothing** goes to `Game::presentation.commitUnpresented`, which seals no record.

## In-betweens

`DrawReplay` (`game/frame/draw_replay.*`, a `Session` member) owns the title's `ReplayFrame`.

- **Section** (`DrawSection`): the root scene's present `(*(*0x8004E0DC + 8))()` dispatched with
  `sp = 0x1F8003FC`, as `0x80010278` calls it, without the `DisplayFrame` that follows. Its chain head is
  the last bucket of the table in use (`*0x8005B68C + 0x3FFC`); `linked` accepts either table's head.
  Every model draw runs in this present; the update links only texture-scroll copies and the pause page,
  which stay in the table and are presented as drawn.
- **When**: `DrawReplay::beforePresent` runs before every present. It forgets every sample unless the
  process state's present is `0x80010278`, or when a frame ran no update and present.
- **Tracks and sources**, sampled at the section's entry and written t of the way into each replay:
  - `scene fade`: the scene clock `0x8009F644` BOOT's mode present copies into the screen fade.
  - `ContextMotion` (`game/render/context_motion.*`): every draw context the real draw selected through
    `FUN_80018B08` (gameplay world `0x80051640`, HUD `0x8009F734`, menu cameras): translation +0x00..+0x08,
    eye +0x0C..+0x14, H +0x18, view angles +0x54/+0x58. `FUN_80014540` derives the view from these, so
    everything the draw places from the camera follows.
  - `ComponentMotion` (`game/render/component_motion.*`): every render component `0x80019A60` drew (a3)
    or `FUN_8001ACC0` turned into a root transform (a0). Position and scale blend straight; angles the short
    way with the rebuild halfword +0x52 set so the draw rebuilds +0x30 from them, unless rotation order +0x51
    is 4 (the context's rotation); otherwise the MATRIX at +0x30 blends as a transform when neither sample has
    a rebuild pending. The weighted pose (+0x70 progress, +0x72 frame) blends as one phase; a keyframe step
    of one animation (+0x74 code k to k+1) is drawn as the weighted pose from k toward k+1.
- **Identity**: a component pairs by address, `ComponentIncarnations` generation (every copy of the render
  template `0x8005A830` starts a life; its copiers are listed in `component_incarnation.h`) and model +0x6C;
  a context by address. Unpaired records are drawn as they are now. BOOT's scratch component `0x800A0C74`
  is refilled inside the draw and is not sampled.
- **Sandbox**: `GuestRedraw` journals every store and rolls it back; a device access refuses the in-between
  and the interval shows the real frame. The notes (`noteSelected`, `noteDrawn`) ignore calls made while
  replaying, and `ComponentIncarnations::beginLife` does not count one.
- **Run end**: `[replay]` intervals replayed and refused, `[motion]` unpaired records per source.

Retired at the override: the LOADING card's `FUN_80018B08` call (`disc/loading_card_skip.cpp:isLoadingCardDraw`).

## How to check it

Hidden run at the player's settings with a flow replay (`PSXPORT_PAD_REPLAY=replays/flow/polar-push-control.pad`),
then `preseq` through the control channel and judge crops of a moving object across presents; the run-end
`[replay]`/`[motion]` lines name refusals and unpaired records. Leak proof: the same replay with `fps60` 0 and 1
and `PSXPORT_DEBUG=fielddigest` reaches the same `ram=` hash at every guest field (matched by `ticks=`).

## Known gaps

- The hub portal ("PRESS X BUTTON TO START") present reads the DMA control register; its in-betweens are
  refused and it runs at the logic rate (issue 0048).
- Texture scrolls are linked in the update, so the Polar Push aurora scrolls only on real presents (issue 0047).
