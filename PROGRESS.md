# Ravel progress ledger

The project's single source of truth for status. Updated at the end of every milestone and whenever a bug is found.

Status words: **VERIFIED** (command output proves it), **IMPLEMENTED** (written, not yet proven), **BLOCKED**, **FAILED**.

## Phase

Milestone 1: timed replay.

## Completed

| Item | Status | Evidence |
|---|---|---|
| Prototype imported unchanged (commit `dbd67cd`) | VERIFIED | 7 files, pushed to `main` |
| `make test` passes on macOS | VERIFIED | Both `PASS` lines; no warnings with `-Wall -Wextra -pedantic` |
| `make demo` matches the golden trace | VERIFIED | `diff -u fixtures/expected.csv timeline.csv` printed nothing |
| Prototype code review | VERIFIED | Five bugs found, filed as #1–#5 |
| Milestone 0: repository setup (#6) | VERIFIED | PR #7 merged as merge commit `74ebce4`; `git status` clean after build |

## In progress

- #8 `docs/SEMANTICS.md`: the semantics, written before any code

## Next

Milestone 1, remaining issues, one per reviewable unit:

2. CMake + CTest with Debug, Release and ASan+UBSan configurations; `make test` and `make demo` kept as thin wrappers
3. Test check macro with file and line reporting; our own seeded PRNG and shuffle (fixes #3)
4. Timestamped event model and FIFO-aware book (fixes #2)
5. Replay clock and scheduler with a documented total order, plus timestamp-tie tests
6. Strict JSON scenario parser (nlohmann/json via FetchContent)
7. Trace, summary and manifest export with interval metrics
8. Canonical timed scenario (loss at 10 ms, detection at 13 ms, recovery at 25 ms), golden test and `docs/HAND_CHECK.md`
9. Determinism test: run the scenario twice and byte-compare the outputs

## Known bugs

| Issue | Summary | Planned fix |
|---|---|---|
| #1 | `Receiver` silently drops out-of-order events when its 4,096-entry buffer is full; the consumer then stays withheld with no visible cause | Milestone 3 |
| #2 | `Book` keys orders by ID and does not record FIFO order within a price level | Milestone 1 |
| #3 | The test's `std::shuffle` calls consume an implementation-defined number of draws, so the 10,000-order workload differs between libc++ and libstdc++ | Milestone 1 |
| #4 | The naive consumer's `Book::apply` throws on execute or cancel of an order it never saw | Milestone 2 |
| #5 | `Receiver::restore` throws on a snapshot older than `applied`; overlapping repairs need it ignored and counted | Milestone 3 |

## Decisions

| Decision | Why |
|---|---|
| C++20, one instrument, one process | Keeps the experiment about feed recovery, not infrastructure |
| CMake + CTest as the real build; `make` targets kept as wrappers | Multiple configurations (Debug, Release, ASan+UBSan) and CI, without breaking the original commands |
| Dependencies through FetchContent pinned to a release tag; nlohmann/json only | Reproducible builds with nothing to install by hand |
| Integer ticks, integer quantities, int64 nanoseconds (exported as labelled ms) | Exact comparisons and byte-identical outputs; no floating-point drift |
| Our own seeded PRNG, not `std::` distributions | Standard-library distributions and `std::shuffle` differ across implementations (see #3) |
| Strict JSON scenario files | A typo in a scenario must fail loudly, not silently change an experiment |
| `runs/` is generated and ignored; committed results go in `results/` | Runs are reproducible from scenarios, and only published results need to be versioned |
| LOBSTER for historical data, with simulated delivery sequences labelled as simulated | LOBSTER has no packet boundaries or transport sequences, so we make no packet-loss claims about the real feed |
| Work flows issue, branch, small commits, PR, merge commit (squash and rebase disabled) | Each building commit stays on `main` with its reasoning; PRs stay reviewable |

## Test status

| Suite | Last result | Where |
|---|---|---|
| `make test` (matcher oracle, 10,000 submissions; receiver cases; 1,000 shuffled duplicate deliveries) | PASS | Apple M3, Apple clang 17.0.0, macOS (Darwin 24.5.0) |
| Demo golden trace (`fixtures/expected.csv`) | IDENTICAL | Same machine |

## Last verified state

- 2026-11-13: commit `dbd67cd` on Apple M3, Apple clang 17.0.0 (clang-1700.0.13.5), arm64-apple-darwin24.5.0, GNU Make 3.81, with `-std=c++20 -O2 -Wall -Wextra -pedantic`. Tests pass, the demo output is byte-identical to the golden trace, and there were no compiler warnings.
