# Features — Crash Bash

Title hops only. Each row is where Crash Bash joins a psxport feature hop (`psxport F-X.n`); a feature
with no row runs on the framework alone. Producers and state renders in detail: `docs/presentation.md`.

## F-BOOT

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-BOOT.1 | `game/entry/catalog.cpp:createCatalog` — `SCUS_945.70` identity, `PSXPORT_CRASHBASH_DISC`, provisioning under `scratch/bin`, one `Runtime` |
| 2 | psxport F-BOOT.5 | `game/runtime/runtime.cpp:Runtime::startProgram` — crt0 (`kCrt0`), then `game/boot/boot_prefix.cpp:runBootPrefix` |
| 3 | psxport F-BOOT.6 | `game/runtime/runtime.cpp:Runtime::registerOverrides` — binds `StockLibcd`, `StockVSync` (`0x800320EC`) and the card device, then installs the title overrides and state renders |
| 4 | psxport F-BOOT.6 | `game/runtime/session.h:Session` — per-Core guest execution, dev requests, Polar Push census, incarnations, packet collector, frame cut, state producers |
| 5 | psxport F-BOOT.6 | `game/boot/boot_object_callbacks.cpp:registerBootObjectCallbackOverrides`, `game/boot/memory_card_startup.cpp:registerMemoryCardStartupOverride` — the boot object callbacks and card startup |

## F-TURN

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-TURN.2 | `game/frame/frame_driver.cpp:FrameDriver::stepFrame` — one measured process update/present, one commit |
| 2 | psxport F-TURN.2 | `game/frame/display_frame.cpp:registerDisplayFrameOverride` → `FrameDriver::deliverDisplayFields` — replaces DisplayFrame `0x800272AC`'s `VSync(n)`: the VBlank root `0x8003ADD4` per field, `Timing::vsyncFields(n)` fields, n = the cadence word `0x8004E0E0` |
| 3 | psxport F-TURN.2 | `game/execution/guest_execution.cpp:runGuestCallToReturn` — a blocking guest call resumed across fields under the turn cap |
| 4 | psxport F-TURN.4 | `game/runtime/runtime.cpp:Runtime::reportRun` — the guest-call turn cap |

## F-INPUT

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-INPUT.2 | `game/input/recording_phase.cpp:RecordingPhase::of` — outer scene and menu scene records packed |

## F-CD

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-CD.1 | `game/runtime/runtime.cpp:Runtime` — stock command `0x8003EBF8`, sync `0x8003E6B0`, search `0x80034C6C` |
| 2 | psxport F-CD.1 | `game/disc/cd_file_read.cpp:registerCdFileReadOverride` — descriptor-relative reads; each sector write retires the images it overwrites |
| 3 | psxport F-CD.1 | `game/disc/cd_startup.cpp`, `game/disc/cd_license_startup.cpp` — the measured startup and licence handshakes |
| 4 | psxport F-CD.3 | `game/runtime/runtime.cpp:Runtime::admitExecutable` — the resident image generation bound in `GuestExecution` |
| 5 | psxport F-CD.3 | `game/boot/nested_module_image.cpp:NestedModuleImage` with `boot_image.cpp`, `menu_image.cpp` — BOOT, menu and the five overlays of slot `0x800B32B4` authenticated and published |

## F-RENDER

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-RENDER.2 | `game/runtime/runtime.cpp:Runtime::packetPool` — live base/end pointers `0x8005F790`/`0x8006379C` and `0x8005F794`/`0x800637A0` |

## F-FPS60

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-FPS60.1 | `game/runtime/runtime.cpp:Runtime::stateProducer` — answered from `Session::stateProducers` |
| 2 | psxport F-FPS60.1 | `game/render/model_face_producer.cpp:registerModelFaceProducer` — model draw `0x80019A60`, face state in `model_state.cpp` |
| 3 | psxport F-FPS60.1 | `game/render/ui_producer.cpp:registerUiProducers` — text, panel and quad components and BOOT's HUD and menu, leaf state in `leaf_state.cpp` |
| 4 | psxport F-FPS60.1 | `game/render/component_incarnation.cpp:registerComponentIncarnationOwners` — a re-initialised component is a new object |
| 5 | psxport F-FPS60.2 | `game/frame/frame_cut.cpp:FrameCut::isCut` — process state, mode scene and menu screen changes |

## F-WIDE

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-WIDE.1 | `game/runtime/runtime.cpp:Runtime::guestWidescreenProjection` — the record canvas at the configured aspect; guest 2D is not re-laid out |

## F-SAVE

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-SAVE.2 | `game/runtime/runtime.cpp:Runtime::nativeState` — `GuestExecution`'s bound image records |

## F-DEBUG

| hop | joins | owner |
|---|---|---|
| 1 | psxport F-DEBUG.1 | `game/debug/debug_menu.cpp:debug::DebugMenu` — `arenas`, `arena <id> [battle|tournament]` (`DevArena`) and `pause <action>` (`DevPause`), applied by `FrameDriver::stepFrame` at a frame boundary |

## CB-SKIP (title only)

| hop | owner |
|---|---|
| 1 | `game/boot/boot_logo_skip.cpp:registerBootLogoSkipOverride` — Start/Cross ends the logos through the scene lifecycle |
| 2 | `game/disc/cd_file_read.cpp:registerLoadPumpDrainOverride` — the load pump `0x8001231C` drained to quiescence |
| 3 | `game/disc/loading_card_skip.cpp:registerLoadingCardSkipOverride` — the LOADING card retired at its two call sites |

## CB-POLAR (title only)

| hop | owner |
|---|---|
| 1 | `game/gameplay/polar_push_contact.cpp:registerPolarPushContactOverride` — DAT22510 contact traversal, motion and effects |
| 2 | `game/runtime/session.h:Session::polarContactCensus` — the per-Core contact census |
