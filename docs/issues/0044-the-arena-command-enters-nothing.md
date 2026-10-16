# 0044 — the `arena` command enters nothing

**Status:** open · **State:** S016

## Observed
Real-window play check (2026-10-16): `arena 0` armed at boot logs `committed at f6` and `entered at f7` with menu
screen `0x00000000` and guest arena selection `0x00000000`; the run stays in the attract demo (Pogo Painter,
`DEMO`). Armed once `arena list` answers (f33), the same. `arena 2` walks menu screens `0x800B8E28`, `0x800B8E50`,
`0x800B8E3C`, `0x800B9DF4`, `0x800B8E64`, `0x800BCC04`, never matches SELECT BATTLE TYPE and is abandoned at
f2433. The play check used `replays/flow/polar-push-control.pad` instead.

## Traced
`game/debug/dev_arena.cpp:DevArena::applyArmed` runs the mode step while no menu screen is current (screen 0) and
then, in `Stage::AwaitArenaScreen`, commits as soon as `guest::kArenaSelection` equals the request without having
seen SELECT BATTLE TYPE; the selection word is 0 before any menu runs, so arena 0 is "entered" at once. Why the
flow for arena 2 never shows a screen with SELECT BATTLE TYPE's enter/update pair is not traced.

## Gap
Rebuild the request in the Tomba! 2 `warp` pattern: arm, wait for the menu to own a screen, apply through the
guest's own transition owner, and require the arena screen before a commit; with a unit test through
`DevArena::applyArmed`.
