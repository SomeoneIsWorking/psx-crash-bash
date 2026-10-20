# Codemap — Crash Bash

Where Crash Bash's code lives and what each owner owns. Feature chains are in `docs/features.md`, producers
and state renders in `docs/presentation.md`; the framework side is `external/psxport/docs/codemap.md`.

## Owners

| owner | path | owns |
|---|---|---|
| `createCatalog` | `game/entry/catalog.*` | The one-title catalog: `SCUS_945.70` identity, `PSXPORT_CRASHBASH_DISC`, provisioning under `scratch/bin`, one `Runtime`; the entry is psxport's product main |
| `Runtime` | `game/runtime/runtime.*` | The `GameRuntime`: resident admission, crt0 (`Crt0Boot`), stock libcd/VSync and the card device bound in `registerOverrides`, the title overrides, session, frame driver, packet pool, state producers, cut, widescreen projection, input phase, title commands, native state |
| `Session`, `session` | `game/runtime/session.*` | Per-Core title state: guest execution, `DevArena`, `DevPause`, Polar Push census, component incarnations, packet collector, frame cut, the state-producer table |
| `GuestExecution` | `game/execution/guest_execution.*` | Image generations per `GuestImage` (the `0x800B32B4` slot is reused by five overlays), native keys bound to them, retirement on overwrite, typed guest calls and their turn cap |
| `ImageIdentityState` | `game/execution/image_identity_state.*` | The save-state section of bound image records; refuses what no title writes |
| `imageContentIdentity`, executable and module facts | `game/execution/image_content_identity.h`; `executable_identity.h.in`, `module_image_identity.h.in` (configured to `title_metadata/execution/`, one header per module manifest) | The catalog content tag; the resident header facts and each module image's manifest facts |
| `runBootPrefix` | `game/boot/boot_prefix.*` | The finite boot prefix after crt0; the frame driver owns repetition |
| boot owners | `game/boot/boot_object_callbacks.*`, `boot_logo_skip.*`, `memory_card_startup.*` | BOOT's object callbacks, the logo skip through the scene lifecycle, card startup |
| module images | `game/boot/authenticated_module_image.*`, `boot_image.*`, `menu_image.*`, `nested_module_image.*` | Authenticating a completed read into BOOT, menu and nested-overlay generations |
| disc | `game/disc/cd_file_read.*`, `cd_startup.*`, `cd_license_startup.*`, `loading_card_skip.*` | The descriptor-relative read and load-pump drain, the startup and licence handshakes, the LOADING card skip |
| `FrameDriver`, `frameDriver` | `game/frame/frame_driver.*` | The `FrameDriver`: one measured process update/present per step, dev requests at the boundary, one commit |
| `registerDisplayFrameOverride` | `game/frame/display_frame.*` | The guest's `DisplayFrame`: frame arena split, DrawOTag, fields handed to the driver |
| `registerGpuTimeoutOverrides` | `game/frame/gpu_timeout.*` | The libgpu timeout bodies as title natives (the stock ones leave r2 differently) |
| `FrameCut` | `game/frame/frame_cut.*` | Whether a sealed record is a cut: process state, mode scene or menu screen changed |
| `measuredGuestCall` | `game/frame/measured_guest_call.h` | A guest call from a retail call site with its measured instruction ticks |
| `RecordingPhase` | `game/input/recording_phase.*` | The replay key from the outer and menu scene records |
| `TouchControls` | `game/input/touch_controls.*` | The touch pad layout and hit tests |
| `registerPolarPushContactOverride`, `ContactCensus` | `game/gameplay/polar_push_contact.*` | DAT22510 Polar Push contact traversal, motion and effects |
| `DevArena`, `DevPause` | `game/debug/dev_arena.*`, `dev_pause.*` | The `arena` and `pause` commands: armed by a command, applied by the driver as pad presses |
| guest facts | `game/title/guest.h` | Recovered guest addresses and layouts; facts only |
| `registerModelFaceProducer` | `game/render/model_face_producer.*` | The `0x80019A60` producer and the mesh body `0x800193A8` override |
| `projectFaces`, `GuestFaceSink` | `game/render/face_projection.*` | The mesh face body over `core.gte()`; faces written to the guest pool |
| face state | `game/render/model_state.*` | Saves a mesh call; renders it at t through `HostFaceSink` |
| `registerUiProducers`, `underName` | `game/render/ui_producer.*` | The component producers, the 2D leaf and body overrides, the call-site namer |
| leaves | `game/render/leaf_packets.*`, `leaf_port.*`, `host_leaf.*`, `leaf_state.*` | The 2D leaf bodies over `EmitMemory`; guest and host ports; leaf state and its render |
| component bodies | `game/render/text_bodies.*`, `panel_bodies.*`, `component_body.*`, `placement_blend.h` | String, digit, panel and border bodies as values a state holds and blends |
| `ComponentIncarnations` | `game/render/component_incarnation.*` | Copies of the render template `0x8005A830` start a new object generation |
| `PacketCollector` | `game/render/packet_collector.*` | What a producer scope drew, kept at DrawOTag if its packets are still linked |
| ordering tables | `game/render/ordering_table_slots.*` | Names both tables; a packet's linked bucket; the draw base carried between tables |
| render support | `game/render/draw_globals.*`, `packet_decode.*` | The globals every packet body reads; a packet as the primitive the GPU draws |
| `registerStateRenders` | `game/render/state_renders.*` | Fills the session's state-producer table |
| AppImage | `packaging/appimage.py`, `packaging/linux/` (desktop entry, icon, metainfo) | Packages `crashbash_port` alone; the first-run disc screen and packaged defaults are psxport's `FirstRunSetup` (issue 0042) |
| Android | `platform/android/` | Activity and SAF media import |
| provisioning | `tools/provision.py`, `tools/loaded_module.py` | Extracting and authenticating the executable and modules from the disc |
| framework fetch, packaging | `bootstrap.py:prepare_product`, `tools/android_package.py` | The `external/psxport` fetch (psxport `tools/psxport_fetch.py`), the Android package |
| tests | `tests/` | Runtime bindings and admission, guest execution and module images, the frame cut, input phase, dev pause, draw base, producers and their renders; `title_game.h` builds a `Game` over its own `Runtime` |
| replays | `replays/flow/` | Front-end flows into Polar Push, Crash Ball, Pogo Painter and Crate Crush (battle and tournament) |

## Where new work goes

- A guest function Crash Bash replaces → `game/<subsystem>/`, installed from `Runtime::registerOverrides`.
- A stock library body → a psxport stock class bound in `Runtime`, never a title copy.
- Per-Core title state → `Session`; an override reaches it through `session(core)`.
- A producer or namer → `game/render/`, with its state render in `registerStateRenders` and a row in `docs/presentation.md`.
- A guest address or layout → `game/title/guest.h`.
- Audio is psxport's (SPU, XA); this title owns no mixer.
- Anything every title shares → psxport, not here.
