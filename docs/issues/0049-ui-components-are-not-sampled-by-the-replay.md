# 0049 — UI components are not sampled by the replay

**Status:** resolved 2026-11-11 · **State:** S006 · **Owner:** `game/render/component_motion.*`

## Observed
`ComponentMotion` notes the components `0x80019A60` (models) and `FUN_8001ACC0` (root transforms) draw. Text, panel
and quad components draw through their +0x54 callbacks `0x8001C448`, `0x8001C690` and `0x8001C7FC` from fields of the
component and its +0x6C record, which no source samples, so a sliding panel or text moves only on real presents
(before the producers were deleted, the Crashball panel sliding over f1240-f1254 sat at x=106, 104, 102 across f1240, its
in-between and f1241).

## Cause
The three callbacks place from fields `ComponentMotion` already blends, but only for components it noted. Each one
(decompiled) draws under the component's transform (`FUN_8001965C`) unless flag `0x10000000` is set; then the text
(`FUN_8001A82C`) is placed at position x|y (+0x04, +0x08), the panel (`FUN_8001A6D4`) by the rect of its +0x6C
record (x|y +0x04, x1 +0x0A, y1 +0x0C) and the quad (`0x8001A0D8`) by the four corners of that record.

## Fix
The three callbacks are overridden to note their component with its kind; `ComponentMotion` blends the transform
for text and for panels and quads in 3D, and the rect or corners for panels and quads in screen space. A change of a
screen x's centring bits steps.

## Check
`replays/flow/crashball-control.pad` (with issue 0048 fixed): the "PRESS X BUTTON TO START" panel and its text
slide down over f1250-f1254 and each in-between lies between its neighbours (`scratch/replay/cb3`). Run end:
components 412 of 176078 unpaired.
