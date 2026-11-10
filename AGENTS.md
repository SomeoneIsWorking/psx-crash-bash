# Crash Bash port

A psxport native/dynarec title: Crash Bash's native overrides plus psxport's pinned Lightrec
integration for every remaining guest path. Framework contract: `external/psxport/AGENTS.md`;
framework changes: `external/psxport/docs/workspace/PROTOCOL.md`. Execution policy: the
`dynarec-port` and `dynarec-overrides` skills.

`external/psxport` is not a git submodule. It is a relative symlink to the workspace's live framework
checkout when one exists, or a clone of psxport `main` where there is none. psxport's
`tools/psxport_fetch.py`, run from psxport `main` by `bootstrap.py`, establishes that checkout; there
is no framework pin.

## Overrides

`crashbash_runtime_test` checks every native override installation by address. Override and
translated-block identity include the loaded image generation because several modules reuse the
same guest address range.

## Presentation

The picture is the guest's GP0 output on psxport's record path (`external/psxport/docs/presentation.md`).
60 fps comes from producers: native overrides that key the guest's packets by the guest object drawing
them, found by RE, never by draw order or count. Cuts come from the guest's scene state. Widescreen is
the record canvas; guest 2D is not re-laid out.

The completion bar is representative gameplay that works and looks correct. Frame comparison, an
independent emulator, and the separately built test target may locate a divergence; they do not
define presentation completion. Boot, logos, menus, attract loops, and one frame are not
representative gameplay.

## Layout

`game/runtime/runtime.{h,cpp}` (`Runtime`) is the title's `GameRuntime`; boot, frame, device,
diagnostics, and render responsibilities live in dedicated modules (`docs/codemap.md`).
