#include "../src/scenario.hpp"
#include "check.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <iostream>
using namespace ravel;
using nlohmann::json;

namespace {
json valid() {
    return json::parse(R"({
        "schema_version": 1,
        "name": "canonical_lost_cancel",
        "input": {"path": "fixtures/canonical.txt", "fnv1a64": "0123456789abcdef"},
        "seed": 7,
        "transit_latency_ns": 0,
        "faults": [{"type": "drop", "seq": 6}],
        "policies": ["naive", "snapshot_replay"],
        "retransmit_delay_ns": 0,
        "snapshot_delay_ns": 12000000,
        "buffer_capacity": 4096,
        "heartbeat_interval_ns": 0,
        "observation_end_ns": 40000000
    })");
}

// Applies one edit to the valid scenario and checks that parsing fails with a message containing `expected`.
bool rejects(const std::function<void(json&)>& edit, const std::string& expected) {
    json j = valid();
    edit(j);
    try {
        parse_scenario(j.dump());
    } catch (const ScenarioError& e) {
        if (std::string(e.what()).find(expected) != std::string::npos) return true;
        std::cerr << "unexpected message: " << e.what() << '\n';
    }
    return false;
}

bool rejects_text(const std::string& text, const std::string& expected) {
    try {
        parse_scenario(text);
    } catch (const ScenarioError& e) {
        return std::string(e.what()).find(expected) != std::string::npos;
    }
    return false;
}
}

int main() try {
    {
        const Scenario s = parse_scenario(valid().dump());
        CHECK(s.name == "canonical_lost_cancel" && s.input_path == "fixtures/canonical.txt");
        CHECK(s.input_fnv1a64 == 0x0123456789abcdefu && s.seed == 7);
        CHECK(s.transit_latency == 0 && s.snapshot_delay == 12'000'000 && s.observation_end == 40'000'000);
        CHECK(s.faults == std::vector<Fault>{{FaultType::drop, 6}});
        CHECK(s.policies == std::vector<Policy>{Policy::naive, Policy::snapshot_replay});
        CHECK(s.buffer_capacity == 4096 && s.heartbeat_interval == 0 && s.retransmit_delay == 0);
    }
    // Structure: unknown and missing keys at every level.
    CHECK(rejects([](json& j) { j["latency_ns"] = 0; }, "unknown key 'latency_ns'"));
    CHECK(rejects([](json& j) { j.erase("seed"); }, "missing key 'seed'"));
    CHECK(rejects([](json& j) { j["input"]["sha"] = "x"; }, "scenario.input: unknown key 'sha'"));
    CHECK(rejects([](json& j) { j["input"].erase("fnv1a64"); }, "missing key 'fnv1a64'"));
    CHECK(rejects([](json& j) { j["faults"][0]["delay_ns"] = 1; }, "faults[0]: unknown key 'delay_ns'"));
    CHECK(rejects([](json& j) { j["faults"][0].erase("seq"); }, "faults[0]: missing key 'seq'"));
    CHECK(rejects([](json& j) { j = json::array(); }, "expected an object"));
    // Types: floats, booleans, strings and negatives are not integers here.
    CHECK(rejects([](json& j) { j["transit_latency_ns"] = 1.5; }, "transit_latency_ns: expected an integer"));
    CHECK(rejects([](json& j) { j["transit_latency_ns"] = 1.0; }, "transit_latency_ns: expected an integer"));
    CHECK(rejects([](json& j) { j["snapshot_delay_ns"] = "12000000"; }, "snapshot_delay_ns: expected an integer"));
    CHECK(rejects([](json& j) { j["observation_end_ns"] = true; }, "observation_end_ns: expected an integer"));
    CHECK(rejects([](json& j) { j["transit_latency_ns"] = -1; }, "transit_latency_ns: must be at least 0"));
    CHECK(rejects([](json& j) { j["observation_end_ns"] = 0; }, "observation_end_ns: must be at least 1"));
    CHECK(rejects([](json& j) { j["retransmit_delay_ns"] = 9223372036854775808u; }, "retransmit_delay_ns: out of range"));
    CHECK(rejects([](json& j) { j["seed"] = -1; }, "seed: expected a non-negative integer"));
    CHECK(rejects([](json& j) { j["buffer_capacity"] = 0; }, "buffer_capacity: must be at least 1"));
    CHECK(rejects([](json& j) { j["name"] = 3; }, "name: expected a string"));
    // Values.
    CHECK(rejects([](json& j) { j["schema_version"] = 2; }, "only version 1 is supported"));
    CHECK(rejects([](json& j) { j["name"] = "Bad Name"; }, "name: must match"));
    CHECK(rejects([](json& j) { j["input"]["path"] = ""; }, "input.path: must not be empty"));
    CHECK(rejects([](json& j) { j["input"]["fnv1a64"] = "0123456789ABCDEF"; }, "expected 16 lowercase hex digits"));
    CHECK(rejects([](json& j) { j["input"]["fnv1a64"] = "0123"; }, "expected 16 lowercase hex digits"));
    CHECK(rejects([](json& j) { j["faults"][0]["type"] = "reorder"; }, "faults[0].type: unknown fault type"));
    CHECK(rejects([](json& j) { j["faults"][0]["seq"] = 0; }, "faults[0].seq: must be at least 1"));
    CHECK(rejects([](json& j) { j["faults"].push_back(j["faults"][0]); }, "faults[1]: duplicate fault"));
    CHECK(rejects([](json& j) { j["faults"] = json::object(); }, "faults: expected an array"));
    CHECK(rejects([](json& j) { j["policies"] = json::array(); }, "policies: expected a non-empty array"));
    CHECK(rejects([](json& j) { j["policies"][1] = "fast"; }, "policies[1]: unknown policy 'fast'"));
    CHECK(rejects([](json& j) { j["policies"][1] = "naive"; }, "policies[1]: duplicate policy"));
    // Malformed JSON, comments and trailing commas are parse errors.
    CHECK(rejects_text("{", "scenario: json:"));
    CHECK(rejects_text(R"({"schema_version": 1,})", "scenario: json:"));
    CHECK(rejects_text("// comment\n{}", "scenario: json:"));
    // Faults may be empty (control runs).
    {
        json j = valid();
        j["faults"] = json::array();
        CHECK(parse_scenario(j.dump()).faults.empty());
    }
    std::cout << "PASS: strict scenario parsing (valid file, unknown and missing keys, types, ranges, values, malformed JSON)\n";
} catch (const std::exception& e) {std::cerr << "FAIL: " << e.what() << '\n'; return 1;}
