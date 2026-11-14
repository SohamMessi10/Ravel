#include "scenario.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>

namespace ravel {
namespace {
using nlohmann::json;

[[noreturn]] void fail(const std::string& where, const std::string& what) {
    throw ScenarioError("scenario: " + where + ": " + what);
}

// The object must have exactly these keys: no extras, none missing.
void require_keys(const json& obj, const std::string& where, std::initializer_list<std::string_view> keys) {
    if (!obj.is_object()) fail(where, "expected an object");
    for (const auto& [key, value] : obj.items())
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) fail(where, "unknown key '" + key + "'");
    for (auto key : keys)
        if (!obj.contains(key)) fail(where, "missing key '" + std::string(key) + "'");
}

// Integers only: floats, booleans and strings are rejected, and so are values outside [min, INT64_MAX].
std::int64_t get_int(const json& obj, const std::string& where, const char* key, std::int64_t min) {
    const json& v = obj.at(key);
    const std::string path = where + "." + key;
    if (!v.is_number_integer()) fail(path, "expected an integer");
    if (v.is_number_unsigned() && v.get<std::uint64_t>() > std::uint64_t(std::numeric_limits<std::int64_t>::max()))
        fail(path, "out of range");
    const auto value = v.get<std::int64_t>();
    if (value < min) fail(path, "must be at least " + std::to_string(min));
    return value;
}

std::uint64_t get_uint(const json& obj, const std::string& where, const char* key, std::uint64_t min) {
    const json& v = obj.at(key);
    const std::string path = where + "." + key;
    if (!v.is_number_unsigned()) fail(path, "expected a non-negative integer");
    const auto value = v.get<std::uint64_t>();
    if (value < min) fail(path, "must be at least " + std::to_string(min));
    return value;
}

std::string get_string(const json& obj, const std::string& where, const char* key) {
    const json& v = obj.at(key);
    if (!v.is_string()) fail(where + "." + key, "expected a string");
    return v.get<std::string>();
}

bool is_name(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

std::uint64_t parse_hex64(const std::string& s, const std::string& where) {
    if (s.size() != 16) fail(where, "expected 16 lowercase hex digits");
    std::uint64_t value = 0;
    for (char c : s) {
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else fail(where, "expected 16 lowercase hex digits");
        value = value << 4 | std::uint64_t(digit);
    }
    return value;
}

Policy parse_policy(const json& v, const std::string& where) {
    if (!v.is_string()) fail(where, "expected a string");
    const auto s = v.get<std::string>();
    if (s == "naive") return Policy::naive;
    if (s == "gap_aware") return Policy::gap_aware;
    if (s == "retransmit") return Policy::retransmit;
    if (s == "snapshot_replay") return Policy::snapshot_replay;
    fail(where, "unknown policy '" + s + "'");
}
}

Scenario parse_scenario(std::string_view json_text) {
    json root;
    try {
        root = json::parse(json_text);
    } catch (const json::parse_error& e) {
        fail("json", e.what());
    }
    const std::string top = "scenario";
    require_keys(root, top, {"schema_version", "name", "input", "seed", "transit_latency_ns", "faults",
                             "policies", "retransmit_delay_ns", "snapshot_delay_ns", "buffer_capacity",
                             "heartbeat_interval_ns", "observation_end_ns"});
    if (get_uint(root, top, "schema_version", 0) != 1) fail(top + ".schema_version", "only version 1 is supported");

    Scenario s;
    s.name = get_string(root, top, "name");
    if (!is_name(s.name)) fail(top + ".name", "must match [a-z0-9_-]+");

    const json& input = root.at("input");
    require_keys(input, top + ".input", {"path", "fnv1a64"});
    s.input_path = get_string(input, top + ".input", "path");
    if (s.input_path.empty()) fail(top + ".input.path", "must not be empty");
    s.input_fnv1a64 = parse_hex64(get_string(input, top + ".input", "fnv1a64"), top + ".input.fnv1a64");

    s.seed = get_uint(root, top, "seed", 0);
    s.transit_latency = get_int(root, top, "transit_latency_ns", 0);

    const json& faults = root.at("faults");
    if (!faults.is_array()) fail(top + ".faults", "expected an array");
    for (std::size_t i = 0; i < faults.size(); ++i) {
        const std::string where = top + ".faults[" + std::to_string(i) + "]";
        require_keys(faults[i], where, {"type", "seq"});
        if (get_string(faults[i], where, "type") != "drop") fail(where + ".type", "unknown fault type");
        Fault f{FaultType::drop, get_uint(faults[i], where, "seq", 1)};
        if (std::find(s.faults.begin(), s.faults.end(), f) != s.faults.end()) fail(where, "duplicate fault");
        s.faults.push_back(f);
    }

    const json& policies = root.at("policies");
    if (!policies.is_array() || policies.empty()) fail(top + ".policies", "expected a non-empty array");
    for (std::size_t i = 0; i < policies.size(); ++i) {
        const std::string where = top + ".policies[" + std::to_string(i) + "]";
        const Policy p = parse_policy(policies[i], where);
        if (std::find(s.policies.begin(), s.policies.end(), p) != s.policies.end()) fail(where, "duplicate policy");
        s.policies.push_back(p);
    }

    s.retransmit_delay = get_int(root, top, "retransmit_delay_ns", 0);
    s.snapshot_delay = get_int(root, top, "snapshot_delay_ns", 0);
    s.buffer_capacity = static_cast<std::size_t>(get_uint(root, top, "buffer_capacity", 1));
    s.heartbeat_interval = get_int(root, top, "heartbeat_interval_ns", 0);
    s.observation_end = get_int(root, top, "observation_end_ns", 1);
    return s;
}

Scenario load_scenario(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw ScenarioError("scenario: cannot open " + path);
    std::ostringstream text;
    text << file.rdbuf();
    return parse_scenario(text.str());
}
}
