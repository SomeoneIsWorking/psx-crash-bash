# 0036 — menu and UI 2D quads are drawn unkeyed

**Status:** resolved · **State:** S006

The census over Polar Push (1,394 frames) and Crashball (3,740) left 64,308 and 94,898 primitives
unkeyed, from the 2D leaves `0x8002992C`, `0x80029D28` and `0x8001A0D8` plus the static LOADING card
`0x80018B08`. Unkeyed primitives are drawn as the current frame, so a moving menu element stepped at 30 Hz.

Cause: the leaves take a shared image descriptor and a packed screen position, and nothing above them
was a producer. The callers that own each element are render components drawn through their +0x54
callbacks: text `0x8001C448` (→ `0x8001A82C` → string renderer `0x800243A0`, glyphs through
`0x8002992C`/`0x80029D28` with s2 one past the glyph byte), panel `0x8001C690` (→ `0x8001A6D4`: body,
then `0x8001A43C`'s four borders through `0x8001A0D8`) and quad `0x8001C7FC`. BOOT draws its HUD
(`0x800798A4`: icon records in s0, per-player digit records in s4) and menu page items (`0x800809A0`,
item record in s4, numbers through `0x800248A0`) outside any component.

Menu text is not built in a shared buffer: over both replays every string address `0x800243A0` drew
carried one text (MENU overlay `0x800B3xxx`, BOOT `0x800AExxx`, resident `0x8004Cxxx`). The `~` at
`0x800AEDAC` is drawn by two components per frame, so the component, not the string, is the object.

Fix: `game/render/ui_producer.{h,cpp}`: the three component callbacks are producers keyed by the
component's incarnation; `callNamed` on the leaves names each element from the call site it returns to
(glyph byte, panel part, digit place), and the BOOT sites open their owner's key (record, place).
Tests: `tests/test_ui_producer.cpp` (9 cases through the shipping overrides; 7 fail without them).

Evidence (2026-10-07, psxport 7e8d4fb1, 4:3, fps60 off):

| run | keyed before | keyed after | unkeyed left |
|---|---|---|---|
| Polar Push f0-1394 | 1,598,087 of 1,662,395 (96.13%) | 1,661,862 (99.97%) | 532, LOADING card |
| Crashball f0-3740 | 5,809,153 of 5,904,051 (98.39%) | 5,902,090 (99.97%) | 1,959, LOADING card |

Duplicate keys 0 in both runs; recordcheck mismatched=0 on all 1,395 and 3,741 presents. The Crashball
panel and its text slide 8 px per frame over f1240-f1254; at fps60 the panel's left border is at x=106
in real f1240, 104 in the in-between and 102 in real f1241.

Open: BOOT's direct `0x8001A6D4` calls and the projected text path `0x800246D0` are not keyed; neither
ran in the flow replays.
