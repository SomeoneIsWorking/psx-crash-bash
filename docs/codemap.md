# Codemap — Crash Bash

Where Crash Bash's code lives and what each owner owns. Feature chains are in `docs/features.md`, the
60 fps replay in `docs/presentation.md`; the framework side is `external/psxport/docs/codemap.md`.

Frame loop: owned. `FrameDriver` runs the process runner `0x800270F0`'s update and present; native DisplayFrame `0x800272AC` replaces VSync(n) with the cadence word `0x8004E0E0` fields, extended to the field grid when the guest clock ran past them (`Timing::vsyncFields`).

## Owners

| owner | path | owns |
|---|---|---|
| `createCatalog` | `game/entry/catalog.*` | The one-title catalog: `SCUS_945.70` identity, `PSXPORT_CRASHBASH_DISC`, provisioning under `scratch/bin`, one `Runtime`; the entry is psxport's product main |
| `Runtime` | `game/runtime/runtime.*` | The `GameRuntime`: resident admission, crt0 (`Crt0Boot`), stock libcd/VSync and the card device bound in `registerOverrides`, the title overrides, session, frame driver, packet pool, replay frame, cut, widescreen projection, input phase, title commands, native state |
| `Session`, `session` | `game/runtime/session.*` | Per-Core title state: guest execution, `DevArena`, `DevPause`, Polar Push census, component incarnations, frame cut, draw replay |
| `GuestExecution` | `game/execution/guest_execution.*` | Image generations per `GuestImage` (the `0x800B32B4` slot is reused by five overlays), native keys bound to them, retirement on overwrite, typed guest calls and their turn cap |
| `ImageIdentityState` | `game/execution/image_identity_state.*` | The save-state section of bound image records; refuses what no title writes |
| `imageContentIdentity`, executable and module facts | `game/execution/image_content_identity.h`; `executable_identity.h.in`, `module_image_identity.h.in` (configured to `title_metadata/execution/`, one header per module manifest) | The catalog content tag; the resident header facts and each module image's manifest facts |
| `runBootPrefix` | `game/boot/boot_prefix.*` | The finite boot prefix after crt0; the frame driver owns repetition |
| boot owners | `game/boot/boot_object_callbacks.*`, `boot_logo_skip.*`, `memory_card_startup.*` | BOOT's object callbacks, the logo skip through the scene lifecycle, card startup |
| module images | `game/boot/authenticated_module_image.*`, `boot_image.*`, `menu_image.*`, `nested_module_image.*` | Authenticating a completed read into BOOT, menu and nested-overlay generations |
| disc | `game/disc/cd_file_read.*`, `cd_startup.*`, `cd_license_startup.*`, `loading_card_skip.*` | The descriptor-relative read and load-pump drain, the startup and licence handshakes, which draw-context selection is the LOADING card |
| `FrameDriver`, `frameDriver` | `game/frame/frame_driver.*` | The `FrameDriver`: one measured process update/present per step, dev requests at the boundary, one commit |
| `registerDisplayFrameOverride` | `game/frame/display_frame.*` | The guest's `DisplayFrame`: frame arena split, DrawOTag, fields handed to the driver |
| `registerGpuTimeoutOverrides` | `game/frame/gpu_timeout.*` | The libgpu timeout bodies as title natives (the stock ones leave r2 differently) |
| `FrameCut` | `game/frame/frame_cut.*` | Whether a sealed record is a cut: process state, mode scene or menu screen changed |
| `DrawReplay` | `game/frame/draw_replay.*` | The title's `ReplayFrame`: the scene fade track, the two motion sources, when to sample and when to forget |
| `DrawSection` | `game/frame/draw_section.*` | The replayed draw section: the root scene present on the scratchpad stack, the chain head |
| `measuredGuestCall` | `game/frame/measured_guest_call.h` | A guest call from a retail call site with its measured instruction ticks |
| `RecordingPhase` | `game/input/recording_phase.*` | The replay key from the outer and menu scene records |
| `TouchControls` | `game/input/touch_controls.*` | The touch pad layout and hit tests |
| `registerPolarPushContactOverride`, `ContactCensus` | `game/gameplay/polar_push_contact.*` | DAT22510 Polar Push contact traversal, motion and effects |
| `debug::DebugMenu` | `game/debug/debug_menu.*` | The title debug menu (psxport `TitleDebugMenu`): `arenas`, `pause` verbs and the warp catalogue (arena, mode) |
| `DevArena`, `DevPause` | `game/debug/dev_arena.*`, `dev_pause.*` | The arena arrival and `pause` routes: armed by the debug menu, applied by the driver as pad presses |
| guest facts | `game/title/guest.h` | Recovered guest addresses and layouts; facts only |
| `ContextMotion` | `game/render/context_motion.*` | The draw contexts `FUN_80018B08` selected, sampled and blended; the LOADING card retired there |
| `ComponentMotion` | `game/render/component_motion.*` | The render components `0x80019A60`/`FUN_8001ACC0` drew, sampled and blended (position, angles or matrix, scale, pose) |
| `ComponentIncarnations` | `game/render/component_incarnation.*` | Copies of the render template `0x8005A830` start a new object generation |
| ordering tables | `game/render/ordering_table_slots.*` | Names both tables; the draw base carried between tables |
| AppImage | `packaging/appimage.py`, `packaging/linux/` (desktop entry, icon, metainfo) | Packages `crashbash_port` alone; the first-run disc screen and packaged defaults are psxport's `FirstRunSetup` (issue 0042) |
| Android | `platform/android/` | Activity and SAF media import |
| provisioning | `tools/provision.py`, `tools/loaded_module.py` | Extracting and authenticating the executable and modules from the disc |
| launcher, packaging | `bootstrap.py`, `tools/run.py`, `tools/android_package.py` | The `external/psxport` fetch, provisioning, build and exec on psxport `tools/port/launcher.py`, the Android package |
| replays | `replays/flow/` | Front-end flows into Polar Push, Crash Ball, Pogo Painter and Crate Crush (battle and tournament) |

## Where new work goes

- A guest function Crash Bash replaces → `game/<subsystem>/`, installed from `Runtime::registerOverrides`.
- A stock library body → a psxport stock class bound in `Runtime`, never a title copy.
- Per-Core title state → `Session`; an override reaches it through `session(core)`.
- Moving state the draw reads → a track or `MotionSource` in `DrawReplay` (`game/render/`), described in `docs/presentation.md`.
- A guest address or layout → `game/title/guest.h`.
- Audio is psxport's (SPU, XA); this title owns no mixer.
- Anything every title shares → psxport, not here.
