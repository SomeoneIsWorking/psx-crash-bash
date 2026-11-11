# Replay interpolation — Crash Bash

60 fps in-betweens by replaying the game's own draw section with its moving inputs blended between two
logic ticks (psxport `docs/plans/replay-interpolation.md`). This file is the RE map the replay stands on and
what is left; how the implementation works is `docs/presentation.md`.

## Frame loop

- Process runner `0x800270F0` runs the current process state `{enter, update, present}` (`0x8005B648`). Every
  scene observed runs under state `0x8004E0B8`: update `0x80010394` (load pump, pads, root scene update), present
  `0x80010278`.
- `0x80010278`: moves the stack to the scratchpad (`FUN_800320DC`, `sp = 0x1F8003FC`), calls the root scene's
  present `(*(*0x8004E0DC + 8))()`, then `DisplayFrame` `0x800272AC` with the cadence word `0x8004E0E0`.
- BOOT root present `0x80092B7C` → `0x8001E898(0x8009F658)` → the mode scene's present `0x80092E94`:
  `(*0x8009F8B4)()`, screen fade `0x800569AC` = scene clock `0x8009F644`, `FUN_80082590`, then
  `0x8001E898(0x8009F8A4)` for the menu or arena scene.
- Mode update `0x80092EDC`: pause dispatcher `FUN_8007F314`, then `FUN_8007A7C0`, or the paused path. Texture
  scrolls (`0x8001DDB4` → `FUN_80029100` → `0x80028F28`, two DR_MOVEs) and the pause page are linked here,
  in the update.

## Draw section

The root scene present, dispatched with `sp = 0x1F8003FC`. A probe over boot, menus and Polar Push found every
model draw (`0x80019A60`) inside it; what the update links stays in the table and is captured as drawn.
The chain head is the last bucket of the table in use, `*0x8005B68C + 0x3FFC` (tables `0x8005B790`,
`0x8005F79C`, cleared by ClearOTagR each frame, so the section rebuilds rather than appends). At t = 1 the
replay's chain equals the real one in 1,633 of 1,633 frames.

## Inputs the draw reads

- **Camera**: `FUN_80018B08(viewport, context)` makes `context` current (`0x800569E0`) and `FUN_80014540`
  derives the view: `RotMatrix(context + 0x54)` → +0x74, translation from +0x00..+0x08 → +0x88, eye +0x0C..+0x14,
  H +0x18. Gameplay world context `0x80051640` (selected from BOOT `0x8009305C`), HUD `0x8009F734`.
- **Render component** (0xA8 bytes, chained at +0x5C): +0x00 flags (bit 31 weighted pose), +0x04..+0x0C
  position, +0x10 angles, +0x20..+0x28 scale, +0x30 MATRIX, +0x50 scale flag, +0x51 rotation order (4 = the
  context's rotation), +0x52 rebuild halfword (the draw rebuilds +0x30 from the angles through `FUN_800142D4`
  and clears it), +0x54 draw callback, +0x68 depth bias, +0x6C model, +0x70 progress, +0x72 frame, +0x74 frame
  code (`0x4000 | anim << 7 | frame`; kinds 0x1000/0x2000/0x3000 sprite/0x5000).
- **Root transforms**: `FUN_8001ACC0(component)` builds `0x80056A44`, which the entity draw `FUN_8001C2C0`
  (entity +0xC) and the animation-tree draw `FUN_80022070` (list `0x80058B78`, children through `FUN_800201BC`)
  place children by.
- **Fade**: the scene clock `0x8009F644`.

## Left to do

- Hub portal screen: a DMA register read inside the present refuses its in-betweens (issue 0048).
- Texture scroll copies are linked in the update (issue 0047).
- UI components drawn through +0x54 callbacks are not sampled (issue 0049).
- A paced real-time run on a quiet host (about 0.5 tick per vblank).
