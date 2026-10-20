# Crash Bash

Crash Bash is being migrated to one native/Lightrec hybrid over the authenticated USA PlayStation
release. The title retains its cohesive native boot, device, input and producer owners; the picture is
the guest's own GP0 output on psxport's record path. Every other guest path will execute through
psxport's per-`Core` Lightrec runtime.

The old offline translator integration, generated guest corpus, static dispatcher/registry, seed
configuration, static-only verifiers, build tree, and static product binaries were deleted before
dynarec implementation. They are not a bridge, fallback, or oracle.

The native title adapter authenticates and loads the resident executable, installs the 17
image-qualified native owners, and selects the record path with keyed 60 fps in-betweens. Its 5
original-body calls enter psxport's generation-aware dispatcher and scoped original-call boundary.

`crashbash_port` is psxport's framework `main` over a one-title catalog: `psx::host::ProductHost`
boots Crash Bash directly, authenticates its executable against the catalog identity, and the title's
`admitExecutable` publishes the resident image.

## Player input and provisioning

The intended default remains `./run.sh`, with an optional explicit USA CHD:

```sh
./run.sh
./run.sh "/path/to/Crash Bash (USA).chd"
```

`run.sh` provisions the authenticated executable and modules, then starts `crashbash_port`. It never
emits or compiles guest source. Disc resolution is explicit argument, `PSXPORT_CRASHBASH_DISC`,
`PSXPORT_DISC`, the same keys in `.env`, the disc chosen on psxport's first-run screen, then one
root-level `.chd`. The AppImage (`packaging/appimage.py`) ships `crashbash_port` alone; with no disc it
shows that first-run screen. Original game content remains untracked.

`run.sh` is only a stable root handoff to `uv run --frozen python bootstrap.py`. Python owns
dependency discovery, provisioning, build policy, and launch behavior; it passes the locked
interpreter through to project tools.

## Building

There are no unit tests or gates. Build with Clang:

```sh
CC=clang CXX=clang++ uv run --frozen python external/psxport/tools/port/consumer_build.py --build build/agent --target crashbash_port
```

Linux CI consumes the same framework's setup action and runs the same build without any game assets.
The bounded replays under `replays/flow/` are the scenarios for gameplay qualification.

## Product completion

The complete port must prove nonzero translated execution, correct
loaded-image invalidation, bounded VSync/interrupt/exception exits, image-scoped overrides, and a gameplay link without an explicit interpreter mode. Boot, logos, menus, FMV, and a first frame are implementation
checkpoints only. Completion requires representative controllable gameplay with correct rendering,
audio, timing, devices, and frame-time evidence, followed by broader retail-mode coverage.

See `docs/project-goals.md`, `docs/project-state.md`, `docs/codemap.md`, and
`docs/re-frontier.md` for the canonical goal, state, ownership, and evidence boundaries.
