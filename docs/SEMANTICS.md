# Ravel semantics

This document defines what Ravel measures and how. Code, tests and reports must agree with it. If they disagree, either the code is wrong or this document is changed first, in its own commit, with the reason.

Scope: one instrument, one sequenced source stream, one process. Every run evaluates exactly one consumer policy against the reference.

## 1. Units

| Quantity | Internal type | Export |
|---|---|---|
| Price | Integer ticks (`int64`); the tick size is recorded per dataset | Integer ticks, in columns named `*_ticks` |
| Quantity | Integer lots (`int64`) | Integer lots |
| Time | Simulated nanoseconds since the start of the run (`int64`) | Integer nanoseconds in columns named `*_ns`. Human-facing reports and the viewer show milliseconds and label the unit. |
| Sequence | Source sequence number (`uint64`), starting at 1 and contiguous within a session | Integer |

Simulated time is never mixed with wall-clock time. Wall-clock measurements exist only in the benchmark harness (milestone 5) and never appear in run outputs.

## 2. Entities

**Source event.** Sequence `s`, source time `T(s)`, kind (add, execute, cancel), order ID, side, price, quantity. Source times are non-decreasing in `s`. Several events may share a source time (one incoming order can produce several executions and an add).

**Reference.** An evaluator-only book that applies every source event, intact and in sequence order. Consumers never read it.

**Delivery.** One arrival of one source event at the consumer: `(s, occurrence, delivery time)`. The occurrence number is 1 for the original transmission and 2, 3, ... for duplicates. Without faults, delivery time = `T(s) + L`, where `L` is the scenario's normal transit latency. A dropped event has no delivery. A delayed event has a later delivery time. Reordering is the result of delays.

**Retransmission.** An arrival of a source event caused by a consumer request. It has its own request time and arrival time, and it can itself be lost.

**Snapshot.** The source book (every resting order with ID, side, price, remaining quantity and FIFO position) after the last source event with time `<=` the capture time. It records the last included sequence `S`, the capture time and the delivery time. A snapshot can be older than events the consumer has already buffered; buffered events after `S` complete it.

**Heartbeat.** A source message that carries `next_seq`: one more than the highest sequence with source time `<=` the heartbeat's emission time. See section 9.

**Consumer.** A policy (naive, gap-aware buffering, retransmission request, or snapshot plus replay). It sees only deliveries, repairs and heartbeats that have reached it by the current time.

## 3. Scheduler and total order

All runs use one event queue. Every scheduled item has a key, and items are processed in increasing key order:

1. **Time** (ns).
2. **Event class**, ranked:

   | Rank | Class | Effect |
   |---:|---|---|
   | 0 | `SOURCE` | The reference applies source event `s` |
   | 1 | `DELIVERY` | A feed delivery (original, delayed or duplicate) reaches the consumer |
   | 2 | `RETRANSMISSION` | A retransmitted event reaches the consumer |
   | 3 | `SNAPSHOT` | A snapshot reaches the consumer |
   | 4 | `HEARTBEAT` | A heartbeat reaches the consumer |
   | 5 | `TIMER` | A policy timer fires (for example, a retransmission deadline) |

3. **Sequence:** the event's sequence; for a snapshot, its last included sequence `S`; for a heartbeat, its `next_seq`; for a timer, the sequence it concerns.
4. **Occurrence:** 1, 2, ... for repeated items that have the same time, class and sequence.

Keys are unique by construction. If two items get the same key, the scheduler reports an error instead of choosing between them.

Why this order:

- `SOURCE` first, so the reference at time `t` includes every source event with time `<= t`, and a zero-latency delivery is never processed before its source event.
- Data (deliveries, retransmissions, snapshots) before heartbeats, so a heartbeat is checked against everything that arrived at the same instant. A heartbeat that arrives together with the event it announces must not open a false gap.
- Timers last, so a deadline that coincides with the arrival it was waiting for sees that arrival.

Exact timestamp ties are covered by tests (milestone 1).

**Settled state.** Several items can share a timestamp. Only the state after the last item at a timestamp lasts for a positive duration. Intermediate states inside a timestamp appear as rows in the trace, but they last zero time, so they never contribute to any duration or moment (section 6).

## 4. Consumer state and publication status

At every time `t` a consumer has:

- `applied` (`A`): the highest sequence `a` such that the consumer's book reflects every sequence up to `a`. For the gap-aware policies, this is the last contiguously applied sequence. The naive policy applies whatever arrives, so for it `A` is the highest sequence it has applied, and its book can still lack earlier events.
- `observed` (`H`): the highest sequence the consumer has evidence of, from any delivery, repair, or heartbeat (`next_seq - 1`).
- `book`: its reconstructed order book.
- `status`, exactly one of:

| Status | Meaning |
|---|---|
| `WITHHELD` | The consumer knows its book may be stale and deliberately publishes nothing. For the gap-aware policies this holds while `H > A`, or while a repair is required (for example, after buffer overflow). |
| `PUBLISHING` | Not withheld, and the book has both a best bid and a best ask. The consumer publishes bid, ask and spread. |
| `ONE_SIDED` | Not withheld, and at least one side of the book is empty (this includes an empty book). The consumer publishes the side or sides it has, and spread is undefined. |

The naive policy never enters `WITHHELD`.

`ONE_SIDED` and `WITHHELD` are never merged. An undefined spread on a valid book is a correct publication. A withheld spread is a decision not to publish.

## 5. Reference signal and the four states

The **reference at time `t`** is the reference book after every source event with time `<= t`. Its **signal** is (best bid, best ask), and either may be absent. Spread is derived: `ask - bid` when both exist, otherwise undefined.

The consumer's **published signal** at `t` is its own (best bid, best ask), when its status is `PUBLISHING` or `ONE_SIDED`.

Two signals are equal when their best bids are equal (both absent, or the same price) and their best asks are equal. Equal signals imply equal spreads. Best-level quantities are not part of the signal.

At every instant the consumer is in exactly one of four states:

| State | Condition |
|---|---|
| `WITHHELD` | status is `WITHHELD` |
| `WRONG` | not withheld, and the published signal differs from the reference signal at `t` |
| `LATENT` | not withheld, the published signal equals the reference signal, but the consumer's full book differs from the reference book at `t` |
| `CORRECT` | not withheld, and the full book equals the reference book at `t` |

The four durations add up to the observation window. Withheld time never counts as wrong.

`LATENT` is "book diverged but signal still correct". For example, a lost cancel deep in the bid side leaves the spread right, but the next execution that reaches that level will publish a wrong price.

Book equality compares, at every price level on both sides, the sequence of (order ID, remaining quantity) in FIFO order. This requires a FIFO-aware book (issue #2).

**Error magnitude** is tracked separately from the state:

- spread error in ticks, `|consumer spread - reference spread|`, when both spreads are defined
- its maximum, and its integral over time in tick·ns (an integer)
- time where exactly one of the two spreads is defined, reported as its own duration

## 6. Intervals and durations

Every trace row is a change point. The state set by a row holds over `[t_i, t_{i+1})`, where `t_{i+1}` is the time of the next row. The last interval ends at the observation end `E`.

- The observation window is `[0, E)`. Items with time `>= E` are not processed; how many there were is reported.
- Every duration is a sum of interval lengths in integer nanoseconds. Durations are never derived from row counts.
- Rows with the same time give zero-length intervals, so intermediate states inside a timestamp contribute nothing.
- A **moment** (first wrong output, publication resumed, and so on) is the start of the first *positive-length* interval in that state. A zero-length blip is not a publication.

## 7. Losses, gaps and the three moments

A sequence `s` is **missing at time `t`** if no delivery or retransmission of `s` has reached the consumer at or before `t`, but evidence of a later sequence has (a delivery or repair of some sequence `> s`, or a heartbeat with `next_seq > s`).

- `s` is **lost** if its original delivery was dropped. It may still arrive by retransmission or be covered by a snapshot.
- `s` is **late** if its original delivery was delayed. It may cause a temporary, self-healing gap.

For each missing sequence there are three moments, which can differ:

1. **Signal becomes wrong**, `w(s)`: the start of the first positive-length `WRONG` interval at or after `T(s)`, before trust is restored. It may never happen: a lost deep-book update can leave the signal correct, which is then `LATENT` time.
2. **Gap detectable**, `d(s)`: the earliest settled time at which `s` is missing. This is a property of the delivery schedule, identical for every policy. A gap-aware policy enters `WITHHELD` at `d(s)`. The naive policy ignores the evidence; its summary still reports `d(s)` and says that it did not act on it.
3. **Publication trusted again**, `r(s)`: the first settled time `>= d(s)` at which the consumer leaves `WITHHELD` with `s` applied (or covered by a snapshot). This is not defined for the naive policy, which never withholds.

Derived values:

- **Detection delay** = `d(s) - T(s)`
- **Recovery time** = `r(s) - d(s)`

If several gaps overlap, these moments are still reported for each sequence, but wrong and withheld durations are reported only as run totals. They are never split between overlapping losses.

**Unresolved cases** are labelled explicitly, never dropped and never given a fake time:

- `UNDETECTED_AT_END`: `s` was lost and no evidence of it reached the consumer before `E`.
- `UNRESOLVED_AT_END`: the gap was detected but trust was not restored before `E`.

## 8. Reconstruction versus timeliness

These two questions are measured separately:

- **Reconstruction:** is the consumer's state right for what it claims? Whenever a repair completes (the consumer leaves `WITHHELD`, or installs a snapshot), compare its full book with the reference book after sequence `A`: every order ID, side, price, remaining quantity and FIFO position. Every repair is checked. A mismatch is a reconstruction failure: it is counted, recorded in the trace, and fails the test suite.
- **Timeliness:** how far behind the source is the consumer? **Sequence lag** at `t` = (highest source sequence with time `<= t`) `- A`.

A consumer can be perfectly reconstructed and still lag, which is ordinary transit latency. It can also be current but wrong, like the naive policy after a loss.

## 9. Heartbeat

The heartbeat is optional. Its interval `I` is set per scenario (`0` means off).

- The source emits heartbeat `k` at time `k·I` for `k = 1, 2, ...` while `k·I < E`. It carries `next_seq = 1 + `(the highest sequence with source time `<= k·I`).
- The heartbeat arrives at `k·I + L` and is processed after data items with the same timestamp (section 3).
- On arrival, `H = max(H, next_seq - 1)`. If this makes `H > A`, the gap is detected.

A lost final event has no later sequence to reveal it. With heartbeats off, it must be reported as `UNDETECTED_AT_END`. With heartbeats on, it is detected at the first heartbeat arrival that announces it.

In milestone 1, heartbeats are never faulted. If a later milestone allows heartbeat loss, this section is updated first.

## 10. Control runs

Every faulted scenario has a **control run**: the same source, the same policy, the same normal transit latency `L`, the same observation end, and no faults.

With `L > 0`, even the control run spends some time `WRONG` or `LATENT`, because consumers lag the source by `L`. Summaries report each metric for the faulted run, the control run and the difference. The difference is the effect of the fault.

## 11. Worked example: the canonical timed scenario

Zero transit latency (`L = 0`). The sequence-6 cancel of the last bid at 100 has source time 10 ms and is dropped. The next delivered sequence arrives at 13 ms. A snapshot completes recovery at 25 ms. The exact source times of every event are listed in `docs/HAND_CHECK.md` (milestone 1).

Gap-aware consumer:

| Interval (ms) | Status | Reference spread | Published | State |
|---|---|---:|---:|---|
| `[0, 10)` | per book | as reference | as reference | `CORRECT` |
| `[10, 13)` | `PUBLISHING` | 4 | 3 | `WRONG` (the loss is invisible: `H = A = 5`) |
| `[13, 25)` | `WITHHELD` | 4 | none | `WITHHELD` (`H > A`) |
| `[25, E)` | `PUBLISHING` | 4 | 4 | `CORRECT` (snapshot plus buffered events; reconstruction checked) |

So: `w(6) = 10 ms`, `d(6) = 13 ms`, `r(6) = 25 ms`. Detection delay = 3 ms, recovery time = 12 ms, wrong time = 3 ms, withheld time = 12 ms.

Naive consumer: `WRONG` over `[10, E)`, because it keeps the cancelled bid at 100 and publishes spread 3 against a reference of 4. `d(6) = 13 ms` is reported but not acted on, and `r(6)` is not defined.

Control (no fault, `L = 0`): both consumers are `CORRECT` for all of `[0, E)`. The fault's effect therefore equals the faulted run's numbers.

## 12. Output determinism

Running the same scenario twice must produce byte-identical files.

- No wall-clock times, hostnames or absolute paths in run outputs. The manifest records the git commit, compiler, flags, scenario hash and delivery-schedule hash.
- Rows and keys are written in a defined order (scheduler order, or sorted keys). Iteration order of unordered containers never reaches an output.
- Integers wherever possible. Where a non-integer must be exported, its formatting is fixed and documented next to the column.
- Randomness comes only from Ravel's own seeded PRNG (issue #3).

## 13. Deferred decisions

These are recorded so they are decided deliberately, not by accident:

- How the naive policy handles a mutation for an order it never saw (#4). Decided in milestone 2.
- The visible outcome of buffer overflow (#1). Decided in milestone 3.
- What happens to a snapshot older than `A` (#5). Ignored and counted is planned; decided in milestone 3.
- Faulted heartbeats, if needed. Decided in milestone 2 or 3.
