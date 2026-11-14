#include "export.hpp"
#include "input.hpp"
#include <nlohmann/json.hpp>
#include <sstream>

namespace ravel {
using nlohmann::json;

const char* to_string(Policy p) {
    switch (p) {
    case Policy::naive: return "naive";
    case Policy::gap_aware: return "gap_aware";
    case Policy::retransmit: return "retransmit";
    case Policy::snapshot_replay: return "snapshot_replay";
    }
    return "?";
}
const char* to_string(EventClass c) {
    switch (c) {
    case EventClass::source: return "source";
    case EventClass::delivery: return "delivery";
    case EventClass::retransmission: return "retransmission";
    case EventClass::snapshot: return "snapshot";
    case EventClass::heartbeat: return "heartbeat";
    case EventClass::timer: return "timer";
    }
    return "?";
}
const char* to_string(Status s) {
    switch (s) {
    case Status::publishing: return "PUBLISHING";
    case Status::one_sided: return "ONE_SIDED";
    case Status::withheld: return "WITHHELD";
    }
    return "?";
}
const char* to_string(State s) {
    switch (s) {
    case State::correct: return "CORRECT";
    case State::latent: return "LATENT";
    case State::wrong: return "WRONG";
    case State::withheld: return "WITHHELD";
    }
    return "?";
}
const char* to_string(GapOutcome o) {
    switch (o) {
    case GapOutcome::resolved: return "RESOLVED";
    case GapOutcome::undetected_at_end: return "UNDETECTED_AT_END";
    case GapOutcome::unresolved_at_end: return "UNRESOLVED_AT_END";
    case GapOutcome::not_acted_on: return "NOT_ACTED_ON";
    }
    return "?";
}

namespace {
std::string opt(const std::optional<Price>& v) { return v ? std::to_string(*v) : ""; }
json opt_json(const std::optional<Time>& v) { return v ? json(*v) : json(nullptr); }

json durations(const Summary& s) {
    return {{"correct_ns", s.correct_ns}, {"latent_ns", s.latent_ns}, {"wrong_ns", s.wrong_ns},
            {"withheld_ns", s.withheld_ns}, {"publishing_ns", s.publishing_ns}, {"one_sided_ns", s.one_sided_ns}};
}

json summary_body(const Summary& s) {
    json j = durations(s);
    j["first_wrong_ns"] = opt_json(s.first_wrong);
    j["max_spread_error_ticks"] = s.max_spread_error_ticks;
    j["spread_error_tick_ns"] = s.spread_error_tick_ns;
    j["spread_defined_mismatch_ns"] = s.spread_defined_mismatch_ns;
    j["reconstruction_checks"] = s.reconstruction_checks;
    j["reconstruction_failures"] = s.reconstruction_failures;
    j["unprocessed_items"] = s.unprocessed_items;
    json gaps = json::array();
    for (const GapReport& g : s.gaps) {
        gaps.push_back({{"seq", g.seq}, {"source_time_ns", g.source_time}, {"wrong_ns", opt_json(g.wrong)},
                        {"detected_ns", opt_json(g.detected)}, {"trusted_ns", opt_json(g.trusted)},
                        {"detection_delay_ns", g.detected ? json(*g.detected - g.source_time) : json(nullptr)},
                        {"recovery_time_ns", g.detected && g.trusted ? json(*g.trusted - *g.detected) : json(nullptr)},
                        {"outcome", to_string(g.outcome)}});
    }
    j["gaps"] = gaps;
    return j;
}
}

std::string trace_csv(const std::vector<TraceRow>& rows) {
    std::ostringstream out;
    out << "time_ns,class,seq,occurrence,source_seq,ref_bid_ticks,ref_ask_ticks,status,bid_ticks,ask_ticks,"
           "applied,observed,state,action\n";
    for (const TraceRow& r : rows)
        out << r.time << ',' << to_string(r.cls) << ',' << r.seq << ',' << r.occurrence << ',' << r.source_seq << ','
            << opt(r.ref_bid) << ',' << opt(r.ref_ask) << ',' << to_string(r.status) << ',' << opt(r.bid) << ','
            << opt(r.ask) << ',' << r.applied << ',' << r.observed << ',' << to_string(r.state) << ',' << r.action
            << '\n';
    return out.str();
}

std::string deliveries_csv(const std::vector<Delivery>& deliveries) {
    std::ostringstream out;
    out << "seq,occurrence,delivery_time_ns\n";
    for (const Delivery& d : deliveries) out << d.seq << ',' << d.occurrence << ',' << d.time << '\n';
    return out.str();
}

std::string summary_json(const Scenario& sc, const Summary& run, const Summary& control) {
    json effect = durations(run);
    const json base = durations(control);
    for (auto& [key, value] : effect.items()) value = value.get<Time>() - base.at(key).get<Time>();
    const json j = {{"scenario", sc.name},
                    {"policy", to_string(run.policy)},
                    {"observation_end_ns", run.observation_end},
                    {"transit_latency_ns", sc.transit_latency},
                    {"units", {{"time", "ns"}, {"price", "ticks"}}},
                    {"run", summary_body(run)},
                    {"control", summary_body(control)},
                    {"effect_vs_control", effect}};
    return j.dump(2) + "\n";
}

std::string manifest_json(const BuildInfo& b, const std::string& scenario_file, std::uint64_t scenario_hash,
                          std::uint64_t input_hash, std::uint64_t delivery_schedule_hash) {
    const json j = {{"format_version", 1},
                    {"git_commit", b.git_commit},
                    {"compiler", b.compiler},
                    {"build_type", b.build_type},
                    {"flags", b.flags},
                    {"scenario_file", scenario_file},
                    {"scenario_fnv1a64", hex64(scenario_hash)},
                    {"input_fnv1a64", hex64(input_hash)},
                    {"delivery_schedule_fnv1a64", hex64(delivery_schedule_hash)}};
    return j.dump(2) + "\n";
}
}
