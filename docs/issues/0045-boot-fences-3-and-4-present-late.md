# 0045 — boot fences 3 and 4 present late

**Status:** open · **State:** S016

## Observed
40 s paced hidden runs from boot (ires 0, fps60 1, 3840x2160 sink, `PSXPORT_DEBUG=presentseq`,
`scratch/player-bugs-5/paced40-seq{,2,3}/run.log`, load 0.8-1.5): the two longest present intervals are fences 3 and 4,
in the first 110 ms after the first present: 36.06/39.04, 35.10/34.69 and 34.57/41.41 ms. Every later interval is at
most 33.6 ms (the 30 Hz cadence). Without the log, runs on the same build put the worst interval at 39-55 ms and miss
0 or 1 present (`paced40-fix`, `paced40-review`, pass 4 `scratch/player-bugs-4/paced40`), so a run counts a miss only
when this boot interval crosses the miss threshold.

## Measured
The interval coincides with the first spanning guest calls (`PSXPORT_DEBUG=guest-call`, `paced40-seq`): `0x8008E5BC`
(3 turns, 1.6M guest cycles) and `0x80010394` (4 turns, 1.7M), one commit per one-field turn. psxport `335ea43a` (a
turn is one field, before it half a field) did not move it: 39.31 ms before, 35-41 ms after with presentseq.

## Next
Attribute the host time of those turns (first translation of the boot blocks versus guest work) before any change.
