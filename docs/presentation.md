# Presentation — Crash Bash

How Crash Bash's guest draws reach the record path and its 60 fps in-betweens: the producers that key
packets, the state they save and the renders that redraw it at `t`. The framework side is
`external/psxport/docs/presentation.md`; owners are in `docs/codemap.md`, hops in `docs/features.md`.

## Guest draw → presentation

Guest draw callbacks (`0x80021770`/`0x80021990` → standard `0x80019F1C`, or the alternate
`0x8001DD50`) → **`render::registerModelFaceProducer`'s `0x80019A60` producer** (object = a3's incarnation) → the guest
body writes its packets (`FUN_800193A8` per mesh, `FUN_80029D28` for 0x3000 sprite codes) and links
them into the ordering table, each store bound to the open key → `FUN_800193A8`'s namer rebinds each
face's header to `meshFaceElement(face list, index)` → `DisplayFrame` (`registerDisplayFrameOverride`)
hands the table to the guest's own DrawOTag and carries the draw base `0x800569D8` into the table walked next (`ordering_table_slots.cpp:followDrawBase`; the pause page is drawn in the update with the base the last present left) → `psxport:GpuDevice` executes it and `Gp0RecordTap` fills
the frame record with the keys → `FrameDriver::stepFrame` notes `FrameCut` and calls
`Game::presentation.commit` → `psxport:FramePresenter` seals the record, asks
`Runtime::sealedFrameIsCut`, and presents the keyed in-between and the record through
`RecordRasterizer`.

- **2D UI**: the component draw callbacks `0x8001C448` (text), `0x8001C690` (panel) and `0x8001C7FC`
  (quad) are producers like `0x80019A60` (object = the component's incarnation); the 2D leaves they reach
  (`0x8002992C`, `0x80029D28`, `0x8001A0D8`, via `0x800243A0`/`0x800248A0`) are overridden by
  `registerUiProducers`, whose `ui_producer.cpp:underName` names the element from the call site the leaf returns to.
- **Real field vs in-between**: with `fps60` on, each logic frame presents the in-between then the record.
  Every producer below is a state producer and renders its objects from saved state. A cut, an incomplete record or a record
  with no keys is drawn as is.
- **Widescreen**: `Runtime::guestWidescreenProjection` declares the player's aspect; the record
  canvas adds the margins and the guest's own primitives fill them. The HUD stays where the guest
  draws it (issue 0035).
- **A frame that presented nothing** (`deliveredFields_ == 0`) goes to
  `Game::presentation.commitUnpresented`, which seals no record.

## Producers

A producer is a native override whose packet stores carry the key `(producer, object, element, part)`
(psxport `presentation.md`, Interpolation).

| Guest address | Owner | Arg | Object | Element |
|---|---|---|---|---|
| 0x80019A60 | `registerModelFaceProducer` (`CrashBash::ModelDraw`) | A3 | the drawn render component's incarnation: `incarnationObject(a3, generation)` | 0 for a 0x3000 sprite code; a face otherwise |
| 0x8001C448 | `registerUiProducers` (`CrashBash::TextComponentDraw`) | A0 | the text component's incarnation | a glyph: `glyphElement(byte)`, its byte's main-RAM offset (string address + glyph index) |
| 0x8001C690 | `registerUiProducers` (`CrashBash::PanelComponentDraw`) | A0 | the panel component's incarnation | `PanelPart`: body, left, right, top, bottom |
| 0x8001C7FC | `registerUiProducers` (`CrashBash::QuadComponentDraw`) | A0 | the quad component's incarnation | 0 |
| 0x800798A4 (BOOT) | `underName` opens a `Scope` at its leaf call sites | s0 / s4 | the HUD icon record (s0, `0x800996E0`/`0x8009AD54` + 8i) or the player's digit record (s4, `0x8009AD74` + 8 * player) | 0 for an icon; `DigitPlace` for a digit |
| 0x800809A0 (BOOT) | `underName` opens a `Scope` at its leaf call sites | s4 | the 24-byte menu item record | a glyph, or the `DigitPlace` of `0x800248A0`'s three digits |

## State producers

`state_renders.cpp:registerStateRenders` fills `Session::stateProducers`, which
`Runtime::stateProducer` answers from.

The producers above save a state and render it (psxport `presentation.md`). The state is what the
guest's drawing body reads, and the render runs that same body on inputs a fraction `t` of the way between
two states; no packet is interpolated. The bodies are native C++ and the overrides run them too:

- `0x800193A8` (`CrashBash::MeshFaceEmit`) is `face_projection.cpp:projectFaces`: RTPS the strip's first
  two vertices, then RTPS + AVSZ3 + NCLIP per face, culling by the vertex flags and the depth limit. The
  override gives it the live GTE and writes each visible face into the guest's packet buffer
  (`GuestFaceSink`); a render gives it the saved GTE and reads each face's projected points (`HostFaceSink`).
- `0x80029D28`, `0x8002992C`, `0x8001A0D8` (sprite, Gouraud textured quad, Gouraud quad in 2D or through the
  GTE) are `leaf_packets.cpp:buildLeaf` over psxport's `EmitMemory`: guest pool memory for the override
  (`executeLeaf`), host words for a render. GTE control save/blend/guard, register numbers, state bytes and blends come from psxport (`gte_control.h`, `gte_registers.h`, `state_bytes.h`, `blend.h`).

`PacketCollector::Scope` (opened by the component override) collects what the bodies ran; `save` marks the
scope when the guest body returns and `PacketCollector::commit`, called by `DisplayFrame` right before the
guest's DrawOTag, saves each marked scope. A call whose packet is no longer linked into the table in use
(`ordering_table_slots.cpp:linkedSlot`) is dropped, so a packet another draw relinked or overwrote after its
own scope ended, and a culled face's stale link into the other table, are not saved. A scope that ran one
mesh body saves a face state; one that ran only leaves saves a leaf state; anything else saves nothing and
stays as the guest drew it. The collector keeps its scopes, pending scopes and mesh calls across frames, and face
saves and renders work in the session's `faceScratch`, so a steady stream of mesh draws allocates nothing; the
leaf path still allocates per draw (psxport issue 0173).

- Face state (`model_state.cpp`): the GTE control registers, the face list, the vertex words the guest
  animated into its scratch buffer, the depth bias and limit, the table and bucket base, and the saved
  packet words (colours, texture words, draw modes) of each face the body linked. The render blends the
  rotation and translation registers and every vertex coordinate between two states with the same face list,
  projects again under `psx::present::GteGuard` on the core's bound GTE, and emits each face the body links at the bucket the body gives it,
  last linked first.
- Leaf state (`leaf_state.cpp`): each leaf call's arguments and what its body reads besides them (display
  scale, fade, bias and limit, origin, the texture record, a shaded quad's corners and colours, the GTE
  transform of a projected one). The render blends sprite and quad positions, flat corners and origin, or
  projected corners and transform, between two states with the same number of calls of the same shapes, and
  runs `buildLeaf` on each.
- Component bodies (`text_bodies.cpp`, `panel_bodies.cpp`, `component_body.cpp`; the BOOT producers and the
  component producers save these): `0x800243A0` and `0x800248A0` (string and number layout) and `0x8001A6D4`
  and `0x8001A43C` (panel body and border) are native. Each calls its leaves through a `LeafPort`: the
  override's `GuestLeafPort` dispatches the leaf overrides (so the override differential sees the same
  nested calls as the guest), a render's `HostLeafPort` emits host packets. A scope saves each body's own
  fields (position, size, scale, string bytes, record words, tint) in `leaf_state.cpp` items, and the render
  blends those fields at `t` and re-runs the body (`component_body.cpp:blendBody`/`runBody`), so a moved
  component lays out its glyphs and parts at `t`. Leaf calls outside a body (BOOT's HUD icons) stay
  leaf items. The string body writes the tint colours back to the caller's stack slots as the guest does.

An unpaired or differently shaped state is drawn as saved. At `t = 1` the render draws exactly what the guest
linked.

Equality with the original bodies is proved by `PSXPORT_OVERRIDE_DIFF` over the Polar Push replay (all four
addresses: memory, GTE and result registers; the native bodies end on the same `v0`/`v1` the guest leaves,
the list terminator for the mesh, the display scale for the sprites, the linked header for the shaded quad).

Rotation is blended element by element, as a render of a turning model is only as good as that approximation over one
tick.

UI element namer: the overrides `registerUiProducers` installs at `0x8002992C`, `0x80029D28`, `0x8001A0D8`,
`0x800243A0` and `0x800248A0` run under `ui_producer.cpp:underName`, which reads the return address. A glyph site of `0x800243A0` (s2 is one past
the glyph's byte), a digit site of `0x800248A0`, or a panel/quad site of `0x8001A0D8` names an element of
the open object; with no scope open it names nothing. A BOOT site (only while BOOT's generation holds
that address, `runtime::imageHolds`) opens the owner's whole key. Menu strings are constant overlay or
resident data, one text per address over both flow replays, so the byte is identity; the component
keeps two components showing one string apart.

Element namer: `0x800193A8` (`CrashBash::MeshFaceEmit`) names face i of the mesh whose face list it
was given `meshFaceElement(faceList, i)`, the list's main-RAM offset above an 11-bit index. Face i's
packet is at `buffer + i * 40` whether or not it was culled, and `FUN_80019094` pops that buffer per
(model frame, parity) in call order, so the buffer is not identity and the object is.

Incarnations: the guest re-initialises render components in place (a briefing page flip rebuilds its
layer's components through `0x8001D3B0`; pools hand freed slots out again). Every copy of the render
template `0x8005A830` into a component starts a new life, and its only copiers are overridden by
`registerComponentIncarnationOwners`, which bumps that component's generation after the original runs:
pool allocators `0x80018F3C`/`0x8001D48C`, buffer fill `0x8001D3B0`, placed arrays
`0x8001D6B4`/`0x8001E0A8`, animation node creators `0x80021A1C`/`0x80021798`/`0x8002128C`, and BOOT's
`0x8007EB68` (static `0x8009AD94`) and `0x8009440C` (scratch `0x800A0C74`). The set comes from
scanning every disc image for `addiu …, 0xA830` and for calls to those functions.

`0x8009440C` refills the one scratch component for each entry of `0x800A0C10` (two a frame, mesh
codes `0x200B`/`0x200C`) and draws it, so it is a native port (`component_incarnation.cpp`) whose
incarnation is the entry index (`ComponentIncarnations::refill`): each entry is its own object and keeps
its key across frames. Counting one life per call put both entries under one key, which the frame
states read as ambiguous and left without a render.

Not producers, with the reason: the LOADING card `0x80018B08` is static. Not keyed yet: BOOT's direct
panel calls (`0x8001A6D4` from `0x80082608`/`0x8008267C`, outside any component) and the projected text
path `0x800246D0` of `0x8001A82C`; neither ran in the flow replays.
