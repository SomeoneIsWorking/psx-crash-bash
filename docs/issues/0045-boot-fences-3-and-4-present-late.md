# 0045 — boot fences 3 and 4 present late

**Status:** fixed against the port's guest clock (oracle not compared) · **State:** S016

## Observed
40 s paced hidden runs from boot (ires 0, fps60 1, 3840x2160 sink, `PSXPORT_DEBUG=presentseq`,
`scratch/player-bugs-5/paced40-seq{,2,3}/run.log`, load 0.8-1.5): the two longest present intervals are fences 3 and 4,
in the first 110 ms after the first present: 36.06/39.04, 35.10/34.69 and 34.57/41.41 ms; a run counted a miss only
when this boot interval crossed the miss threshold.

## Cause
`game/frame/frame_driver.cpp:FrameDriver::deliverDisplayFields` stood in for retail `VSync(n)` (`0x800320EC`, called by
DisplayFrame `0x800272AC` with the frame's field count) by delivering exactly n fields. Retail VSync waits until n
fields have passed since the last one; a frame whose work already ran past n fields gets no wait and spans those
fields. The boot frame at fence 4 runs the spanning calls `0x8008E5BC` (3 turns, 1.6M guest cycles) and `0x80010394`
(4 turns, 1.7M): 3.2M instructions, about 5.7 fields of guest time, executed in 31 ms of executor segments plus
overhead (43 ms host, `scratch/player-bugs-9/cb-boot-seg`), while the frame was paced as 2 fields (33.4 ms).

## Fix
`deliverDisplayFields` delivers psxport `Timing::vsyncFields(n)`, the one owner of the libetc rule. Decompiled
`0x800320EC` (`scratch/player-bugs-9/decomp`): VSync(n) waits until the field counter reaches its last return + n - 1,
then for the next VBlank, so a frame whose guest work ran past field k since the last return spans max(n, k + 1)
fields and ends on a VBlank. The first fix counted max(n, k), which ended such a frame off the field grid at the
guest's time.

Shown in the running product, against the port's own guest clock:
- psxport `scratch/player-bugs-9/v2-cb-grid` (600 frames unpaced, `PSXPORT_DEBUG=presentseq,fielddigest`): fence 4 is `6/1`,
  and every one of the 928 field deliveries lands on the field grid (emulated ticks a whole number of fields from the
  first).
- psxport `scratch/player-bugs-9/v2-audio-cb` (2400 frames paced, audio on through SDL's dummy sink, load 2.9-2.5): 0 missed.
  Fence 4 shows 0.25 ms after its deadline, 98 ms after fence 3; the player sees that 6-field hold at boot. Cadence
  4213 `2/2`, 287 `2/1`, 5 `3/1` (fences 94, 465, 476, 1401, 2316, frames whose guest work ran past 2 fields, which
  the first fix anchored off the grid as 2-field frames), 1 `6/1`.

## Delivering only the requested fields (2026-10-28, reverted)
A pass made `deliverDisplayFields` deliver the cadence word `0x8004E0E0` alone. The hold did not shrink: the pacer ends
an interval at the guest's time, so fence 4 still presented about 5.9 fields after fence 3, but the guest got 2 VBlank
callbacks and 2 SPU advances for it and every later field landed off the grid (`scratch/frame-loops/after-nopace`:
1525 of 1528 digest fields off the grid; fence 4 ticks 3397241 to 6741393). Reverted: an owned driver still counts
`Timing::vsyncFields(n)`, because the extension past n is the guest clock, not a VSync wait. Back on the grid
(`scratch/frame-loops/revert-nopace`, 600 frames unpaced): 268 `2/1`, 3 `3/1` (fences 94, 465, 476), 1 `6/1` (fence 4),
0 of 928 digest fields off the grid.

## Gaps
- Whether retail holds the boot frame for 6 fields, and whether its fences 94, 465, 476, 1401 and 2316 run 3 fields,
  depends on how closely the port's instruction count stands for console time. No console oracle frames were compared,
  so "fixed" means fixed against the port's guest clock, not shown to match retail.
