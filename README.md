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

`crashbash_port` is the real executable entry: it authenticates the resident image, creates the
per-Core title runtime, binds hardware owners, and enters psxport's native boot/frame loop.

## Player input and provisioning

The intended default remains `./run.sh`, with an optional explicit USA CHD:

```sh
./run.sh
./run.sh "/path/to/Crash Bash (USA).chd"
```

The launcher provisions the authenticated executable and modules, then launches the
player entry. It never emits or compiles guest source. Disc resolution is
explicit argument, `PSXPORT_CRASHBASH_DISC`, `PSXPORT_DISC`, the same keys in `.env`, then one
root-level `.chd`. Original game content remains untracked.

`run.sh` is only a stable root handoff to `uv run --frozen python bootstrap.py`. Python owns
dependency discovery, provisioning, build policy, and launch behavior; it passes the locked
interpreter through to project tools.

## Verification

The canonical asset-free verifier builds the real native title adapter, runs every Crash Bash CTest
(including formatting, clang-tidy, launcher, provisioning, and source-policy contracts), and checks
the linked artifact for retired execution paths:

```sh
CC=clang CXX=clang++ uv run --frozen python tools/verify.py
```

`tools/verify.py` resolves the pinned framework and delegates build/test policy to its shared
`ConsumerVerifier`. Builds use `build/migration/` and the locked Python interpreter. Linux CI consumes
the same framework's pinned setup action and runs this command without any game assets.

The source-policy check reports all 17 registrations and 5 original calls and rejects the retired
translator and dispatch paths. A local real-executable check exercises the same resident loader:

```sh
PSXPORT_CARD=scratch/title-adapter-test/card.mcr uv run --frozen python -c \
  'import subprocess; subprocess.run(["build/migration/crashbash_title_adapter_test", "scratch/bin/crashbash/SCUS_945.70"], check=True)'
```

This authenticates and publishes the resident image; it does not execute retail boot or gameplay.
The bounded replays under `replays/flow/` are the scenarios for gameplay qualification.

## Product completion

The complete port must prove nonzero translated execution, correct
loaded-image invalidation, bounded VSync/interrupt/exception exits, image-scoped overrides, and a gameplay link without an explicit interpreter mode. Boot, logos, menus, FMV, and a first frame are implementation
checkpoints only. Completion requires representative controllable gameplay with correct rendering,
audio, timing, devices, and frame-time evidence, followed by broader retail-mode coverage.

See `docs/project-goals.md`, `docs/project-state.md`, `docs/codemap.md`, and
`docs/re-frontier.md` for the canonical goal, state, ownership, and evidence boundaries.
