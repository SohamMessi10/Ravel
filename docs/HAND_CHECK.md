# Hand check: canonical lost cancel

This is a by-hand calculation of the first timed acceptance case in `docs/BRIEF.md` §6, worked through with the definitions in `docs/SEMANTICS.md`. `tests/canonical_test.cpp` asserts every number below.

- Scenario: `scenarios/canonical_lost_cancel.json`
- Input: `fixtures/canonical.txt` (the prototype's `fixtures/session.txt` commands, with times added)
- Transit latency 0, snapshot delay 12 ms, observation end E = 50 ms, sequence 6 dropped

The times below are fixture inputs, not measured market results.

## Source events (times in ms)

| Time | Command | Sequence | Effect on the reference book |
|---:|---|---|---|
| 1 | B 1 99×10 | 1 add | bid 99, no ask |
| 2 | B 2 100×5 | 2 add | best bid 100 |
| 3 | S 3 103×8 | 3 add | ask 103, so spread 3 |
| 4 | S 4 103×6 | 4 add | queued behind order 3 |
| 5 | B 5 103×3 | 5 execute | order 3 drops to 5 lots; spread still 3 |
| **10** | **C 2** | **6 cancel (dropped)** | **the last order at 100 leaves; best bid 99, spread 4** |
| 13 | B 6 98×9 | 7 add | deep bid; spread 4 |
| 15 | S 7 105×7 | 8 add | deep ask |
| 17 | B 8 103×6 | 9 and 10 execute | order 3 filled (5 lots), order 4 drops to 5 lots; ask stays 103 |
| 20 | S 9 106×4 | 11 add | deep ask |
| 26–44 | 19 commands, 1 ms apart | 12–30 add | bids at 97 and 96, asks at 107; best levels unchanged |

From 10 ms to E, the reference publishes bid 99, ask 103, spread 4.

## Snapshot + replay consumer

1. **[0, 10) ms:** every event is delivered at its source time (zero latency), so the consumer matches the reference. State `CORRECT`, 10 ms. It is `ONE_SIDED` over [0, 3) and `PUBLISHING` from 3 ms.
2. **10 ms:** sequence 6 is never delivered. The consumer has applied 5 and observed 5, so it sees no gap and publishes bid 100, ask 103, spread 3, against a reference spread of 4. **Signal wrong at 10 ms.**
3. **13 ms:** sequence 7 arrives. Applied is 5 and observed is 7, so the gap is visible. **Gap detected at 13 ms.** The consumer withholds publication and requests a snapshot. The snapshot is captured now: the reference after every event with time ≤ 13 ms, which is sequences 1–7. It will arrive 12 ms later.
4. **15–20 ms:** sequences 8–11 arrive and are buffered (applied is still 5).
5. **25 ms:** the snapshot arrives. The consumer installs it (applied 7), then applies buffered 8, 9, 10 and 11 (applied 11 = observed). Its full book equals the reference after sequence 11, which is one reconstruction check with no failures. **Publication trusted again at 25 ms**, with spread 4.
6. **[25, 50) ms:** deliveries keep it equal to the reference. `CORRECT`.

| Interval (ms) | State | Length |
|---|---|---:|
| [0, 10) | CORRECT | 10 ms |
| [10, 13) | WRONG (3 against 4) | **3 ms** |
| [13, 25) | WITHHELD | **12 ms** |
| [25, 50) | CORRECT | 25 ms |
| **Total** | | 50 ms = E |

- Detection delay = 13 − 10 = 3 ms
- Recovery time = 25 − 13 = 12 ms
- Spread error: 1 tick over 3 ms, giving a maximum of 1 tick and an integral of 3,000,000 tick·ns

## Naive consumer

It applies everything it receives and never withholds. It keeps the cancelled 100 bid, so from 10 ms it publishes spread 3 against 4. No later event changes either best level, so it stays wrong until the observation end.

| Interval (ms) | State | Length |
|---|---|---:|
| [0, 10) | CORRECT | 10 ms |
| [10, 50) | WRONG | **40 ms, until E** |

The gap was detectable at 13 ms. The naive consumer did not act on it, so the outcome is `NOT_ACTED_ON` and it has no "trusted again" time.

## Gap-aware consumer without repair

It is identical to snapshot + replay until 13 ms, then it withholds for the rest of the run: wrong 3 ms, withheld 37 ms, correct 10 ms, outcome `UNRESOLVED_AT_END`.

## No-fault control

With the same latency (0) and no faults, every consumer is `CORRECT` for all 50 ms. The fault's effect therefore equals the faulted numbers above.

## Difference from the untimed prototype demo

The prototype demo restored a snapshot "through 8" at sequence 10, because its timing was hard-coded. In the timed run, the snapshot is captured at detection (13 ms), so it covers sequences 1–7, and 8–11 are replayed from the buffer. Both repair the same loss, and both end with a full book equal to the reference.
