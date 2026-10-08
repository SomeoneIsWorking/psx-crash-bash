# 0035 — the record path's HUD is not anchored at 16:9

**Status:** open · **State:** S019

At 16:9 the record path presents the guest's HUD where the guest draws it, centred on the 4:3
buffer, with the world filling the margins (Polar Push f1370, record path against the previous native
product at the same frame). The native path anchored the
portraits, lives digits and power bars to the widened edges; that layout code was deleted with the
native renderer.

Proper fix: a title producer that anchors each edge element by its guest owner and a host-only
offset of its own packets. The owners are known: portraits `0x80019EE8` (`jal 0x80029D28`), lives
digits `0x8007A010`/`0x80079F9C`, the string walk `0x80024688`/`0x80024640`, and Polar Push's power
bars, which are 0x3000-code objects drawn through `0x80019A60`. psxport `presentation.md` allows a HUD
element to move only when a title producer anchors it; the record path has no such producer yet.
