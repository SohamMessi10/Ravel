# Ravel — Market Data Replay & Recovery Engine

Project brief for Claude Code · November 13, 2026

## Research question

When a consumer misses a market-data update, how long does it publish a wrong trading signal, when can it detect the problem, and what evidence justifies resuming publication?

Ravel combines a C++20 price-time matching engine with deterministic feed replay. The matcher supplies controlled ground truth. The replay experiment measures what a downstream consumer actually saw. The design draws inspiration from [lockstep](https://github.com/Aneesh495/lockstep) and [AegisFeed](https://github.com/MohitManna-2006/AegisFeed). Implement independently and use Ravel’s own fixtures, tests, and results.

## Starting point

A starter package named ravel.zip already exists. Read its README, Makefile, src/engine.hpp, src/main.cpp, tests/test.cpp, fixtures/session.txt, and fixtures/expected.csv before changing code. From the unpacked project directory:

~~~sh
make test
make demo
make ravel
./ravel fixtures/session.txt > timeline.csv
diff -u fixtures/expected.csv timeline.csv
~~~

The prototype compiled and ran on Linux. Its tests compared 10,000 seeded order submissions against an independent linear-scan matcher, event by event, and exercised 1,000 shuffled deliveries with duplicates. These counts measure correctness coverage. Mac verification, latency benchmarks, and historical-feed validation are still pending.

### Implemented

- One instrument, integer price ticks and quantities, session-unique order IDs. Resting orders execute at the maker’s price in price/time order. Partial fills, residual orders, and full cancellation are supported.
- Sequenced add, execute, and cancel events. A single incoming order may produce several executions followed by an add for its remaining quantity.
- An intact reference book, a naive consumer, and a gap-aware receiver. The receiver buffers out-of-order events, advances only through contiguous sequences, ignores applied duplicates, and supports snapshot installation and catch-up. The pending buffer is bounded at 4,096 events.
- A scripted 30-event scenario and CSV output. Loss and snapshot timing are currently hard-coded in the demo. The naive path handles this fixture; behavior for incompatible later mutations must be defined before broader fault sweeps.

## Canonical failure

The best bid is 100, the next bid is 99, and the best ask is 103. A cancellation removes the last order at 100. The correct spread moves from 3 to 4 ticks. A consumer that misses the cancellation can still show a plausible spread of 3.

| Source sequence | Reference spread | Naive output | Gap-aware output |
|---|---:|---:|---|
| 5 | 3 | 3 | 3 |
| 6: cancellation lost | 4 | 3 | 3; loss is invisible |
| 7–9: later sequences arrive | 4 | 3 | Withheld; later events buffered |
| 10: snapshot through 8 arrives | 4 | 3 | 4 after applying buffered 9 and 10 |
| 11–30 | 4 | 3 | 4; matches the reference |

The first wrong output, first detectable gap, and recovery are three different events. A lost deep-book update can leave the spread correct while the book is wrong. A final missing message can remain undetected without a heartbeat or a later sequence.

## Runtime design

1. Select an instrument, source, fault schedule, recovery policy, and observation end time.
2. Generate synthetic events from order commands or decode historical exchange messages directly. Historical executions must retain their recorded meaning.
3. Apply the intact events to an independent reference book. Use an evaluator with access to ground truth; consumers see only deliveries and repairs available at the current time.
4. Schedule original deliveries, losses, delays, duplicates, retransmissions, and snapshots on a deterministic simulated clock. Specify tie-breaking for events with identical timestamps.
5. Run all policies against the same inputs and fault schedule. Observe every transition, including source changes that consumers have not yet received.
6. Compare recovered state against ground truth at the same sequence. Separately compare the signal published at a given time against the source signal then current.
7. Export a trace, run summary, and reproducibility metadata. The browser viewer reads that same trace.

## Data contracts

| Record | Required fields |
|---|---|
| Synthetic command | Input position, command/order ID, instrument, side, limit in ticks, quantity, cancellation target as applicable |
| Reference event | Session/stream ID, sequence, source timestamp, instrument, event type, resting order ID, side, price and quantity |
| Delivery | Event identity, delivery timestamp, occurrence number, fault/scenario ID; dropped events have no delivery |
| Snapshot | Session, instrument, last included sequence, capture time, delivery time, resting order state sufficient for future updates |
| Scenario | Source identifier or checksum, seed, fault rules, normal delivery latency, repair delays, policy, buffer capacity, observation end |
| Trace row | Evaluation time, policy, last applied/highest observed sequence, freshness, bid, ask, spread, recovery action |
| Run summary | Wrong-signal time, withheld time, detection and repair delays, error magnitude, unresolved gaps, recovered-state equality |

Use integer units internally and label them in exports. Preserve the difference between an undefined spread on a one-sided book and a deliberately withheld spread. Document dataset provenance, tick size, clock units, session boundaries, supported messages, and redistribution terms. If the historical source lacks original packet boundaries or transport sequences, label injected delivery sequences as simulated.

## Milestones and acceptance tests

| Stage | Deliverable | Acceptance |
|---|---|---|
| 0. Reproduce baseline | Starter engine and demo running on the user’s Mac | Tests pass and CSV matches the supplied golden trace |
| 1. Timed replay | Deterministic scheduler, scenario file, timed trace and interval metrics | Identical inputs yield byte-identical traces; the hand-calculated scenario below passes |
| 2. Fault scenarios | Configurable single/burst loss, delays, duplicates, reordering and seeded fault generation | Include a harmless duplicate, self-healing reordering, lost best-bid cancel, lost deep-book update, and undetectable final loss |
| 3. Recovery policies | Compare naive publication, contiguous buffering, retransmission and snapshot-plus-replay | Verify all orders and remaining quantities after repair. Exercise old snapshots, overlapping repairs, buffer overflow and end-of-run unresolved gaps |
| 4. Historical adapter | One documented feed format, one redistributable test fixture and a larger user-supplied input path | Verify known book states or agreement with an independent decoder. Count unsupported events and separate recorded from simulated fields |
| 5. Performance study | Reproducible release-mode benchmark and one profiled optimization | Publish raw repetitions, hardware, compiler flags, workload size, before/after performance and passing correctness checks |
| 6. Interactive replay | Browser viewer with play/pause, event stepping, scrubbing, scenario and snapshot-delay controls | Every plotted figure and displayed value traces to an exported event or interval |
| 7. Portfolio release | README, architecture diagram, CI, reproduction commands, and results page | Another person can reproduce the canonical experiment from a clean checkout; every public metric links to its run |

### First timed acceptance case

Use a synthetic scenario with zero normal transit delay. Lose the cancellation at 10 ms, deliver the first later sequence at 13 ms, and complete recovery at 25 ms. The gap-aware consumer must publish the wrong spread for 3 ms over [10, 13) and withhold it for 12 ms over [13, 25). Verify exact order state at the recovered sequence. These timestamps are fixture inputs, not measured market results.

## Measurement and demonstration

- Compute wrong-signal and withheld durations from intervals between changes, including the final interval through the observation end. Track numerical spread error separately from book inequality.
- Measure detection delay from the lost source event to evidence of loss, and recovery delay from detection to trustworthy publication. Explicitly label undetected and unresolved cases.
- Compare every fault run against a no-fault control with the same normal transit latency. Report publication availability alongside error duration: silence and confidently publishing a wrong quote are different outcomes.
- Benchmark events/second, memory use, processing latency percentiles, snapshot installation and catch-up on named hardware. Document sample counts and timer overhead. Simulated outage durations and wall-clock processing latency are separate measurements.
- The final demo shows reference, naive and gap-aware books side by side. Mark first wrong output, gap detection, snapshot arrival and publication resumption. Let the visitor vary snapshot delay while the input tape and missing cancellation stay fixed.

## Handoff

Start with milestone 0, then build milestone 1. Keep the baseline reproducible and show the exact files, commands and expected checks for each change. The user runs Terminal and Git commands and reviews changes before publishing. Preserve existing authorship and omit Claude co-author and session trailers from drafted commits. Treat planned features and proposed experiment values as plans until verified.
