# 0051 — the Crashball leak proof differs at one guest field

**Status:** open · **State:** S006 · **Owner:** `game/frame/draw_replay.*`

## Observed
`replays/flow/crashball-control.pad` to step 2100, `fielddigest`, fps60 1 against 0 (2026-11-11): guest RAM is
equal at 2061 of 2062 shared ticks. Tick 400891044 (fps60 0 field 331, in the menus before the hub) differs in the
high halves of some words only, and the next field is equal again. Polar Push is equal at all 1700 shared fields.
The same field also differs between the native ClearOTagR body and the guest DMA path at fps60 0 (issue 0048), so
it is a point where a change in timing shows; whether the replay moves guest timing there is not known.

## Next
Dump RAM at that tick in both runs (`fielddigest` with a RAM dump at the tick) and name the words that differ and
their writer.
