# 0050 — the replay has no cut below scene level, and poses do not cross-blend

**Status:** open · **State:** S006 · **Owner:** `game/frame/frame_cut.*`, `game/render/component_motion.*`

## Observed (by reading, not yet seen in frames)
- `FrameCut` holds an interval only when the process state, the mode scene or the menu screen changes. A camera
  or a character repositioned inside one scene (a round restart, a respawn) keeps its address, incarnation and
  model, so the first in-between blends across the jump.
- A pose blends only within one animation. An animation switch is drawn as the new pose, not cross-blended, and a
  keyframe step that wraps to the start of a looping animation is drawn as now.

## Next
- RE the arena round reset and the respawn paths for the guest's own declaration (a camera reset or a placement
  call) and hold the interval, or that object, from there.
- A wrap needs the animation's keyframe count (the record `0x80019A60` reads at +0x08) and what the weighted draw
  reads past the last keyframe; a cross-blend needs both poses drawn at once, which the guest's weighted draw
  cannot do.
