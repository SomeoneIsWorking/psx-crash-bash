# 0049 — UI components are not sampled by the replay

**Status:** open · **State:** S006 · **Owner:** `game/render/component_motion.*`

## Observed
`ComponentMotion` notes the components `0x80019A60` (models) and `FUN_8001ACC0` (root transforms) draw. Text, panel
and quad components draw through their +0x54 callbacks `0x8001C448`, `0x8001C690` and `0x8001C7FC` from fields of the
component and its +0x6C record, which no source samples, so a sliding panel or text moves only on real presents
(before the producers were deleted, the Crashball panel sliding over f1240-f1254 sat at x=106, 104, 102 across f1240, its
in-between and f1241).

## Next
RE which fields the three callbacks place from (component +0x04.. or the +0x6C record), note those components from
their callbacks, and blend those fields in `ComponentMotion` or a source of their own.
