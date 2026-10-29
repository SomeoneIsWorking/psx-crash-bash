# 0047 — the Polar Push aurora scrolls only on real presents

**Status:** fixed 2026-10-29 · **Owner:** `game/render/texture_scroll_producer.*`, with psxport's keyed VRAM copy
(`present/frame_composer.*`, `gpu/record_raster_setup.cpp:addCopy`, `record.frag`)

## Observed
Polar Push at fps60, 16:9, `replays/flow/polar-push-control.pad` then fences 1500-1630 (`PSXPORT_FPS60_DUMP_FROM=1500`,
unpaced, 1920x1080 sink): every in-between was composed, but the aurora behind the arena was identical to the
following real present in each one. The sky's texture moves once per logic frame, so at 60 Hz the aurora stepped at
30 Hz while the players moved at 60.

## Cause
The sky faces are static; the texture moves. `FUN_80028F28(scroller, owner)`, reached from `FUN_80029100` under the
update `0x80010394`, adds the scroller's speed to its 8.8 position, wraps it into the 64-row strip, and links two
`SetDrawMove` packets at the head of the table's last bucket: rows p..64 of the source (960,128) to the top of the
sky texture (992,0), and rows 0..p below them. Those copies were unkeyed, so an in-between replayed S's copies and
showed the next frame's scroll. Texels also read only the top-left pixel of their scaled block, so even a copy moved
by part of a row could not show.

## Fix
`FUN_80028F28` is a producer (object = the scroller); it saves the position, strip size, source and destination,
and the slot the copies were linked into. Its render emits both copies at the position blended the short way round
the strip, each placed by the fractional row (`CopyPlace`). psxport keys VRAM copies like primitives, places a
render's copies by slot, draws a placed copy's rect and source shift at scale, and samples a texel at the fragment's
position inside its scaled block, so the sky shows the strip half a row on.

## Check
`scratch/filed-defects-2/0047/`: the same replay, ires 0 (5x at this sink), dumps from fence 1500. Over the aurora
(`classify.py`, the pixels where the two real presents differ), the share of each in-between matching the next real
present is 0.40-0.75 (mean 0.55) across all 140 in-betweens (`after/aurora.txt`); before the fix every in-between
matched the next real. Real presents draw the guest's copies at whole rows.
