# 0038 — the menu model 0x800A28F4 really moves its faces that far

**Status:** resolved (no defect) · **State:** S006

BOOT layer-1 component `0x800A28F4` (incarnation 1) is drawn in every menu frame of the Polar Push
replay (f476-1000) with about 500 keyed faces; a few dozen step 40-230 px between consecutive frames.
Question: a fast-moving model, or faces named wrongly by `model_face_producer.cpp`?

Checked on real frames (4:3, fps60 off), pairing each face key in N-1 with N:

- No face changes UV or CLUT between frames (uvChanged 0 in every frame), and consecutive elements of
  one face list sit next to each other and move together, as one mesh does.
- Every step of 40 px or more belongs to a face whose centre is outside the 320x240 screen in both
  frames (124 of 124 over f600-f603): faces close to the camera, where a slow turn is a large step.
  On-screen faces move 2-10 px; the median step is 5.5-6.4 px.
- No other face of the model takes the old position of a large-step face (0 of 124), which a renamed
  element would show.

The guest itself moves those faces by those amounts; the naming is correct and nothing changed.
