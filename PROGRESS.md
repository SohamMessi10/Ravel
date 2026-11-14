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
| Milestone 1.1: `docs/SEMANTICS.md` (#8) | VERIFIED | PR #9 merged as merge commit `206ae4c` |
| Milestone 1.2: CMake + CTest build (#10) | VERIFIED | 2/2 CTest tests pass in the `debug`, `release` and `asan` presets; ASan+UBSan flags confirmed in compile and link commands; the golden-test script returns exit 1 on a deliberately wrong file; `make test` and `make test PRESET=asan` pass from a clean build; `./ravel` output identical to `fixtures/expected.csv` |

| Milestone 1.3: `CHECK` macro and splitmix64 PRNG (#12, fixes #3) | VERIFIED | 3/3 CTest tests pass in all presets; a deliberately failing `CHECK` printed file, line and expression and exited 1; mutating one shift (30 to 29) made `random_tests` fail at `random_test.cpp:20`; `grep` finds no `std::` random engines, distributions or `std::shuffle` in `src` or `tests` |

## In progress

- Nothing open. Next is milestone 1.4.

## Open verification items

- The PR #11 description says commit `0c522bd` was tested on its own (`make test` 1/1), but that output was never pasted. This is not verified until it is run: `git switch --detach 0c522bd && make clean && make test`, then `git switch main`.

## Next

Milestone 1, remaining issues, one per reviewable unit:

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
| #4 | The naive consumer's `Book::apply` throws on execute or cancel of an order it never saw | Milestone 2 |
| #5 | `Receiver::restore` throws on a snapshot older than `applied`; overlapping repairs need it ignored and counted | Milestone 3 |

Fixed:

| Issue | Summary | Fixed by |
|---|---|---|
| #3 | The seeded test workload depended on `std::shuffle`'s implementation-defined draw count | Milestone 1.3: `ravel::SplitMix64` and `ravel::shuffle` (#12) |

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
| CMake presets (schema 6, CMake 3.25 or later): `debug`, `release`, `asan`, each building into `build/<preset>` | One command per configuration, locally and in CI; 3.25 is the minimum CMake that reads schema 6 |
| ASan+UBSan uses `-fno-sanitize-recover=undefined` | Otherwise UBSan prints a report and continues, and the test still passes |
| The Release preset uses CMake's default `-O3 -DNDEBUG`; the prototype Makefile used `-O2` | The code has no `assert`s, so `NDEBUG` changes nothing today. Benchmarks (milestone 5) record the exact flags. |
| The demo golden test byte-compares with `cmake -E compare_files`, without `--ignore-eol` | Byte-identical output is the requirement, so line endings count |
| No test framework; a `CHECK` macro plus one CTest test per executable | The prototype's single-binary setup was adequate once failures report file, line and expression. A framework would be a dependency without a current need. |
| splitmix64 as the only PRNG, written from Vigna's public-domain reference | 64-bit state, passes BigCrush, a few lines, and fully specified by unsigned arithmetic. Known-answer tests use values printed by the compiled reference, cross-checked with Rosetta Code. |
| Bounded draws use rejection (threshold `2^64 mod bound`) | `r % bound` favours small values; rejection is unbiased and still deterministic for a seed |

## Test status

| Suite | Last result | Where |
|---|---|---|
| CTest `engine_tests` (matcher oracle, 10,000 submissions from `SplitMix64(42)`; receiver cases; 1,000 shuffled duplicate deliveries) | PASS in `debug`, `release`, `asan` | Apple M3, Apple clang 17.0.0, macOS (Darwin 24.5.0), CMake 4.5.0 |
| CTest `random_tests` (splitmix64 known answers for seeds 1234567 and 0; bounded draws; shuffle) | PASS in `debug`, `release`, `asan` | Same machine |
| CTest `demo_golden_trace` (byte-compare with `fixtures/expected.csv`) | PASS in `debug`, `release`, `asan` | Same machine |

## Last verified state

- 2026-11-13: branch `test/check-and-prng` (#12): 3/3 CTest tests in `debug`, `release` and `asan`, and the shift mutation is caught.
- 2026-11-13: branch `build/cmake` (#10) on the same machine with CMake 4.5.0 (Homebrew). `ctest` passes 2/2 in all three presets; `make test`, `make test PRESET=asan` and the `./ravel` golden diff pass from a clean build.
- 2026-11-13: commit `dbd67cd` on Apple M3, Apple clang 17.0.0 (clang-1700.0.13.5), arm64-apple-darwin24.5.0, GNU Make 3.81, with `-std=c++20 -O2 -Wall -Wextra -pedantic`. Tests pass, the demo output is byte-identical to the golden trace, and there were no compiler warnings.
