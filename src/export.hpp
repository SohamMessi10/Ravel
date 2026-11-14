#pragma once
#include "replay.hpp"
#include <string>
#include <vector>

namespace ravel {
const char* to_string(Policy p);
const char* to_string(EventClass c);
const char* to_string(Status s);
const char* to_string(State s);
const char* to_string(GapOutcome o);

// Exact text of each run output. Integers only, a fixed column and key order, no wall-clock
// data, so the same inputs always give the same bytes.
std::string trace_csv(const std::vector<TraceRow>& rows);
std::string deliveries_csv(const std::vector<Delivery>& deliveries);
// The faulted run, its no-fault control (same latency), and the run minus control.
std::string summary_json(const Scenario& scenario, const Summary& run, const Summary& control);

struct BuildInfo {
    std::string git_commit, compiler, build_type, flags;
};
std::string manifest_json(const BuildInfo& build, const std::string& scenario_file, std::uint64_t scenario_hash,
                          std::uint64_t input_hash, std::uint64_t delivery_schedule_hash);
}
