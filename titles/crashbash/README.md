# Crash Bash — selected target

The selected target is the North American retail disc (`SCUS-94570`, NTSC-U). Its identity and boot
image measurements define the authenticated inputs for the runtime guest executor. Recorded runs
reached guest main and the measured IRQ callback, executed BOOT plus four measured alternatives in
the nested `0x800B32B4` slot, and followed a real Cross press into DAT28136 without an unresolved
guest target or guest-VSync violation.

`SYSTEM.CNF` names `cdrom:\SCUS_945.70;1`. The executable independently contains both the
`Sony Computer Entertainment Inc. for North America area` and `BASCUS-94570` markers. The disc also
contains a Spyro 3 demo payload, but that is not the configured boot executable.

| executable fact | measured value |
|---|---|
| disc path | `SCUS_945.70` |
| disc extent | LBA 23, 432,128 bytes |
| disc layout | one 75,774-sector data track; `SYSTEM.CNF` at LBA 234; `CRASHBSH.DAT` at LBA 236 |
| file size | 432,128 bytes |
| SHA-1 | `c4a06208612ff2625b9083596d36fdf60b01be5f` |
| SHA-256 | `fd5727a18feb2a2d5a6359a55966f0266284d1e50f64ee9b8a127a97091bd516` |
| entry PC | `0x8002E7B0` |
| text mapping | `0x80010000..0x80079000` (`0x69000` bytes, all present) |
| header GP | `0x00000000` |
| header SP | `0x801FFFF0` |

The tracked machine-readable executable identity is `executable.json`.

## Loaded-module evidence

The first file read loads 189 sectors at `0x80078C90`; its first word is the dispatcher entry
`0x80092BDC`. That BOOT module later reuses a nested slot at `0x800B32B4` for MENU and three
DAT-prefixed alternatives. MENU's raw function table at `+0x6270` holds observed callback
`0x800B5244`; the entryless alternatives are reached through runtime-patched dispatch tables and are
therefore named by measured disc LBA rather than inferred roles. Full-DAT identity, disc LBA, DAT
offset, payload identity, load range, and any entry-pointer evidence are tracked in the five
`*_module.json` manifests. Provisioning verifies 32/32 facts before publishing any payload. The
Cross-controlled product path loads the 42-sector DAT28136 image from LBA 28136, executes registration
body `0x800B4E1C`, replaces app callback `0x80093038` with `0x800B4694`, and executes the new callback.

## CRT0 evidence

psxport's shipping CRT0 decoder scanned 36 instructions from the real entry, reached the InitHeap call,
and resolved 8/8 boot fields:

| field | measured value |
|---|---|
| BSS zero range | `0x8006E9F0..0x80078C90` (41,632 bytes) |
| stack-top word | `0x8002E860` (`0x00200000`) |
| second stack word | `0x8006D8B4` (`0x00008000`) |
| runtime SP/FP | `0x80200000` |
| runtime GP | `0x8006E9EC` |
| heap | `0x80078C90`, `0x17F370` bytes; InitHeap receives `0x80078C94` |
| InitHeap thunk | `0x8003ACCC` |
| heap globals | absent; values remain register-only in this CRT0 |

The runtime CRT0 audit compared 10/10 fields
with zero disagreement or unresolved values before dispatching guest main `0x8002718C`.

## Reproduce the measurement

Build the framework tools explicitly with Clang, then inspect and extract from a provisioned disc:

```sh
CC=clang CXX=clang++ cmake -S . -B build/maintainer
cmake --build build/maintainer --target discdump crt0_extract -j16
build/maintainer/psxport_build/tools/discdump list "$CRASHBASH_DISC"
build/maintainer/psxport_build/tools/discdump get SYSTEM.CNF "$CRASHBASH_DISC" scratch/raw/crashbash-usa
build/maintainer/psxport_build/tools/discdump get SCUS_945.70 "$CRASHBASH_DISC" scratch/raw/crashbash-usa
build/maintainer/psxport_build/tools/crt0_extract scratch/raw/crashbash-usa/SCUS_945.70
```

Disc images and extracted files stay under external storage or gitignored `scratch/`; neither is tracked.
For normal provisioning, `uv run --frozen python tools/provision.py [disc.chd]` owns disc resolution,
validates `SYSTEM.CNF`, checks these same 11 executable facts plus every registered module identity,
and atomically publishes only the verified executable and complete payload set.
