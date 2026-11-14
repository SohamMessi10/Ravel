#include "replay.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ravel {
namespace {
Book reference_after(const std::vector<Event>& tape, Seq seq) {
    Book book;
    for (Seq s = 1; s <= seq; ++s) book.apply(tape[s - 1]);
    return book;
}

std::optional<Price> spread_of(const std::optional<Price>& bid, const std::optional<Price>& ask) {
    if (!bid || !ask) return {};
    return *ask - *bid;
}

// Durations and moments from the trace's change points (docs/SEMANTICS.md sections 5-7).
void summarize(const std::vector<Event>& tape, const std::vector<Delivery>& deliveries,
               const Scenario& sc, const std::vector<TraceRow>& rows, Summary& sum) {
    const Time end = sc.observation_end;
    // Row i's state holds over [rows[i].time, next row's time), the last one until the end.
    auto length = [&](std::size_t i) { return (i + 1 < rows.size() ? rows[i + 1].time : end) - rows[i].time; };
    const Time start_len = rows.empty() ? end : rows.front().time;
    sum.correct_ns += start_len;  // before the first item both books are empty
    sum.one_sided_ns += start_len;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const TraceRow& r = rows[i];
        const Time len = length(i);
        if (len == 0) continue;  // intermediate state inside one timestamp
        switch (r.state) {
        case State::correct: sum.correct_ns += len; break;
        case State::latent: sum.latent_ns += len; break;
        case State::wrong: sum.wrong_ns += len; if (!sum.first_wrong) sum.first_wrong = r.time; break;
        case State::withheld: sum.withheld_ns += len; break;
        }
        if (r.status == Status::withheld) continue;
        (r.status == Status::publishing ? sum.publishing_ns : sum.one_sided_ns) += len;
        const auto mine = spread_of(r.bid, r.ask), ref = spread_of(r.ref_bid, r.ref_ask);
        if (mine && ref) {
            const Price err = *mine > *ref ? *mine - *ref : *ref - *mine;
            sum.max_spread_error_ticks = std::max(sum.max_spread_error_ticks, err);
            sum.spread_error_tick_ns += err * len;
        } else if (mine.has_value() != ref.has_value()) {
            sum.spread_defined_mismatch_ns += len;
        }
    }
    for (const Fault& f : sc.faults) {
        GapReport g{f.seq, tape[f.seq - 1].time, {}, {}, {}, GapOutcome::undetected_at_end};
        for (const Delivery& d : deliveries)
            if (d.seq > f.seq && d.time < end && (!g.detected || d.time < *g.detected)) g.detected = d.time;
        // Trusted again: first settled row at or after detection that publishes with s applied.
        // The naive policy never withholds, so it is never "trusted again".
        if (g.detected && sum.policy != Policy::naive) {
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const TraceRow& r = rows[i];
                if (length(i) > 0 && r.time >= *g.detected && r.status != Status::withheld && r.applied >= f.seq) {
                    g.trusted = r.time;
                    break;
                }
            }
        }
        const Time wrong_limit = g.trusted ? *g.trusted : end;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const TraceRow& r = rows[i];
            if (length(i) > 0 && r.state == State::wrong && r.time >= g.source_time && r.time < wrong_limit) {
                g.wrong = r.time;
                break;
            }
        }
        if (!g.detected) g.outcome = GapOutcome::undetected_at_end;
        else if (sum.policy == Policy::naive) g.outcome = GapOutcome::not_acted_on;
        else g.outcome = g.trusted ? GapOutcome::resolved : GapOutcome::unresolved_at_end;
        sum.gaps.push_back(g);
    }
}
}

std::vector<Delivery> build_deliveries(const std::vector<Event>& tape, const Scenario& sc) {
    std::vector<Delivery> out;
    for (const Event& e : tape) {
        const bool dropped = std::any_of(sc.faults.begin(), sc.faults.end(),
                                         [&](const Fault& f) { return f.type == FaultType::drop && f.seq == e.seq; });
        if (!dropped) out.push_back({e.seq, 1, e.time + sc.transit_latency});
    }
    return out;
}

RunResult run_replay(const std::vector<Event>& tape, const Scenario& sc, Policy policy) {
    if (sc.heartbeat_interval != 0) throw std::invalid_argument("replay: heartbeats are not implemented yet");
    if (policy == Policy::retransmit) throw std::invalid_argument("replay: the retransmit policy is not implemented yet");
    for (std::size_t i = 0; i < tape.size(); ++i) {
        if (tape[i].seq != i + 1) throw std::invalid_argument("replay: tape sequences must be 1, 2, 3, ...");
        if (i && tape[i].time < tape[i - 1].time) throw std::invalid_argument("replay: tape times must not decrease");
    }
    for (const Fault& f : sc.faults)
        if (f.seq > tape.size()) throw std::invalid_argument("replay: fault refers to a sequence beyond the input");

    RunResult out;
    out.deliveries = build_deliveries(tape, sc);
    struct Item { std::size_t index = 0; std::optional<Snapshot> snapshot; };
    Scheduler<Item> queue;
    for (std::size_t i = 0; i < tape.size(); ++i)
        queue.schedule({tape[i].time, EventClass::source, tape[i].seq, 1}, {i, {}});
    for (const Delivery& d : out.deliveries)
        queue.schedule({d.time, EventClass::delivery, d.seq, d.occurrence}, {d.seq - 1, {}});

    const bool gap_aware = policy != Policy::naive;
    Book reference, naive;
    Receiver receiver(sc.buffer_capacity);
    Seq source_seq = 0, naive_high = 0;
    bool snapshot_pending = false;
    std::uint32_t snapshots = 0;
    Summary& sum = out.summary;
    sum.policy = policy;
    sum.observation_end = sc.observation_end;

    // A snapshot captures the reference as it stands now and arrives after the snapshot delay.
    auto request_snapshot = [&](Time now) {
        snapshot_pending = true;
        ++snapshots;
        queue.schedule({now + sc.snapshot_delay, EventClass::snapshot, source_seq, snapshots},
                       {0, Snapshot{source_seq, reference}});
    };

    while (!queue.empty() && queue.next_key().time < sc.observation_end) {
        auto [key, item] = queue.pop();
        const bool was_withheld = gap_aware && !receiver.fresh();
        std::string action;
        if (key.cls == EventClass::source) {
            reference.apply(tape[item.index]);
            source_seq = tape[item.index].seq;
            action = "source";
        } else if (key.cls == EventClass::delivery && !gap_aware) {
            naive.apply(tape[item.index]);
            naive_high = std::max(naive_high, tape[item.index].seq);
            action = "applied";
        } else if (key.cls == EventClass::delivery) {
            const Seq before = receiver.applied;
            const Event& e = tape[item.index];
            receiver.receive(e);
            action = receiver.applied > before ? "applied" : e.seq <= before ? "duplicate_ignored" : "buffered";
            if (!was_withheld && !receiver.fresh()) action = "gap_detected";
            if (!receiver.fresh() && policy == Policy::snapshot_replay && !snapshot_pending) {
                request_snapshot(key.time);
                action += ";snapshot_requested";
            }
        } else if (key.cls == EventClass::snapshot) {
            receiver.restore(*item.snapshot);
            snapshot_pending = false;
            action = "snapshot_installed";
            if (!receiver.fresh()) {
                request_snapshot(key.time);
                action += ";snapshot_requested";
            }
        } else {
            throw std::logic_error("replay: unexpected event class");
        }

        // A repair completes when the consumer stops withholding or installs a snapshot:
        // its full book must equal the reference after its applied sequence.
        if (gap_aware && receiver.fresh() && (was_withheld || key.cls == EventClass::snapshot)) {
            ++sum.reconstruction_checks;
            if (!(receiver.book == reference_after(tape, receiver.applied))) ++sum.reconstruction_failures;
        }

        const Book& book = gap_aware ? receiver.book : naive;
        TraceRow row{key.time, key.cls, key.seq, key.occurrence, source_seq,
                     reference.best_bid(), reference.best_ask(), Status::withheld, {}, {},
                     gap_aware ? receiver.applied : naive_high,
                     gap_aware ? receiver.observed() : naive_high, State::withheld, action};
        if (!(gap_aware && !receiver.fresh())) {
            row.bid = book.best_bid();
            row.ask = book.best_ask();
            row.status = row.bid && row.ask ? Status::publishing : Status::one_sided;
            if (row.bid != row.ref_bid || row.ask != row.ref_ask) row.state = State::wrong;
            else row.state = book == reference ? State::correct : State::latent;
        }
        out.trace.push_back(std::move(row));
    }
    sum.unprocessed_items = queue.size();
    summarize(tape, out.deliveries, sc, out.trace, sum);
    return out;
}
}
