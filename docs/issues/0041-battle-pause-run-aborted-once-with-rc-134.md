# 0041 — a Battle Crate Crush pause run aborted once (rc 134) and left no log

**Status:** open, no evidence · **State:** S006

Observed once during the issue 0039 runs: Battle Crate Crush, fps60 on, 4:3, `run 9100`, `pause open` and the page walk, the
process ended with rc 134 (SIGABRT) at about f7644 (before the pause was armed). A rerun of the same command did not abort.

Evidence retained: none. `scratch/boot-menu/battle_on/run.log` and `stdout.log` are the rerun (a clean run-end ledger, 0
fallback blocks, no `error` or abort line, producer census 14,457,937 primitives keyed 14,455,657, 2,279 unkeyed
attributed, 1 span-miss); the aborting run's log was overwritten by it.

Next: rerun the battle replay with a fresh `PSXPORT_LOG_FILE` per run and keep the rc, so an abort leaves its last log lines
(the product's refusal route logs the reason before it aborts; a run with no such line is a host fault).
