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

| Milestone 1.4: timestamped int64 event model and FIFO-aware book (#14, fixes #2) | VERIFIED | Each commit passes 3/3 in all presets on its own; demo trace `IDENTICAL`; making `Book::operator==` ignore queue order made `engine_tests` fail at the FIFO check (`test.cpp:54`) |

| Milestone 1.5: replay scheduler with the documented total order (#16) | VERIFIED | 4/4 CTest tests pass in all presets; demo trace `IDENTICAL`; swapping the snapshot and heartbeat ranks made `scheduler_tests` fail at the exact-tie check (`scheduler_test.cpp:26`) |

| Milestone 1.6: strict JSON scenario parser with nlohmann/json v3.12.0 (#18) | VERIFIED | Archive SHA-256 matched the published value; 5/5 CTest tests in all presets with no warnings; renaming the unknown-key message made `scenario_tests` fail; demo trace `IDENTICAL` |

| Milestone 1.7: replay loop with interval metrics, in memory (#21) | VERIFIED | 6/6 CTest tests in all presets; hand-calculated durations match; making latent states count as correct made `replay_tests` fail at the latent check (`replay_test.cpp:74`); demo trace `IDENTICAL` |

| Milestone 1.8a: timed command input, run outputs and the `ravel-run` CLI (#23) | VERIFIED | 7/7 CTest tests in all presets; changing the FNV prime made `export_tests` fail; demo trace `IDENTICAL` |

| Milestone 1.8b: canonical lost-cancel scenario, `docs/HAND_CHECK.md`, hand-check and golden tests (#25) | VERIFIED | Snapshot + replay is wrong for exactly 3 ms and withheld for exactly 12 ms; naive is wrong for 40 ms (to the observation end); trace rows 10–25 ms reviewed against the hand check; 9/9 CTest tests in all presets; appending a line to a golden file made the golden test fail |

## In progress

- Nothing open. Next is milestone 1.9, the last unit of milestone 1.

## Open verification items

- The PR #11 description says commit `0c522bd` was tested on its own (`make test` 1/1), but that output was never pasted. This is not verified until it is run: `git switch --detach 0c522bd && make clean && make test`, then `git switch main`.

## Next

Milestone 1, remaining issues, one per reviewable unit:

9. Determinism test: run the scenario twice and byte-compare the outputs

## Known bugs

| Issue | Summary | Planned fix |
|---|---|---|
| #1 | `Receiver` silently drops out-of-order events when its 4,096-entry buffer is full; the consumer then stays withheld with no visible cause | Milestone 3 |
| #4 | The naive consumer's `Book::apply` throws on execute or cancel of an order it never saw | Milestone 2 |
| #5 | `Receiver::restore` throws on a snapshot older than `applied`; overlapping repairs need it ignored and counted | Milestone 3 |
| #19 | The scenario parser does not reject duplicate JSON keys; nlohmann/json's `parse()` docs do not say how duplicates are handled | Before milestone 2 fault scenarios |

Fixed:

| Issue | Summary | Fixed by |
|---|---|---|
| #3 | The seeded test workload depended on `std::shuffle`'s implementation-defined draw count | Milestone 1.3: `ravel::SplitMix64` and `ravel::shuffle` (#12) |
| #2 | `Book` keyed orders by ID and ignored FIFO order within a price level | Milestone 1.4: per-level FIFO ladders and queue-order equality (#14) |

## Verified facts

Checked against official docs on 2026-11-13. Reuse these instead of re-checking; re-verify only on a tool upgrade.

| Fact | Source |
|---|---|
| `gh repo create/view/edit`, `gh issue create --title --label --body-file`, `gh pr create --base --head --title --body-file` (passing `--head` for a pushed branch skips the push prompt), `gh pr merge <n> --merge --delete-branch`; `gh repo edit --enable-squash-merge=false` style toggles | https://cli.github.com/manual/ |
| Presets schema 6 needs CMake 3.25; schema 13 needs 4.5. `generator` is optional (schema 3 and later). Test presets: `output.outputOnFailure`, `execution.noTestsAction` | https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html |
| CMake 4.0 removed compatibility with versions below 3.5; 3.31 deprecates below 3.10 | https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html |
| `add_test(NAME … COMMAND <target>)` replaces a target name with the built executable's path | https://cmake.org/cmake/help/latest/command/add_test.html |
| `cmake -E compare_files` exits 0 if the files are the same, 1 if they differ, 2 on invalid arguments; in `cmake -D… -P script`, `-D` must come before `-P` | https://cmake.org/cmake/help/latest/manual/cmake.1.html |
| `execute_process` `OUTPUT_FILE` attaches the last command's stdout; `RESULT_VARIABLE` holds its exit code | https://cmake.org/cmake/help/latest/command/execute_process.html |
| ASan: `-fsanitize=address` at compile and link; `-fno-omit-frame-pointer` gives better stack traces | https://clang.llvm.org/docs/AddressSanitizer.html |
| UBSan continues after a report by default; `-fno-sanitize-recover=…` makes it exit | https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html |
| splitmix64: increment `0x9e3779b97f4a7c15`, shifts 30/27/31, multipliers `0xbf58476d1ce4e5b9` and `0x94d049bb133111eb`; public domain | https://prng.di.unimi.it/splitmix64.c |
| splitmix64, seed 1234567, first five outputs: 6457827717110365317, 3203168211198807973, 9817491932198370423, 4593380528125082431, 16408922859458223821 | https://rosettacode.org/wiki/Pseudo-random_numbers/Splitmix64 |
| Homebrew `cmake` 4.5.0 has an arm64 Sequoia bottle | https://formulae.brew.sh/formula/cmake |
| FNV-1a 64: offset basis `0xcbf29ce484222325`, prime `0x100000001b3`, xor then multiply. Test vectors "" → `cbf29ce484222325`, "a" → `af63dc4c8601ec8c`, "foobar" → `85944171f73967e8` (the vectors were read from a summary; `export_tests` passes against them) | https://datatracker.ietf.org/doc/html/draft-eastlake-fnv (RFC 9923) |
| `configure_file(<in> <out> @ONLY)` replaces only `@VAR@`, rewrites the output only when its content changes, and treats a relative output path as relative to `CMAKE_CURRENT_BINARY_DIR` | https://cmake.org/cmake/help/latest/command/configure_file.html |
| nlohmann/json latest stable is v3.12.0 (2025-04-11); `json.tar.xz` SHA-256 is `42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa` | https://github.com/nlohmann/json/releases |
| nlohmann/json FetchContent: `FetchContent_Declare(json URL …/v3.12.0/json.tar.xz)`, `FetchContent_MakeAvailable(json)`, link `nlohmann_json::nlohmann_json`; the archive has no tests, so they are not built | https://json.nlohmann.me/integration/cmake/ |
| `FetchContent_MakeAvailable` added in 3.14; the `SYSTEM` keyword in 3.25; `EXCLUDE_FROM_ALL` in 3.28 | https://cmake.org/cmake/help/latest/module/FetchContent.html |
| `json::parse` throws `json::parse_error` on invalid input; comments are rejected unless `ignore_comments` is set. The page does not say how duplicate keys are handled | https://json.nlohmann.me/api/basic_json/parse/ |
| `is_number_integer()` is true for signed and unsigned integers and false for floats; `is_number_unsigned()` is true only for unsigned; `get<int64_t>()` on a float truncates and does not check range | https://json.nlohmann.me/api/basic_json/is_number_integer/, https://json.nlohmann.me/api/basic_json/get/ |

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
| `Price`, `Qty`, `Time` are `std::int64_t`; `Event` carries source time | Exact integer comparisons, and room for scaled real-feed prices (LOBSTER's scaling gets verified in milestone 4). The exchange rejects decreasing time before changing any state, so a rejected call never consumes a sequence number. |
| The untimed demo fixture stamps every event with time 0 | It has no timestamps; timed scenarios (milestone 1) replace it, and the golden trace does not print time |
| The scheduler is a `std::map` keyed by (time, class, sequence, occurrence) with a defaulted `<=>` | Keys must be unique and totally ordered. A map detects duplicates and gives a defined order; `std::priority_queue` does neither. The struct's member order is the tie-break rule. |
| Scenario schema v1: every field required, times in integer ns, input identified by path plus FNV-1a 64-bit hash, `faults` limited to `drop` for now | Strictness catches typos. FNV-1a detects a changed input file; it is an identity check, not a security hash. |
| nlohmann/json pinned by URL plus `URL_HASH SHA256` and fetched as `SYSTEM`; parsing lives in one static library | A changed or tampered archive fails the configure step; the library's headers do not trip our warnings; only one file includes the JSON headers |
| `snapshot_replay` captures the snapshot from the reference at the moment the gap is detected; it arrives after `snapshot_delay_ns`, and another is requested if a gap remains after installing it | The simplest snapshot service. Delayed and stale snapshots are milestone 3 work. |
| Golden outputs (`fixtures/golden/`) cover `trace.csv`, `deliveries.csv` and `summary.json`, but not `manifest.json` | The manifest records the build, which differs between presets. The golden files were generated once, then checked against `docs/HAND_CHECK.md` and an independent assertion test before being committed. |
| In the canonical timed run, the snapshot covers sequences 1–7 (captured at detection, 13 ms) | The prototype hard-coded "through 8". With timing explicit, capture at detection is the defined behaviour; `docs/HAND_CHECK.md` explains the difference. |
| `ravel-run` takes the git commit as an argument, and resolves scenario paths relative to the current directory (the repository root) | A commit baked in at configure time goes stale after the next commit. Run outputs never contain absolute paths. |
| Every policy's run is paired with a control: the same scenario with the faults cleared. `summary.json` reports run, control and `effect_vs_control` | The brief requires each fault's effect to be measured against a no-fault run with the same latency |
| Reconstruction checks rebuild the reference from sequence 1 up to the consumer's applied sequence | Simple and obviously correct, at O(n) per check. Revisit at LOBSTER scale if profiling shows it matters. |
| Processing never goes backwards: a new key must be greater than the last processed key | Metrics come from change points in processed order, so a late insertion must not rewrite what happened at an earlier key (`SEMANTICS.md` §3) |
| `Book` stores FIFO ladders plus an ID index; equality compares only the ladders | Queue order is part of correctness. The index is derived data, so it is excluded from equality and guarded by an "out of sync" error. |

## Test status

| Suite | Last result | Where |
|---|---|---|
| CTest `engine_tests` (matcher oracle, 10,000 submissions from `SplitMix64(42)` with FIFO ladders checked after each; source time; 64-bit prices; FIFO equality; receiver cases; 1,000 shuffled duplicate deliveries) | PASS in `debug`, `release`, `asan` | Apple M3, Apple clang 17.0.0, macOS (Darwin 24.5.0), CMake 4.5.0 |
| CTest `canonical_tests` (every number in `docs/HAND_CHECK.md`, for each policy and the control) | PASS in `debug`, `release`, `asan` | Same machine |
| CTest `canonical_golden_outputs` (`ravel-run` output byte-identical to the reviewed `fixtures/golden/` files) | PASS in `debug`, `release`, `asan` | Same machine |
| CTest `export_tests` (FNV-1a vectors; timed-command errors with line numbers; exact `trace.csv` and `deliveries.csv` text; summary run, control and effect; manifest fields) | PASS in `debug`, `release`, `asan` | Same machine |
| CTest `replay_tests` (lost cancel under each policy; lost deep bid gives latent time; 0 ms and 1 ms no-fault controls; undetected at observation end; determinism; durations sum to the window) | PASS in `debug`, `release`, `asan` | Same machine |
| CTest `scenario_tests` (valid file plus one negative case per strictness rule) | PASS in `debug`, `release`, `asan` | Same machine |
| CTest `random_tests` (splitmix64 known answers for seeds 1234567 and 0; bounded draws; shuffle) | PASS in `debug`, `release`, `asan` | Same machine |
| CTest `scheduler_tests` (exact ties across all six classes; key precedence; duplicate, backwards and empty errors; 20 seeded shuffles of 144 keys) | PASS in `debug`, `release`, `asan` | Same machine |
| CTest `demo_golden_trace` (byte-compare with `fixtures/expected.csv`) | PASS in `debug`, `release`, `asan` | Same machine |

## Last verified state

- 2026-11-13: branch `replay/metrics` (#21): 6/6 CTest tests in all presets; latent-state mutation caught.
- 2026-11-13: branch `scenario/parser` (#18): 5/5 CTest tests in all presets; nlohmann/json archive hash verified with `shasum -a 256`.
- 2026-11-13: branch `engine/scheduler` (#16): 4/4 CTest tests in all presets; demo trace identical; the rank-swap mutation is caught.
- 2026-11-13: branch `engine/event-model` (#14): 3/3 CTest tests in all presets for each commit; demo trace identical. Checking FIFO ladders after every oracle submission raised `engine_tests` time from 0.8 s to 4.0 s (debug), 2.3 s to 13.7 s (asan) and 0.2 s to 0.5 s (release). That cost is in the test's own checks (rebuilding ID maps and ladders each step), not in the engine.
- 2026-11-13: branch `test/check-and-prng` (#12): 3/3 CTest tests in `debug`, `release` and `asan`, and the shift mutation is caught.
- 2026-11-13: branch `build/cmake` (#10) on the same machine with CMake 4.5.0 (Homebrew). `ctest` passes 2/2 in all three presets; `make test`, `make test PRESET=asan` and the `./ravel` golden diff pass from a clean build.
- 2026-11-13: commit `dbd67cd` on Apple M3, Apple clang 17.0.0 (clang-1700.0.13.5), arm64-apple-darwin24.5.0, GNU Make 3.81, with `-std=c++20 -O2 -Wall -Wextra -pedantic`. Tests pass, the demo output is byte-identical to the golden trace, and there were no compiler warnings.
