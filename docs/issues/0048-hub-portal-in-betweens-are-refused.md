# 0048 — the hub portal screen's in-betweens are refused

**Status:** resolved 2026-11-11 · **State:** S006 · **Owner:** `game/frame/gpu_timeout.cpp`

## Observed
`replays/flow/crashball-control.pad`, hidden, ires 0, fps60 1, sink 3840x2160, unpaced: 781 intervals refused,
f~1250-2021, all on the warp-room portal screen ("PRESS X BUTTON TO START" over the purple sky). Each logs
`the redraw of 0x80092B7C reached device-read 32-bit @0x1F8010F0`; the interval shows the real frame, so the screen
runs at 30 Hz. No refusal in the Polar Push replay or in the Crashball match.

## Cause
The portal screen's present clears an ordering table of its own with ClearOTagR (`0x8002F254`), whose GPU driver
body `FUN_80030220` runs the OTC DMA: `DPCR |= 0x08000000`, MADR/BCR/CHCR, then polls CHCR (the refusal stopped at
pc `0x8003126C`, ra `0x80030298`). The DMA writes only RAM, a link per bucket, and completes at once here.

## Fix
`gpu_timeout.cpp:clearTableOwned` owns `FUN_80030220` for real frames and replays alike: it writes the table the
DMA writes (psxport `ordering_table.h:reverseClearedLink`, the formula the OTC device uses), arms the timeout as
the other synchronous GPU services do and returns the count. No device register is touched.

## Check
`replays/flow/crashball-control.pad`, hidden, ires 0, fps60 1, sink 3840x2160, unpaced, to step 2100: 2096
intervals replayed, 0 refused (781 before). Presents f1250-f1253 show the "PRESS X BUTTON TO START" panel
sliding down a step per present, in-betweens between their neighbours; the portal frame above it steps 3 px per
present (`scratch/replay/cb3`).

Differential against the guest DMA path (the same replay, fps60 0, `fielddigest`, a build without the override):
Polar Push RAM equal at 1696 of 1700 fields, Crashball at 2090 of 2100. The differing fields are isolated and
equal again by the next field (Polar Push f1022-f1026); repeated runs of one build are identical, so the
difference is the override's. Not yet traced; the likely cause is the guest instruction count the body accounts
(24) against the DMA path's, which moves where the next interrupt lands.
