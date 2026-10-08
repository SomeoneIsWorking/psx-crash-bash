# RE frontier

This is the ordered evidence chain from the authenticated retail images to the native/dynarec product.
The static product was deleted before dynarec implementation. It is not regenerated, built, run,
retained as a fallback, or used as a comparison oracle.

Running behavior and visible correctness establish the product frontier. Pixel and state comparisons
remain diagnostics for finding causes, not completion conditions.

## Runtime migration

### runtime.retire-static — Remove the old execution path before replacement work

- status: re-verified
- deps:
- evidence: The tracked emitter integration, seed file, generated-registry installer, and static-only
  verifiers are deleted. The ignored generated corpus, static build tree, and static product binaries
  are absent. The source-policy tests reject their return.
- where: `CMakeLists.txt`, `tools/source_policy.py`, `tests/test_source_policy.py`
- gap:
- notes: CMake names the single missing psxport Lightrec adapter boundary. There is no compatibility
  product, selector, or fallback.

### runtime.target — Authenticate the retail target

- status: re-verified
- deps:
- evidence: USA `SYSTEM.CNF` boots `SCUS_945.70`; SHA-256
  `fd5727a18feb2a2d5a6359a55966f0266284d1e50f64ee9b8a127a97091bd516`; PS-X EXE entry
  `0x8002E7B0`, load `0x80010000`, text `0x69000`; CRT0 extraction resolved 8/8 boot fields.
- where: `titles/crashbash/executable.json`, `titles/crashbash/README.md`
- gap:
- notes: Identity and CRT0 evidence only; no execution-engine claim follows from it.

### runtime.images — Authenticate every reached loaded image

- status: re-verified
- deps: runtime.target
- evidence: BOOT is at LBA 35799. MENU, DAT28272, DAT28241, DAT28136, DAT28382, and DAT22510
  occupy the reused nested region at `0x800B32B4`. DAT22510 is `CRASHBSH.DAT + 0x02B81000`, 71 sectors,
  size `0x23800`, SHA-256 `7477dd74ccd80f8f1b8ce63265f8acf130e36a97f1e9182767383556e8ffc7e1` (the retail
  read is 71 sectors; sector 71 continues the module's halfword table).
  The provision path validates the complete 73,220,096-byte DAT and 44/44 recorded facts.
- where: `tools/provision.py`, `tools/loaded_module.py`, `titles/crashbash/*_module.json`
- gap:
- notes: Runtime image generation is part of override and translated-block identity. No module bytes
  are tracked.

### runtime.lightrec — Execute the authenticated images through pinned Lightrec

- status: todo
- deps: runtime.images, psxport per-`Core` dynarec executor
- evidence:
- where: `external/psxport`, `game/execution/`
- gap: Wire the title to psxport's maintained, pinned Lightrec integration; prove nonzero translated
  blocks; audit the gameplay link and configuration surfaces to prove that the interpreter in the
  separately built test target, including diagnostics, is absent and cannot be selected or entered as
  a fallback.
- notes: No offline translator, generated function table, or generated corpus may participate.

### runtime.overrides — Preserve the native override boundary

- status: todo
- deps: runtime.lightrec
- evidence: The migration audit counts 29 current native override installations.
- where: title-owned modules under `game/`
- gap: Register all 29 by complete runtime image generation plus guest address, exercise enabled and
  disabled paths, and prove that overlay replacement cannot inherit a colliding resident/module hook.
- notes: Override changes invalidate any translated call path that captured the old decision.

### runtime.original-calls — Replace generated-body calls with scoped original calls

- status: todo
- deps: runtime.overrides
- evidence: The migration audit counts 17 calls from native owners to guest bodies.
- where: title-owned native overrides; psxport runtime executor API
- gap: Replace all 17 with the single runtime original-call operation. It suppresses only the active
  override for one call, executes the original authenticated guest body through Lightrec, preserves
  guest ABI/state, and cannot recurse into itself.
- notes: An original call never selects a static body or interpreter.

### runtime.first-frontier — Re-establish the current reached path dynamically

- status: todo
- deps: runtime.original-calls
- evidence: Recorded binary facts reach MENU `0x800B5244`; Cross schedules `0x800B8E50`, loads
  DAT28136, registers `0x800B4E1C`, replaces callback `0x80093038` with `0x800B4694`, and executes
  the update. The independent oracle records ResetCallback/setjmp resume
  `0x80031A80 -> 0x8003ACEC -> 0x80031AE8` and IRQ2 `0x8003F5F0 -> 0x8003E14C`.
- where: `replays/flow/` scenarios, `docs/issues/`, future dynarec runtime diagnostics
- gap: Authenticated BOOT and MENU publication now cross strict MENU entry `0x800B5244`. The
  original MENU body exhausts the current-turn budget in the resident pixel-conversion loop at
  `0x80018AA0` with live finite row/pixel counters (issue 0029). Preserve the nested original
  call across bounded host-turn continuations, then continue toward the recorded Cross-selected
  DAT28136 route through nonzero
  Lightrec execution with all reached native owners active, bounded exits, correct invalidation,
  and no guest-VSync violation.
- notes: First frame, logos, and menu entry are implementation discriminators, not representative
  gameplay conformance.

### runtime.gameplay — Prove representative interactive hybrid gameplay

- status: todo
- deps: runtime.first-frontier, flow.pad-input, graphics.producers
- evidence: Retained acceptance inputs reach live Crashball (3,740 frames), Battle Crate Crush
  (15,401), Tournament Crate Crush (7,910), and Polar Push (17,682), each with observable player
  movement and complete native presentation.
- where: `replays/flow/`, producers under `game/render/`, independent emulator diagnostics
- gap: Re-run the bounded scenarios on the native/dynarec gameplay product with nonzero Lightrec
  execution, correct timing/interrupt/device state, rendering, audio, and declared frame-time evidence
  on each released host architecture.
- notes: The interpreter in a separately built test target, including diagnostics, may diagnose a first
  divergence; it is not linked into or selectable by this product.

## Preserved retail and native boundaries

### flow.pad-input — Deliver host pad state through the retail SIO path

- status: re-verified
- deps: runtime.images
- evidence: Class 2 element `0x8006D984`, verifier `0x8003B1BC`, and handler `0x8003B224` own the
  VBlank/SIO chain. Idle/START A/B changes packet `0x80077FBC`, parsed P1 `0x80063A92`, and active-high
  P1 `0x8005133C` while P2 `0x80051394` remains unchanged.
- where: shared psxport SIO/timing owners
- gap:
- notes: Direct guest-buffer injection is rejected.

### flow.menu-cross — Preserve the measured Cross transition

- status: re-verified
- deps: flow.pad-input
- evidence: Active table `0x800B8E28` updates through `0x800B3CA8`; rising-edge `0x80051380`
  accepts Cross `0x4000` at `0x800B3D88-0x800B3D8C` and schedules `0x800B8E50` through
  `0x8009F8A8`.
- where: scene record `0x8009F658`, table `0x800B8E28`
- gap: none for the menu edge itself. The interactive MATCH downstream of that mode is S016.
- notes: START is not an alternate acceptance route. `kAppModeVtable` (`0x8004E0DC`) is NOT the mode
  selector — the scene record at `0x8009F658` is.

### graphics.producers — Key the guest's model packets by object and face

- status: re-verified
- deps: runtime.images
- evidence: `0x80019A60(code, model, flags, object)` is reached only from `0x80019F1C` and `0x8001DD50`
  (Ghidra callers); `0x80019F1C` is called from draw callbacks `0x80021770`/`0x80021990` with
  `entity + 8`. For 0x1000/0x2000/0x4000/0x5000 codes it resolves a mesh record, takes a packet buffer
  from `0x80019094` (free list at `model + parity * 0x28 + 4 + 0xC`, popped in call order) and calls
  `0x800193A8(buffer, vertices, faceList)`, which writes face i at `buffer + i * 40` over
  `{flags, count}` strips ended by count 0xFF and links visible faces into the OT slice at
  `0x800569D8`. 0x3000 codes draw one flat sprite through `0x80029D28`. Producers keyed 99.5%+ of
  live-match primitives with 0 duplicate keys (S006). A render component starts a new incarnation
  whenever the render template `0x8005A830` is copied into it; the complete copier set (every image
  scanned for `addiu …, 0xA830` and calls to the copiers) is `0x80018F3C`, `0x8001D3B0`, `0x8001D48C`,
  `0x8001D6B4`, `0x8001E0A8`, `0x80021A1C`, `0x80021798`, `0x8002128C` and BOOT's `0x8007EB68`,
  `0x8009440C`. The briefing page builder `0x80095BEC` re-fills layer components at
  `0x800A13F4 + layer * 0x1500` through `0x8001D3B0`, then `0x800952F8` sets each from the page record.
- where: `game/render/model_face_producer.{h,cpp}`
- gap: none for models. Large steps of `0x800A28F4`'s off-screen faces are the guest's own motion
  (issue 0038).
- notes: The record path makes the guest's GP0 output the picture; nothing here reconstructs it.

### graphics.ui-producers — Key the 2D UI by the element that draws it

- status: re-verified
- deps: graphics.producers
- evidence: UI render components draw through their +0x54 callbacks with the component in a0: text
  `0x8001C448` → `0x8001A82C` → `0x800243A0(x, y, string, ...)` when component flag 0x10000000 is set
  (string at component+0x6C); panel `0x8001C690` → `0x8001A6D4(component+0x6C)`, body through
  `0x8001A0D8` then `0x8001A43C`'s left/right/top/bottom quads; quad `0x8001C7FC` → `0x8001A0D8(component
  +0x6C)`. `0x800243A0` walks the string in s2 (one past the glyph at each leaf call), drawing a glyph
  through `0x8002992C` (return `0x80024688`) or `0x80029D28` (`0x800245F4`, `0x80024640`). `0x800248A0`
  draws three digits, hundreds first. BOOT `0x800798A4` draws HUD icons from 8-byte records in s0
  (`0x800996E0`, `0x8009AD54`, four each) and per-player digits from s4 = `0x8009AD74` + 8 * player
  (units first); BOOT `0x800809A0(items, y)` walks 24-byte item records in s4. Every string address
  `0x800243A0` drew over both flow replays held one text. Producers keyed 99.97% of both replays with 0
  duplicate keys (issue 0036).
- where: `game/render/ui_producer.{h,cpp}`
- gap: BOOT's direct `0x8001A6D4` calls (`0x80082608`, `0x8008267C`) and the projected text path
  `0x800246D0` are unkeyed and unobserved in the replays.

### presentation.cuts — Scene identity for cuts

- status: in-progress
- deps: flow.menu-cross
- evidence: The process state `0x8005B648`, the mode-selecting scene record `0x8009F658` and the menu
  scene record `0x8009F8A4` each change exactly at a scene enter, which places camera and objects
  afresh; `FrameCut` declares those frames cuts.
- where: `game/frame/frame_cut.{h,cpp}`
- gap: none for object reuse; a re-initialised render component keys as a new object
  (`graphics.producers`, issue 0037).
