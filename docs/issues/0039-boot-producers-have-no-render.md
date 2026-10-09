# 0039 — BOOT's HUD and menu producers have no render; component bodies above the leaves are not re-run

**Status:** resolved (HUD static, menu page items fixed in issue 0040) · **State:** S006

Done: `0x800798A4` and `0x800809A0` open a `PacketCollector::Scope` at their call sites and have
`StateRender` installed; `0x800243A0`, `0x800248A0`, `0x8001A6D4` and `0x8001A43C` are native bodies
(`text_bodies`, `panel_bodies`) that the overrides run and the renders re-run on saved component fields.
Unit tests (`tests/test_ui_producer.cpp`) cover t=1 at 4:3 against the guest walk with scrambled memory and
t=0.5 against a frame drawn halfway. The override differential matches all four bodies on Polar Push, and
Polar Push recordcheck at 4:3 is 0 mismatched with fps60 off and on. `tools/source_policy.py` now expects 37
override registrations (was 35) and 11 original calls (was 12).

Reached with the title `pause` command (`game/debug/dev_pause.{h,cpp}`; codemap, Debug control channel). In a live
match it presses Start through the pad and the guest's own dispatcher `FUN_8007F314` opens the pause page (state word
`0x8005A624` bit 0); `pause up|down|left|right|confirm` then walks its pages. The BOOT menu page `0x800809A0` is called
from `FUN_80080FBC` once per frame from there, with the table of the open page.

Findings (Polar Push `polar-push-control.pad` to f1400, Crashball `crashball-control.pad` to f3600, Battle Crate Crush
and Tournament Crate Crush to f9100, then `pause open`, two `down`, `confirm`, `down`, `right`; 4:3, `aspect=0 ires=1`):
- Recordcheck mismatched presents, fps60 off / on: Polar Push 0 of 1,741 / 0 of 1,770, Crashball 0 of 3,941 / 0 of
  3,941, Battle Crate Crush 0 of 9,441 / 0 of 9,471, Tournament 0 of 9,441 / 0 of 9,470.
- BOOT HUD `0x800798A4` renders (temporary probe, removed): off t=1 only; on t=0.5 and t=1 each logic frame, e.g.
  Polar Push 2,864 at t=0.5 and 2,868 at t=1; Battle 29,021 and 29,453. The HUD's saved states differ between consecutive
  frames only in the ordering-table base (the two tables alternate), a word at +0x18 (it counts down from 0x1000 to 0 in six steps) and the
  texture record (animated sprite frames). No leaf position changes in any of the four replays: the HUD is static
  (record placement is `DAT_800996D4`/`DAT_80099700` origins plus constant record offsets, written by `FUN_8007EAC0`
  and `FUN_8007BE84`; the only writer of a digit's y offset, `FUN_8007B330`, slides it by -8 per frame only in the
  result sequence of a mode that sets `0x80099174 & 0x1000`; the word reads `0x80028013` in Polar Push (f1400),
  `0x80028202` in Crashball (f3600) and `0x8002800B` in Battle Crate Crush (f9100), none with that bit, so the
  sliding digits are not reached by any retained replay).
- The pause page's scoreboard strip is a component panel (not BOOT's HUD) and does move: it slides up over the first
  presents after `pause open`; the consecutive presents show no tearing, doubling or vanishing.
- Menu page `0x800809A0` renders: 1 in Polar Push, Crashball and Tournament (in Polar Push the fading GAME OPTIONS page at f1000), 0
  in Battle. Its pause-page items were built every frame but linked into a table that was not walked again (issue 0040, fixed).

A psxport defect hid every in-between present on these replays and is fixed (`entryWritesRect`): before it, fps60-on
runs presented 0 in-betweens (`final=0` count 0); after it, 1,457 on Polar Push.

Resolved (fps60-on recordcheck at 4:3, `aspect=0 ires=1`, replays `crashball-control.pad` and
`tournament-crate-crush-control.pad`): both mismatch sets were psxport presentation, not a Crash Bash producer.
- Tournament, 11 presents from seq 5131 (all on the y=0 buffer): the glow object `0x21dcac8` draws gouraud additive
  faces from texture page (384,0), which lies in that buffer. A face whose texels overlap pixels the frame already wrote
  is recorded by `Gp0RecordTap::settleLast` as a keyless `VramUpload` of its pixels (3-7 of its 21 faces per frame),
  so `composeFrame` replaced only the 14-18 remaining keyed faces with the render of all 21 and the baked faces drew
  twice, additively. Fix: `VramUpload::key` carries the primitive's key and `composeFrame` leaves an object with a
  keyed upload as drawn. Test: `test_an_object_with_a_baked_upload_keeps_its_entries` (composer) and
  `test_a_feedback_upload_keeps_the_primitives_key` (tap).
- Crashball, seq 332: the first frame of the scene change at f331 clears VRAM with a black 511x511 sprite under a
  whole-VRAM draw area. That is not a scene draw by the canvas rule, so the present composed record 331 over a
  buffer the clear had already overwritten (10,422 black pixels, stp bit 0x8000 against 0x0000). Fix:
  `FramePresenter::shownRecord` presents the device's picture when a record after the shown one wrote the displayed
  buffer. Test: `test_a_later_record_over_the_displayed_buffer_is_not_composed_over`.

Recordcheck after the fix, mismatched presents: Crashball 0 of 3,746 on and off, Tournament 0 of 9,246 on and off,
Polar Push 0 of 1,401 on and off.
