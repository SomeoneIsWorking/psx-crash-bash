# 0040 — BOOT's pause page items were linked into a table that was never walked again

**Status:** resolved (port defect) · **State:** S006

Reproduce: Polar Push replay to f1400, `pause open`, run 60 frames (also Crashball f3450, Battle Crate Crush f9240).
Expected: the open pause page shows PAUSED and its five items (CONTINUE, SHOW RULES, OPTIONS, CHANGE ARENA, QUIT GAME).
Observed: the hollow panel and the scoreboard strip, no text.

Cause: `game/frame/display_frame.cpp:displayFrameOwned` walks the ordering table with `DrawOTag` and returns while the guest's
draw base `0x800569D8` still names the walked table. The pause page is drawn from the update (`FUN_80092EDC` ->
`FUN_8007F314` -> `FUN_80080FBC` -> `0x800809A0`), before the frame's present (`FUN_80018B08`, the only writer of
`0x800569D8`) re-bases it, so the leaves link the glyph packets (bucket 0) into the walked table, which the next
`ClearOTagR` wipes unwalked. Pool (`0x8005B68C`) and link base differed on every item glyph (1,679 of 1,679 stale links in
the probe run were the title and five items; none before the pause opened).

What retail does: `DrawOTag` is a DMA that is still walking the table while the next update runs, and bucket 0 is the last
bucket it reaches, so packets the update appends there are drawn. The page text is visible on retail; the walk here is
instant. The earlier notes were wrong that items 3 to 5 lie below the screen: the leaves halve y (`0x8002992C`), so the
rows sit at screen y 91..151 inside the panel.

Fix: `render::followDrawBase` (`game/render/ordering_table_slots.cpp`) moves the base to the same slice of the table walked
next at the swap in `displayFrameOwned`, so the page is drawn with the frame after it (retail draws it one display earlier).
Test: `tests/test_draw_base.cpp`. Shown: Polar Push f1460 with the page and items; recordcheck at 4:3 `aspect=0 ires=1` is
0 mismatched on 1,741 of 1,741 presents (fps60 off) and 1,770 of 1,770 (on); consecutive presents of the open and of a
`pause down` show the items in every present, the in-between of a logic frame equal to its real frame in the item area
(the tint fade steps at the guest's 30 Hz), the strip sliding up over the first presents.
