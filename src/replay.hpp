#pragma once
#include "engine.hpp"
#include "scenario.hpp"
#include "scheduler.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ravel {
// Publication status and the four exclusive states of docs/SEMANTICS.md sections 4 and 5.
enum class Status { publishing, one_sided, withheld };
enum class State { correct, latent, wrong, withheld };

// One feed arrival. Dropped events have no delivery.
struct Delivery {
    Seq seq;
    std::uint32_t occurrence;
    Time time;
    bool operator==(const Delivery&) const = default;
};

// One row per processed scheduled item, in scheduler order. The row's state holds from
// its time until the next row's time (or the observation end).
struct TraceRow {
    Time time;
    EventClass cls;
    Seq seq;
    std::uint32_t occurrence;
    Seq source_seq;                     // highest sequence applied to the reference
    std::optional<Price> ref_bid, ref_ask;
    Status status;
    std::optional<Price> bid, ask;      // published values; empty while withheld
    Seq applied, observed;
    State state;
    std::string action;
    bool operator==(const TraceRow&) const = default;
};

enum class GapOutcome { resolved, undetected_at_end, unresolved_at_end, not_acted_on };

// The three moments for one dropped sequence (docs/SEMANTICS.md section 7).
struct GapReport {
    Seq seq;
    Time source_time;
    std::optional<Time> wrong, detected, trusted;
    GapOutcome outcome;
    bool operator==(const GapReport&) const = default;
};

struct Summary {
    Policy policy;
    Time observation_end;
    Time correct_ns = 0, latent_ns = 0, wrong_ns = 0, withheld_ns = 0;
    Time publishing_ns = 0, one_sided_ns = 0;
    std::optional<Time> first_wrong;
    Price max_spread_error_ticks = 0;
    std::int64_t spread_error_tick_ns = 0;
    Time spread_defined_mismatch_ns = 0; // exactly one of consumer and reference spread is defined
    std::vector<GapReport> gaps;
    std::uint64_t reconstruction_checks = 0, reconstruction_failures = 0, unprocessed_items = 0;
    bool operator==(const Summary&) const = default;
};

struct RunResult {
    std::vector<Delivery> deliveries;
    std::vector<TraceRow> trace;
    Summary summary;
};

// The delivery schedule: every event not dropped arrives once at source time + transit latency.
std::vector<Delivery> build_deliveries(const std::vector<Event>& tape, const Scenario& scenario);

// Replays the tape against one policy. Heartbeats and the retransmit policy are not implemented yet.
RunResult run_replay(const std::vector<Event>& tape, const Scenario& scenario, Policy policy);
}
