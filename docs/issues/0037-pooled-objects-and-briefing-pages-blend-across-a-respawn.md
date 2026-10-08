# 0037 — a pooled object reused for a new thing blends across the respawn

**Status:** resolved · **State:** S006

Cuts are whole-frame and come from the process state, scene and menu screen (`FrameCut`). Inside one
scene the guest re-initialises render components in place, and the model producer keyed by component
address, so the first in-between after a reuse drew the old object halfway to the new one.

Cause: every copier of the render template `0x8005A830` starts a new life for the component it writes,
and `model_face_producer.cpp:modelDraw` keyed by a3 alone, so two lives of one slot shared a key. The
Polar Push briefing page flip (f1086→f1087) is `0x80095BEC` rebuilding layer 0 through `0x8001D3B0`;
its component `0x800A188C` (index 7 of the layer buffer `0x800A13F4`) changed from sprite `0x3060` to
`0x3066` and stepped 307 px.

Fix: `render::ComponentIncarnations` (`game/render/component_incarnation.{h,cpp}`) overrides the
complete copier set and bumps the component's generation; the producer's object is
`incarnationObject(a3, generation)`. Codemap, Producers.

The f1316→f1317 example was not a reuse. `0x801DD920` was re-initialised once, just before its first
draw at f1316, and the guest draws that one incarnation 83-116 px apart in f1316 and f1317 (the green
effect's first frame); it blends by identity like any other motion.
