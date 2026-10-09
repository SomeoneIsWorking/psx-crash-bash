# 0039 — BOOT's HUD and menu producers have no render; component bodies above the leaves are not re-run

**Status:** open · **State:** S006

Model faces, text, panels and quads are state producers (codemap, State producers). Two keyed producers are
not: `0x800798A4` (HUD icons and digits) and `0x800809A0` (menu items) are opened at their leaf call sites
by `ui_producer.cpp:callNamed` and keep only their key. They are still blended by psxport's `keyedBlend`,
so they lose their in-between the moment that composer is deleted.

Proper fix: give both a scope through `PacketCollector::Scope` at their call sites and install
`StateRender` (`state_renders.cpp:registerStateRenders`) on them; their leaves already run the native bodies
and note their calls.

Also open: the text, panel and quad renders blend the arguments of the leaf calls, not the component fields
the guest bodies above them (`0x800243A0`, `0x800248A0`, `0x8001A6D4`, `0x8001A43C`) read. Those bodies are
guest code and a render cannot call into the guest. Porting them to native bodies over saved fields would
let a render move the component's position and size and lay out its glyphs and parts at `t`.

Recordcheck, which compares the composed record with the device, cannot tell a render from `keyedBlend`
(its `composed=` flag counts both); the override differential, the unit tests and shots of consecutive
presents separate them.
