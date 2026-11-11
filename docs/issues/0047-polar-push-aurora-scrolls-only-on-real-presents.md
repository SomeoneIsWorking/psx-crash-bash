# 0047 — the Polar Push aurora scrolls only on real presents

**Status:** open again 2026-11-11 · **Owner:** `game/frame/draw_replay.*`

Reopened: the texture scroll producer was deleted with the other producers when the in-betweens became the
replayed draw section. `FUN_80028F28` links its copies in the update, which the replay does not run, so each
in-between carries the copies the real frame linked and the aurora steps at 30 Hz again.

## Proper fix
A motion source for the scroller's 8.8 position whose replay rewrites the two DR_MOVE packets the update linked,
placed by the fractional row the way the old render did (psxport `CopyPlace`), so the copies follow the blended
position. Until then the aurora is the real frame's.

## Observed
Polar Push at fps60, 16:9, `replays/flow/polar-push-control.pad` then fences 1500-1630 (`PSXPORT_FPS60_DUMP_FROM=1500`,
unpaced, the 1920x1080 sink of the first report): every in-between was composed, but the aurora behind the arena was identical to the
following real present in each one. The sky's texture moves once per logic frame, so at 60 Hz the aurora stepped at
30 Hz while the players moved at 60.

## Cause
The sky faces are static; the texture moves. `FUN_80028F28(scroller, owner)`, reached from `FUN_80029100` under the
update `0x80010394`, adds the scroller's speed to its 8.8 position, wraps it into the 64-row strip, and links two
`SetDrawMove` packets at the head of the table's last bucket: rows p..64 of the source (960,128) to the top of the
sky texture (992,0), and rows 0..p below them. Those copies were unkeyed, so an in-between replayed S's copies and
showed the next frame's scroll. Texels also read only the top-left pixel of their scaled block, so even a copy moved
by part of a row could not show.

## Fix (2026-10-29, deleted 2026-11-11)
`FUN_80028F28` is a producer (object = the scroller); it saves the position, strip size, source and destination,
and the slot the copies were linked into. Its render emits both copies at the position blended the short way round
the strip, each placed by the fractional row (`CopyPlace`). psxport keys VRAM copies like primitives, places a
render's copies by slot, draws a placed copy's rect and source shift at scale, and samples a texel at the fragment's
position inside its scaled block, so the sky shows the strip half a row on.

## Check
`scratch/filed-defects-2/0047/judge/run.sh`: the same replay at `ires=0` (8x, 5472x1872 dumps), `fps60=1`, sink
3840x2160, unpaced, dumps from fence 1500 to 1645. `judge/aurora.py` measures the aurora (the pixels where the two
real presents differ). All 145 in-betweens lie 0.41-0.57 of the way from the previous real present to the next
(mean 0.50), and only 2-10% of their aurora pixels equal the next real present (`judge/after/aurora.txt`). Before the
fix (1920x1080 sink, 5x), every in-between equalled the next real present. Real presents draw the guest's copies at
whole rows.
