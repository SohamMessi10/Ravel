# Ravel
A C++20 market replay and recovery engine. How long can a missing feed update leave an apparently valid spread wrong, and when can a consumer trust it again?

## Run on macOS or Linux
Requires CMake 3.25 or later, a C++20 compiler and Make. From this directory, run `make test`, then `make demo`. To save machine-readable output: `make ravel && ./ravel fixtures/session.txt > timeline.csv`.

The `make` targets are thin wrappers around CMake presets (`debug`, `release`, `asan`). Choose one with `PRESET`, for example `make test PRESET=asan` for AddressSanitizer and UndefinedBehaviorSanitizer. Or use CMake directly: `cmake --preset release && cmake --build --preset release && ctest --preset release`.

## Experiment
One instrument, integer ticks and lots. A limit-order matcher consumes buys, sells and full cancellations. Marketable orders execute against resting orders at maker prices in price/time order. Unfilled quantity rests. IDs are session-unique; incoming quantities must be positive and at most one million lots.

The supplied commands produce 30 sequenced book events. An intact consumer processes every event. Two other consumers miss sequence 6, a cancellation of the last bid at 100. The naive consumer publishes its last known spread. The sequence-aware consumer stops publishing once a later sequence exposes the gap. At sequence 10 it restores an intact order-ID snapshot through 8 and replays buffered events 9 and 10. Snapshots here represent a feed consumer’s order state, not exchange restart images.

| Sequence | Reference spread | Naive spread | Sequence-aware output |
|---:|---:|---:|---|
| 5 | 3 | 3 | 3 |
| 6 | 4 | 3 | 3 (loss still invisible) |
| 7–9 | 4 | 3 | withheld |
| 10–30 | 4 | 3 | 4 |

These are source-event positions in a synthetic fixture. They are not milliseconds or observed exchange results. A silent loss at the end of a stream needs a heartbeat or another sequence to expose it. The naive path models this specific lost cancellation; it can reject incompatible later mutations on other traces.

## Validation
An independent linear-scan matcher checks every execution, residual order and resulting book across 10,000 seeded submissions. Separate tests cover same-price FIFO, price priority, partial fills, cancellation, duplicate delivery, reordered delivery and snapshot catch-up. The receiver retains up to 4,096 pending events; unresolved holes suppress publication until repaired. This release uses an in-process transport and one documented recovery scenario.

## Next experiment
Implement a historical feed adapter with a small legally redistributable fixture, separate source sequence from simulated delivery time, and sweep loss patterns and snapshot delays. Compare spread error duration, stale time, recovery correctness, and p99 processing time on a named machine. The matcher produces controlled truth for experiments; historical order-book reconstruction should consume published exchange events directly.
