# 0045 — boot fences 3 and 4 present late

**Status:** fixed · **State:** S016

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
`deliverDisplayFields` delivers `max(n, Timing::fieldsSinceDisplayBoundary())`, the whole fields the frame's guest work
already spanned (psxport `EmulatedTime::fieldsSinceBoundary`). Shown (`scratch/player-bugs-9/cb-boot-fix`,
`cb-2400-fix`): fence 4 is a 5-field frame shown 0.2 ms after its deadline (83 ms after fence 3, a load hold), fence 3
1.5 ms; 0 missed in the boot window; over 2400 frames from boot the only frame above its requested 2 fields is that one
(4213 `2/2`, 292 `2/1`, 1 `5/1`).
