# Prior art

Other work on Crash Bash USA `SCUS_945.70` (SHA-1 `c4a06208612ff2625b9083596d36fdf60b01be5f`, SHA-256 `fd5727a18feb2a2d5a6359a55966f0266284d1e50f64ee9b8a127a97091bd516`, load `0x80010000`, entry `0x8002E7B0`). Addresses were checked against our image on 2026-10-13; licences, trees and push dates were re-read through the GitHub API on 2026-11-13. Where a reference and our measurement disagree, the measurement wins.

## Decomp (submodule)

| submodule | URL | pinned | licence | target build and match | progress (verified in tree) | what we may take |
|---|---|---|---|---|---|---|
| `external/crash_bash_decomp` | https://github.com/jake-r-r-w/crash_bash_decomp | `95a48043` (2026-11-13) | MIT (`LICENSE`; GitHub reports NOASSERTION) | `SCUS_945.70`: `configs/USA/checksum.sha` lists our SHA-1 for `main.exe` plus SHA-1s for the 15 overlays (`engine`, `bash`, `papu`, `tank`, `pogo`, `arkenoid`, `polar`, `race`, `spin`, `dino`, `bonk`, `n2o`, `chase`, `titles`, `hub`); splat configs `configs/USA/main.yaml` and `configs/USA/overlays/*.yaml`; symbol files `configs/USA/sym.*.txt` | README claims a byte-matching rebuild of the disc; tree has 56 `src/**/*.c`, no `asm/` files; `docs/ROADMAP-A.md` says phases 1-5 done (2026-10-28) | may copy with attribution: names, types, overlay symbol maps, port code. Its `docs/ROADMAP-A.md` says names and types came from leaked minigame headers, so check provenance before copying any header |

Its native port (`port/`) has, verified in the tree: widescreen as a wider view with the game's 512-wide screen and
culling extended (`gfx_view_extra` in `port/platform/gfx.c`) and per-level extra scenery (`port/game/main/widescenery.c`,
`port/game/main/widesand.c`); 60/120 fps by blending the last two logic frames' post-transform triangles matched by
owner and face (`gfx_mark` in `port/platform/gfx.c`, `docs/PORT.md` "Frame rates"), logic staying at 30 Hz; logo and
loading skips (README "Quick start").

## Other projects

| project | URL | kind | licence | target build | build match (addresses checked) | what we may take | notes |
|---|---|---|---|---|---|---|---|
| mateusfavarin/crashbash | https://github.com/mateusfavarin/crashbash | non-matching decomp on psx-modding-toolchain; 4 named functions, ~35 named globals | MIT | NTSC-U (no hash published) | matched: `_ArcTan 0x8001463C`, `_CSQRT 0x80032490` are Ghidra entries; `_pOT 0x8005B68C` is our `kOrderingTablePointer`, `_levelID 0x8005A648` our scene/arena mode word | may copy with attribution (names) | Superseded by crash_bash_decomp. Last push 2024-05-17. |
| NyperYuhgard/BashRecomp | https://github.com/NyperYuhgard/BashRecomp | psxrecomp static recompilation | none | USA `SCUS_945.70` | matched: entry `0x8002E7B0`; seeds contain our `0x80019A60`, `0x800193A8`, `0x80019094`, `0x8001A6D4`, `0x800243A0`, `0x8001463C` | nothing from the runtime | Unnamed address list. Last push 2026-09-04. |
| CTR-tools/CTR-tools | https://github.com/CTR-tools/CTR-tools | C# format tools; unpacks `CRASHBSH.DAT` | GPL-2.0 | several Crash Bash builds | not applicable (formats) | evidence only | Last push 2026-10-04. |
| xan1242/CrashBashEx | https://github.com/xan1242/CrashBashEx | data-file extractor | MIT | July 2000 demo | mismatched build: demo | evidence only | Last push 2022-08-07. |
| lazycurler/CrashBashResearchACE | https://github.com/lazycurler/CrashBashResearchACE | ACE writeup and tools | GPL-3.0 | NTSC-J | mismatched build: Japanese release | evidence only | Last push 2025-01-29. |
| mateusfavarin/RillAi | https://github.com/mateusfavarin/RillAi | BizHawk Lua RAM reader plus Crash Ball bot | MIT | NTSC-U (no hash) | not checked: runtime RAM addresses only | may copy with attribution (RAM layout leads) | Last push 2021-03-23. |

Not found (2026-11-13): a Crash Bash entry on decomp.dev; another decomp or native port. Searched GitHub repositories
("crash bash", "crashbash", "crash bash decomp", "crash bash port", "SCUS-94570"), decomp.dev, and the web.
