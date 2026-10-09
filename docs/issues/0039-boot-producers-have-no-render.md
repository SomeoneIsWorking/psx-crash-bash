# 0039 — BOOT's HUD and menu producers have no render; component bodies above the leaves are not re-run

**Status:** partial · **State:** S006

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
- fps60-on recordcheck mismatches that predate this change and are not attributed to a producer: Crashball
  1 present (seq 332, 10,422 pixels, a display-area start difference with 2 entries) and Tournament 11
  presents from seq 5131. The pushed revision gives identical lines. fps60 off is clean.
