# 0040 — BOOT's pause page items are built every frame but no primitive reaches the GPU

**Status:** open · **State:** S006

Reproduce: Polar Push replay to f1400, `pause open`, run 60 frames (also Crashball f3450, Battle Crate Crush f9240).
Expected: the open pause page shows its items (`0x800598DC`: five centred strings at y 70..190 plus the `0x70` page
offset). Observed: the screen shows the hollow panel and the scoreboard strip, no item text.

Evidence:
- `0x800809A0` runs every frame with the page table (`DAT_8009BD18 = 0x70` argument), the box item executes
  (`DAT_8009BCE6` changes to `0x66`), and the five string items call `0x800243A0` at y 182, 212, 242, 272, 302 with
  `x = 0x8000` (centred); the glyph leaves get x 240..400 and table base `0x8005B790`/`0x8005F79C`, bucket 0.
- `PSXPORT_OVERRIDE_DIFF` over the pause frames: `0x800243A0` 2,980, `0x8002992C` 2,067, `0x80029D28` 5,472 sampled
  calls, 0 mismatches, so the native bodies equal the guest's.
- The producer census attributes 3 primitives to `0x800809A0` for the whole run (f1000, the GAME OPTIONS page) and none
  during the pause; no unkeyed primitive either. `recordcheck` is 0 mismatched, so the device's VRAM lacks the text too.
- Items 3 to 5 lie below the 234 displayed rows; items 1 and 2 (182, 212) lie under the scoreboard strip's rows.

Cause: not established. Next: trace where the glyph packets linked at bucket 0 of `0x8005B790` go (is that table walked
by `DrawOTag` in the frame the pause draws in), starting from `FUN_80080FBC`'s caller `FUN_8007F314`.
