# 0048 — the hub portal screen's in-betweens are refused

**Status:** open · **State:** S006 · **Owner:** `game/frame/draw_section.*`

## Observed
`replays/flow/crashball-control.pad`, hidden, ires 0, fps60 1, sink 3840x2160, unpaced: 781 intervals refused,
f~1250-2021, all on the warp-room portal screen ("PRESS X BUTTON TO START" over the purple sky). Each logs
`the redraw of 0x80092B7C reached device-read 32-bit @0x1F8010F0`; the interval shows the real frame, so the screen
runs at 30 Hz. No refusal in the Polar Push replay or in the Crashball match.

## Cause (partial)
Something under BOOT's root present `0x80092B7C` on that screen reads DPCR (`0x1F8010F0`), the DMA control
register: a GPU DMA (LoadImage or similar) inside the draw. The caller is not yet identified.

## Next
Find the guest function on that screen's present path that starts the DMA (Ghidra refs to the libgpu DMA entry,
then its callers under `0x80092E94`/`0x8001E898(0x8009F8A4)`). If it uploads animated texture data, its upload is
logic state and belongs outside the replayed section, or the section stops short of it.
