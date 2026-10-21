# 0043 — front-end load holds show the older buffer every other present

**Status:** open · **State:** S006

## Observed
Real-window play check (2026-10-16, `replays/flow/polar-push-control.pad` then 75 s of the live Polar Push match,
fps60 on, 16:9): `records: presents=3646 in_betweens=3362 without_in_between=284 repeats=0 backwards=31`. All 31
steps back and all 284 presents without an in-between are before the replay completes (fence <= 1024); the live
match adds none. The steps back come in three runs of about ten: fences 344-362 and 442-460 (the EUROCOM logo hold,
replay phase `0xb9524000000`) and 1003-1023 (the load into the match, phase `0xa00dc000000`). In each run the
shown record alternates between the last two drawing records (341, 342, 341, ...), every pass's record drawing
nothing. The two pictures differ (`PSXPORT_FPS60_DUMP_FROM=336`: the real presents alternate between two hashes).
Of the 284 presents without an in-between, 232 are passes that drew nothing with no drawing record for the
displayed buffer (the device picture), 29+2 are the steps back, 21 are the first drawing records after them.

## Traced
`game/frame/display_frame.cpp:displayFrameOwned`, the native `DisplayFrame`, flips the display origin and the
ordering table on every call, as the guest function does. During these holds it is called each pass with an empty
ordering table, so the displayed buffer alternates between the two buffers last drawn, and
`FramePresenter::shownRecord` (psxport) returns the record that drew whichever one is displayed: the presenter
shows what the device displays.

## Gap
Not established whether the retail loop calls `DisplayFrame` once per pass with nothing drawn during these holds
(then the flicker between the two buffers is the console's and the §6 "never backwards" bar fails on faithful
output, as in Tomba! 2 issue 0038) or whether the port's frame driver runs passes the retail game does not.
Compare the pass sequence at the EUROCOM logo hold against an oracle run.

## Still reproduces (2026-10-21)
Spyro 1 judder pass 3 ran every title 40 s from boot at real speed (hidden window, no `PSXPORT_NOPACE`) on live
psxport, three times during the pass and once after its review fixes: each Crash Bash run shows records backwards
20 and 272 presents without an in-between, at load 13.3, 19.3, 15.1 and 5.8. The psxport changes of that pass
(sub-pixel places, composer storage) did not move it. The gap above stands.
