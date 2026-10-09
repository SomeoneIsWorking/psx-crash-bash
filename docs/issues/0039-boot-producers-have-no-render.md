# 0039 — BOOT's HUD and menu producers have no render; component bodies above the leaves are not re-run

**Status:** partial (BOOT menu page unreached) · **State:** S006

Done: `0x800798A4` and `0x800809A0` open a `PacketCollector::Scope` at their call sites and have
`StateRender` installed; `0x800243A0`, `0x800248A0`, `0x8001A6D4` and `0x8001A43C` are native bodies
(`text_bodies`, `panel_bodies`) that the overrides run and the renders re-run on saved component fields.
Unit tests (`tests/test_ui_producer.cpp`) cover t=1 at 4:3 against the guest walk with scrambled memory and
t=0.5 against a frame drawn halfway. The override differential matches all four bodies on Polar Push, and
Polar Push recordcheck at 4:3 is 0 mismatched with fps60 off and on. `tools/source_policy.py` now expects 37
override registrations (was 35) and 11 original calls (was 12).

Remaining:
- No replay reaches the BOOT menu page (`0x800809A0` renders once in total), and BOOT HUD leaf positions
  never move between presents, so a menu item or HUD icon moving between consecutive presents has not been
  seen. A route or a title debug option that opens the BOOT menu is needed to close this.

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
