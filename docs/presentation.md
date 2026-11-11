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
  which stay in the table. The hub portal's present clears a table of its own through ClearOTagR, whose OTC DMA
  body `FUN_80030220` is native (`frame/gpu_timeout.cpp:clearTableOwned`) so the section touches no device.
- **When**: `DrawReplay::beforePresent` runs before every present. It forgets every sample unless the
  process state's present is `0x80010278`, or when a frame ran no update and present.
- **Tracks and sources**, sampled at the section's entry and written t of the way into each replay:
  - `scene fade`: the scene clock `0x8009F644` BOOT's mode present copies into the screen fade.
  - `ContextMotion` (`game/render/context_motion.*`): every draw context whose view the real draw derived
    through `FUN_80014540` (gameplay world `0x80051640`, HUD `0x8009F734`, menu cameras): translation +0x00..+0x08,
    eye +0x0C..+0x14, H +0x18, view angles +0x54/+0x58. `FUN_80014540` derives the view from these, so
    everything the draw places from the camera follows.
  - `ComponentMotion` (`game/render/component_motion.*`): every render component `0x80019A60` drew (a3),
    `FUN_8001ACC0` turned into a root transform (a0), or the text, panel and quad callbacks `0x8001C448`,
    `0x8001C690`, `0x8001C7FC` drew (a0), each noted with the kind of draw.
    - Transform (models, text, and panels and quads without flag `0x10000000`): position and scale blend
      straight; angles the short way with the rebuild halfword +0x52 set so the draw rebuilds +0x30 from them,
      unless rotation order +0x51 is 4 (the context's rotation); otherwise the MATRIX at +0x30 blends as a
      transform when neither sample has a rebuild pending. Screen-space text (`FUN_8001A82C`) is placed by
      position x|y; a change of the x's centring bits steps.
    - Pose (models only), within one animation (+0x74 kind 0x4000, same animation bits): a weighted pose
      (+0x70 progress, +0x72 frame) blends as one phase when the later is ahead; a forward keyframe step of any
      size is drawn as the weighted pose the phase passed through. A loop wrap or an animation switch is drawn
      as now (issue 0050).
    - Screen-space panels and quads: the panel rect of the +0x6C record (x|y +0x70, depth|x1 +0x74, y1 +0x78;
      a change of the x's centring bits steps) and the quad's four corners (+0x6C, +0x74, +0x7C, +0x84).
  - `TextureScrollMotion` (`game/render/texture_scroll_motion.*`): every strip `FUN_80028F28` scrolled in the
    update, with the two DR_MOVEs it linked at the last bucket's head. Paired by scroller and strip, the scroll's
    whole rows blend the short way round the strip, and the replay rewrites both moves (rows s.. to the top, ..s
    below them) with the part of a row kept on their words (`GuestRedraw::writeCopy`), so the record draws them
    placed at scale.
- **Identity**: a component pairs by address, `ComponentIncarnations` generation (every copy of the render
  template `0x8005A830` starts a life; its copiers are listed in `component_incarnation.h`), draw kind and,
  except in screen space, model +0x6C; a context by address; a scroll by scroller address and strip. Unpaired
  records are drawn as they are now. BOOT's scratch component `0x800A0C74` is refilled inside the draw and is
  not sampled.
- **Sandbox**: `GuestRedraw` journals every store and rolls it back; a device access refuses the in-between
  and the interval shows the real frame. The notes (`noteSelected`, `noteDrawn`, `noteScrolled`) ignore calls
  made while replaying, and `component_incarnation.cpp:beginLife` begins no life for one.
- **Restore**: a state restore forgets every sample (psxport `MachineState::restore`).
- **Run end**: `[replay]` intervals replayed and refused, `[motion]` unpaired records per source.

Retired at its own override: the LOADING card's `FUN_80018B08` call (`disc/loading_card_skip.cpp`).

## How to check it

Hidden run at the player's settings with a flow replay (`PSXPORT_PAD_REPLAY=replays/flow/polar-push-control.pad`),
then `preseq` through the control channel and judge crops of a moving object across presents; the run-end
`[replay]`/`[motion]` lines name refusals and unpaired records. Leak proof: the same replay with `fps60` 0 and 1
and `PSXPORT_DEBUG=fielddigest` reaches the same `ram=` hash at every guest field (matched by `ticks=`).

## Known gaps

- No cut below scene level, and a pose does not cross-blend an animation switch or blend a loop wrap (issue 0050).
- The native ClearOTagR body leaves guest RAM different from the guest DMA path at a few fields, which
  re-converge the next field (issue 0048).
