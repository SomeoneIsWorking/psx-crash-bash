# 0046 — producer saves and renders allocate per draw

**Status:** open: the mesh path is fixed, the 2D leaf path still allocates

## Observed
psxport issue 0194's per-fence count from boot (unpaced, ires 0, fps60 1, sink 3840x2160, `LD_PRELOAD` malloc counter,
fences 1200-2400 differenced): 1592-1830 allocations per fence, nearly all on the guest thread, against 29-85 in most
titles. Reduced sampled stacks: `psxport/scratch/cost-audit/stk-new/crashbash_port/alloc_top.txt` (before) and
`stk-cb2/crashbash_port/alloc_top.txt` (after); both are unsymbolized because those binaries were rebuilt.

## Cause
- Mesh: each draw built a new `FaceCall` (list, vertices, linked faces), a `StateWriter` and a `SavedFace` vector,
  and each render read both states into new vectors plus a sink index and output list per present.
- Leaf: `renderLeafState`, `HostLeafPort::draw`, `ui_producer.cpp`'s leaf and string notes (`PacketCollector::noteLeaf`,
  `BodyScope` rebuild a `UiNote` and its `leaves` per draw; `commit` destroys them), `saveLeafState` and
  `readString`/`readContext` into new vectors.

## Fix (mesh)
`PacketCollector` keeps its scopes, pending scopes and face calls; saves and renders work in the session's
`Kept<FaceScratch>`; the state is saved from its parts; psxport `StateReader::getAll` fills a kept vector. 378
allocations per fence after. `commit` aborts if it runs inside an open producer scope, whose face calls it would
otherwise keep growing.

## Proper fix (leaf)
The mesh path's treatment: notes kept in the collector and cleared, not destroyed, per commit; leaf saves and renders
in a kept scratch.
