#pragma once
#include "engine.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ravel {
// Any problem with a scenario file: malformed JSON, unknown or missing key, wrong type or bad value.
struct ScenarioError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class FaultType { drop };
struct Fault {
    FaultType type;
    Seq seq;
    bool operator==(const Fault&) const = default;
};

enum class Policy { naive, gap_aware, retransmit, snapshot_replay };

// Schema version 1. Every field is required; times are integer nanoseconds.
struct Scenario {
    std::string name;            // [a-z0-9_-]+, used as the runs/<name>/ directory
    std::string input_path;      // source input, relative to the repository root
    std::uint64_t input_fnv1a64; // FNV-1a 64-bit hash of the input file (written as 16 lowercase hex digits)
    std::uint64_t seed;
    Time transit_latency;
    std::vector<Fault> faults;
    std::vector<Policy> policies;
    Time retransmit_delay;
    Time snapshot_delay;
    std::size_t buffer_capacity;
    Time heartbeat_interval;     // 0 = no heartbeat
    Time observation_end;
};

// Strict parse: unknown keys, missing keys, wrong types and out-of-range values throw ScenarioError.
Scenario parse_scenario(std::string_view json_text);
Scenario load_scenario(const std::string& path);
}
