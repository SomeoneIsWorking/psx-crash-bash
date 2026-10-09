# Codemap

This map records responsibility and placement only. Intent lives in `docs/project-goals.md`, factual
capability state in `docs/project-state.md`, and evidence order in `docs/re-frontier.md`.

## The rule

Every first-party C++ file lives under `game/<concept>/`, and the directory name IS the concept.
A new owner goes in the directory of the thing it owns, never in a neighbour's, and never in a new
"common" directory. The framework (`external/psxport`, the workspace's live checkout) owns everything
reusable; this title owns Crash Bash's game-flow interpretation of it.

## Directories

| Directory | Namespace | Files | One-line responsibility |
| --- | --- | --- | --- |
| `game/entry/` | — (process entry) | `player_entry.cpp` | `main`: read the executable, install `TitleAdapter`, bind the per-Core devices, enter psxport's native boot/frame loop. Nothing else; it composes, it does not implement. |
| `game/execution/` | `crashbash`, `crashbash::runtime` | `title_adapter.{h,cpp}`, `guest_execution.{h,cpp}`, `image_identity_state.{h,cpp}`, `native_owner_set.{h,cpp}`, `image_content_identity.h`, `executable_identity.h.in`, `module_image_identity.h.in` | The typed psxport boundary: `TitleAdapter` (the `GameRuntime` the framework drives), `GuestExecution` (per-Core title context: override registrations, authenticated image bindings, original calls), `ImageIdentityState` (those bindings in a save state), `NativeOwnerSet` (the complete installation list), and the CMake-derived identity metadata. |
| `game/boot/` | `crashbash` | `crashbash_boot.{h,cpp}`, `boot_object_callbacks.{h,cpp}`, `boot_logo_skip.{h,cpp}`, `memory_card_startup.{h,cpp}`, `resident_image.{h,cpp}`, `boot_image.{h,cpp}`, `menu_image.{h,cpp}`, `authenticated_module_image.{h,cpp}`, `nested_module_image.{h,cpp}` | Everything that runs once at power-on: the finite retail boot prefix, the BOOT overlay's object callbacks and logo controller, BIOS memory-card startup, and the authenticated publication of the resident executable, BOOT and MENU (plus `NestedModuleImage` for the five overlays that reuse MENU's load slot). |
| `game/disc/` | `crashbash` | `cd_startup.{h,cpp}`, `cd_license_startup.{h,cpp}`, `cd_file_read.{h,cpp}`, `loading_card_skip.{h,cpp}` | The disc: the measured SCUS_945.70 startup handshake, licence startup, the synchronous file read that also retires authenticated images per sector write, the load-pump drain, and the retirement of the loading-card presentation at its two measured call sites. |
| `game/frame/` | `crashbash` | `crashbash_frame_driver.{h,cpp}`, `frame_cut.{h,cpp}`, `display_frame.{h,cpp}`, `gpu_timeout.{h,cpp}`, `measured_guest_call.h` | The frame turn: `CrashBashFrameDriver` owns one measured input→audio→simulation→render→present order, the single presentation commit, the Polar Push `polarContactCensus`, the render `componentIncarnations`, and `FrameCut` (whether the record it seals is a cut, from the guest's process state, scene and menu screen); `registerDisplayFrameOverride` is the guest's own frame call it is invoked from; `registerGpuTimeoutOverrides` is the synchronous GPU timeout/transfer; `measuredGuestCall` is the one way a native owner calls a guest body. |
| `game/input/` | `crashbash`, `crashbash::input` | `crashbash_input_phase.{h,cpp}`, `crashbash_touch_controls.{h,cpp}` | `InputPhase` (which screen is taking input — the key a `.pad` recording is keyed on) and `CrashBashTouchControls` (the authored landscape touch overlay for Android). |
| `game/render/` | `crashbash::render` | `model_face_producer.{h,cpp}`, `ui_producer.{h,cpp}`, `face_projection.{h,cpp}`, `leaf_packets.{h,cpp}`, `model_state.{h,cpp}`, `leaf_state.{h,cpp}`, `packet_collector.{h,cpp}`, `state_renders.{h,cpp}`, `draw_globals.{h,cpp}`, `packet_decode.{h,cpp}`, `ordering_table_slots.{h,cpp}`, `component_incarnation.{h,cpp}` | Producers: native overrides that key the guest's own packets for 60 fps interpolation (Producers below: models, and the 2D UI of text, panels, quads and BOOT's HUD), the native bodies of `0x800193A8` and the three 2D leaves with the state and render of the four component producers (State producers below), the ordering-table bucket of a packet, and `ComponentIncarnations`, which counts each re-initialisation of a render component so a reused slot keys as a new object. Nothing here draws; the picture is the guest's GP0 output on psxport's record path. |
| `game/gameplay/` | `crashbash::polar` | `polar_push_contact.{h,cpp}` | The DAT22510 native owner of Polar Push contact traversal, motion and effects. Frame and presentation ownership, and the per-Core `ContactCensus` tally, stay in `CrashBashFrameDriver`. |
| `game/debug/` | `crashbash::debug` | `dev_arena.{h,cpp}` | The developer `arena` control-channel command. It ARMS a request; the frame driver advances it through the game's own menu flow. No player input reaches it. |
| `game/title/` | `crashbash::guest` | `crashbash_guest.h` | The recovered SCUS_945.70 guest facts: addresses, scene/scene-record layouts, arena table and menu flow. Facts only; no behavior. |
| `packaging/linux/` | `crashbash::appimage` | `launcher_main.cpp`, `install_media.{h,cpp}`, `user_paths.{h,cpp}` | The AppImage launcher's first-run media setup. `user_paths.cpp` is this product's only environment-read boundary. |
| `platform/android/` | — (Java) | `app/src/main/java/.../CrashBashActivity.java`, `CrashBashMediaImport.java` | Android Activity and SAF media policy. |
| `tools/` | — (Python) | `provision.py`, `loaded_module.py`, `psxport_fetch.py`, `android_package.py`, `source_policy.py`, `verify.py` | Provisioning and its loaded-module identity, the Android packaging step, the source-boundary policy, the framework fetch, and the one gate. The PS-X EXE header reader is psxport's shared `tools/formats/psx_exe.py`, which provisioning loads. Every tool here is run by the product build, `run.sh`, or `tools/verify.py`; a tool nothing runs is not kept. |
| `tests/` | — | `test_*.cpp`, `test_*.py` | The title's unit and selftest set, run by CTest as `crashbash_*`. |

## Who owns it

Each chain names the class and the method at every hop. `psxport:` marks a framework hop.

### The frame turn

`main` (`game/entry/player_entry.cpp`) → `psxport_install_game(runtime)` → `psxport:native_boot_run(core)`
→ `psxport:FrameLoopShell::prepareProduct` → the loop's `psxport:FrameLoopShell::step(core, frame)` →
**`CrashBashFrameDriver::stepFrame`** (the title's single frame owner) → the guest process state's
update/present pair → the guest's own `DisplayFrame` override →
`CrashBashFrameDriver::deliverDisplayFields` → back at the driver tail, one
`Game::presentation.commit(core, fields, game.temporalPresentation.get())` → `psxport:GpuState` fence
advances once, which `FrameLoopShell::step` asserts.

- `stepFrame` never delegates frame ownership to guest VSync. It brackets the frame for
  `Game::perf`, and its phases are a **partition**, not a nest: the per-field SPU advance stays
  inside the GameLogic span, so the `audio` slot reports 0.00 by construction.
- **While a blocking load or presentation blocks**: it is still `CrashBashFrameDriver::stepFrame`.
  There is no second loop and no second input pump in this title. The blocking work is reached from
  `deliverDisplayFields` → `runtime::dispatchGuest(core, guest::kVblankRoot)` or from a
  `measuredGuestCall`, and each of those is a bounded executor exit the driver resumes itself; a
  guest call needing more than one display field is resumed by
  `crashbash::runtime::runGuestCallToReturn` under the `kGuestCallTurnCap` fence. Host input is not
  re-pumped here because `Game::pad.serviceFrame()` already ran at the top of `stepFrame` and
  psxport's `Pad::pollHostInput` is the single host pump.

### Host input → guest pad buffer

`psxport:psx::input::HostInput` (the SDL/gamepad mask) → `psxport:Pad::pollHostInput` (the one host
pump) → `psxport:Pad::serviceFrame` at the top of `CrashBashFrameDriver::stepFrame`, which resolves
forced / REPL / replay input and records or replays the frame → `psxport:Pad::fillBuffer` writes the
standard digital packet into the guest's VBlank slot.

- **Recording/replay key**: `crashbash::InputPhase::of(core)`, returned by
  `TitleAdapter::inputPhase`. psxport owns segment persistence and phase matching.
- **Movie / skip and any other consumer of button edges**: `psxport:Pad::sampleButtonEdges` /
  `pressedButton`. The guest's own state machine decides what an edge means; nothing here consumes
  or suppresses game input. This title has no movie player, so the logo cancel it does own is
  `crashbash::boot_logo_skip` (Start/Cross through the scene lifecycle, never a timer).
- **Debug control channel**: `psxport:DbgServer::service` at the loop tail →
  `psxport:DbgServerInternals::handleCommand` → `GameRuntime::controlCommand` →
  **`TitleAdapter::controlCommand`** → `crashbash::debug::DevArena::handle`. The command only arms
  a request; `CrashBashFrameDriver::stepFrame` advances it through `DevArena::applyArmed` using the
  title's own pad (`Pad::driveTap`), which is the same path a player's press takes.

### Guest draw → presentation

Guest draw callbacks (`0x80021770`/`0x80021990` → standard `0x80019F1C`, or the alternate
`0x8001DD50`) → **`render::registerModelFaceProducer`'s `0x80019A60` producer** (object = a3's incarnation) → the guest
body writes its packets (`FUN_800193A8` per mesh, `FUN_80029D28` for 0x3000 sprite codes) and links
them into the ordering table, each store bound to the open key → `FUN_800193A8`'s namer rebinds each
face's header to `meshFaceElement(face list, index)` → `DisplayFrame` (`registerDisplayFrameOverride`)
hands the table to the guest's own DrawOTag → `psxport:GpuDevice` executes it and `Gp0RecordTap` fills
the frame record with the keys → `CrashBashFrameDriver::stepFrame` notes `FrameCut` and calls
`Game::presentation.commit` → `psxport:FramePresenter` seals the record, asks
`TitleAdapter::sealedFrameIsCut`, and presents the keyed in-between and the record through
`RecordRasterizer`.

- **2D UI**: the component draw callbacks `0x8001C448` (text), `0x8001C690` (panel) and `0x8001C7FC`
  (quad) are producers like `0x80019A60` (object = the component's incarnation); the 2D leaves they reach
  (`0x8002992C`, `0x80029D28`, `0x8001A0D8`, via `0x800243A0`/`0x800248A0`) are overridden by
  `ui_producer.cpp:callNamed`, which names the element from the call site the leaf returns to.
- **Real field vs in-between**: with `fps60` on, each logic frame presents the in-between then the record.
  Every producer below is a state producer and renders its objects from saved state; `keyedBlend` no longer
  composes any of them. A cut, an incomplete record or a record
  with no keys is drawn as is.
- **Widescreen**: `TitleAdapter::guestWidescreenProjection` declares the player's aspect; the record
  canvas adds the margins and the guest's own primitives fill them. The HUD stays where the guest
  draws it (issue 0035).
- **A frame that presented nothing** (`deliveredFields_ == 0`) goes to
  `Game::presentation.commitUnpresented`, which seals no record.

### Producers

A producer is a native override whose packet stores carry the key `(producer, object, element, part)`
(psxport `presentation.md`, Interpolation).

| Guest address | Owner | Arg | Object | Element |
|---|---|---|---|---|
| 0x80019A60 | `registerModelFaceProducer` (`CrashBash::ModelDraw`) | A3 | the drawn render component's incarnation: `incarnationObject(a3, generation)` | 0 for a 0x3000 sprite code; a face otherwise |
| 0x8001C448 | `registerUiProducers` (`CrashBash::TextComponentDraw`) | A0 | the text component's incarnation | a glyph: `glyphElement(byte)`, its byte's main-RAM offset (string address + glyph index) |
| 0x8001C690 | `registerUiProducers` (`CrashBash::PanelComponentDraw`) | A0 | the panel component's incarnation | `PanelPart`: body, left, right, top, bottom |
| 0x8001C7FC | `registerUiProducers` (`CrashBash::QuadComponentDraw`) | A0 | the quad component's incarnation | 0 |
| 0x800798A4 (BOOT) | `callNamed` opens a `Scope` at its leaf call sites | s0 / s4 | the HUD icon record (s0, `0x800996E0`/`0x8009AD54` + 8i) or the player's digit record (s4, `0x8009AD74` + 8 * player) | 0 for an icon; `DigitPlace` for a digit |
| 0x800809A0 (BOOT) | `callNamed` opens a `Scope` at its leaf call sites | s4 | the 24-byte menu item record | a glyph, or the `DigitPlace` of `0x800248A0`'s three digits |

#### State producers

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
stays as the guest drew it.

- Face state (`model_state.cpp`): the GTE control registers, the face list, the vertex words the guest
  animated into its scratch buffer, the depth bias and limit, the table and bucket base, and the saved
  packet words (colours, texture words, draw modes) of each face the body linked. The render blends the
  rotation and translation registers and every vertex coordinate between two states with the same face list,
  projects again under `gte::Guard`, and emits each face the body links at the bucket the body gives it,
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
linked. Tests: `test_model_face_producer.cpp` (`the_render_at_t_reprojects_moved_vertices`,
`the_render_at_t_reprojects_through_a_moved_camera`, `a_culled_face_is_not_drawn_and_a_new_face_appears_at_t_one`,
`a_packet_unlinked_after_its_scope_is_not_saved`, `a_face_linked_in_the_other_table_is_not_saved`) and
`test_ui_producer.cpp` (`the_text_render_draws_the_glyphs_the_guest_linked`,
`the_panel_render_draws_the_parts_the_guest_linked`, `the_quad_render_draws_the_quad_the_guest_linked`,
`the_projected_quad_render_reprojects_through_the_moved_camera`). Each runs the shipping overrides over real
guest memory (display environment, ordering table, vertices, GTE), checks `t = 1` against the guest's own
walk with that memory scrambled after the save, and `t = 0.5` against the walk of a frame drawn halfway.

Equality with the original bodies is proved by `PSXPORT_OVERRIDE_DIFF` over the Polar Push replay (all four
addresses: memory, GTE and result registers; the native bodies end on the same `v0`/`v1` the guest leaves,
the list terminator for the mesh, the display scale for the sprites, the linked header for the shaded quad).

The string and number bodies (`0x800243A0`, `0x800248A0`) and the component callbacks that place glyphs and
panel parts stay guest code, so a render cannot re-run them: it moves the arguments they gave the leaves
(position, corners), not the component's own fields. The BOOT HUD and menu-item producers have no render
(issue 0039). Rotation is blended element by element, as a render of a turning model is only as good as that
approximation over one tick.

UI element namer: `ui_producer.cpp:callNamed` overrides `0x8002992C`, `0x80029D28`, `0x8001A0D8`,
`0x800243A0` and `0x800248A0` and reads the return address. A glyph site of `0x800243A0` (s2 is one past
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

Not producers, with the reason: the LOADING card `0x80018B08` is static. Not keyed yet: BOOT's direct
panel calls (`0x8001A6D4` from `0x80082608`/`0x8008267C`, outside any component) and the projected text
path `0x800246D0` of `0x8001A82C`; neither ran in the flow replays.

### CD / streaming

Guest CD call → `registerCdStartupOverride` / `registerCdLicenseStartupOverride` /
`registerCdFileReadOverride` (all in `game/disc/`) → the descriptor-relative read writes sectors
into guest RAM, and each sector write calls `crashbash::retireImagesForCdSectorWrite` →
`crashbash::runtime::retireAuthenticatedImagesForWrite` → `GuestExecution::retireImagesOverlapping`,
which subtracts the overwritten bytes from the authenticated generation and retires the native keys
at overwritten entries → the completed read is offered to the publication owners
(`isBootImageRead`/`isMenuImageRead`/`NestedModuleImage::offer`) → `completeModuleImageRead`
authenticates guest RAM and publishes a fresh generation →
`GuestExecution::bindAuthenticatedImage` → the authenticated-image info line.

- The five nested overlays reuse the `0x800B32B4` slot, so a publication here necessarily retires the
  previous occupant; `NestedModuleImage` exists because without it a nested load would leave its own
  code with no image identity, which the executor refuses as a typed fault.
- The retail load queue's pacing pump (`0x8001231C`) is drained to quiescence by
  `registerLoadPumpDrainOverride`, from the pump's own three queue words. That is the whole of the
  title's loading-removal policy; the LOADING card is retired separately by
  `registerLoadingCardSkipOverride` at its two measured call sites.

### Audio

The guest's own SPU writes in the VBlank root the frame driver dispatches
(`deliverDisplayFields` → `psxport:dispatchGuest(kVblankRoot)`); `Game::spu_audio.frame()` advances the
host SPU once per delivered field. There is no title-owned mixer: XA/XADPCM playback and the Beetle
SPU are psxport's (`psxport:xa_bind`, `spu_bind`, `xa_*`), and `PSXPORT_NOAUDIO` is the silent-run
switch. The frame-time `audio` slot is 0.00 by construction (see the frame turn above), not measured.

### The debug / control channel

`psxport:DbgServer` (loopback TCP, started by `psxport:native_boot_run`; always on unless the port is
taken) → `handleCommand`: framework commands first (`r/w/regs/press/shot/dumpram/step/play/state/
session/frame`), then `GameRuntime::controlCommand` → `TitleAdapter::controlCommand` →
`crashbash::debug::DevArena::handle`. Run-end execution numbers are psxport's
(`logRunEndLedger`) plus this title's guest-call turn cap (`reportGuestCallTurnCap`). A guest call the
runtime refuses is the product's one refusal route: the reason is logged and the process aborts.

## Placement rules for new work

- A native override belongs in the directory of the thing it replaces, next to its sibling overrides,
  and is added to `game/execution/native_owner_set.cpp` — never registered somewhere else.
- **A native override receives a bare `Core*`, so it owns no state of its own.** Any per-Core state an
  override reads or writes — a gameplay tally — is a member of
  `CrashBashFrameDriver`, reached through `crashbash_frame_driver.cpp`'s `frameDriver(core)` lookup.
  A file-scope or `thread_local` object is never the owner: it survives Core teardown and cannot be
  scoped to one Core's lifetime. `CrashBashFrameDriver` is the only per-Core title object reachable
  from a native override, so a second one must not be introduced without moving that lookup.
- A guest address or a guest-struct layout belongs in `game/title/crashbash_guest.h` as an
  `inline constexpr`; nothing else declares a guest address.
- A producer or element namer belongs in `game/render/`, is registered from `native_owner_set.cpp`, and
  is listed in the Producers table above.
- A run-lifetime number belongs to psxport's execution ledger, or is logged at the site the fact
  happens; it does not belong in a title-owned tally beside the code that produces it.
- This codemap is updated in the same change whenever responsibility moves or a new owner appears.
