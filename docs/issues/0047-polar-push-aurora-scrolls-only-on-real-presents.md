# 0047 — the Polar Push aurora scrolls only on real presents

**Status:** open · **State:** S006

## Observed
Polar Push at fps60, 16:9, `replays/flow/polar-push-control.pad` then fences 1500-1630 (`PSXPORT_FPS60_DUMP_FROM=1500`,
unpaced, 1920x1080 sink): every in-between is composed and lies between its neighbours, but the aurora behind the
arena is identical to the following real present in every in-between (8x8 tile check over the upper middle of the
picture: in-between to next real mean difference 0.0-0.8, previous real to next 4-10). The sky's texture moves once
per logic frame and lands on the in-between, so at 60 Hz the aurora steps at 30 Hz while the players move at 60.

## Traced
The sky faces are static: their packets (e.g. `0x80194100`, gouraud textured triangles, tpage `0x000F`, u 0x80-0x9F,
v 0x23-0x3F, so VRAM x 992-999) hold the same positions and texture words from frame to frame. The motion is the
texture: each frame the update `0x80010394` links two GP0 `0x80` VRAM copies at the head of the table that rotate a
64-row strip into the sky's texels by one row, `src(960,128)->dst(992,40-n) 16x(24+n)` and
`src(960,152+n)->dst(992,0) 16x(40-n)`. The in-between samples VRAM as the newest record left it, so it shows the
next frame's scroll. The leaf function under `0x80010394` that writes the copies is not named yet.

## Proper fix
Key the two copies by the guest function that issues them (a title producer over that leaf) and give the record path
a state render for a VRAM-to-VRAM copy whose source row is blended at `t`, which the scaled VRAM can show at a
fraction of a texel row. psxport has no keyed VRAM copy today, so this is a framework addition plus the title producer.

## Left out
The README's interpolation clip uses Crashball instead of Polar Push until this is fixed.
