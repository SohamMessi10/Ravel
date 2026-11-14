#include "../src/export.hpp"
#include "../src/input.hpp"
#include "check.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace ravel;
using nlohmann::json;

namespace {
bool rejects(const std::string& text, const std::string& expected) {
    try { parse_commands(text); } catch (const std::runtime_error& e) {
        return std::string(e.what()).find(expected) != std::string::npos;
    }
    return false;
}
}

int main() try {
    // FNV-1a 64-bit test vectors (RFC 9923 / draft-eastlake-fnv test code).
    CHECK(fnv1a64("") == 0xcbf29ce484222325u);
    CHECK(fnv1a64("a") == 0xaf63dc4c8601ec8cu);
    CHECK(fnv1a64("foobar") == 0x85944171f73967e8u);
    CHECK(hex64(0x0123456789abcdefu) == "0123456789abcdef" && hex64(0) == "0000000000000000");

    // Timed commands: times are stamped on events; every error names its line.
    const std::string text = "1000000 B 1 99 10\n2000000 S 2 103 5\n";
    const auto tape = parse_commands(text);
    CHECK(tape.size() == 2 && tape[0].time == 1'000'000 && tape[1].time == 2'000'000 && tape[1].order.price == 103);
    CHECK(rejects("1 X 1 99 10\n", "line 1: unknown command"));
    CHECK(rejects("1 B 1 99 10\n2 B 2 99\n", "line 2: expected <price_ticks> <qty>"));
    CHECK(rejects("1 C 1 7\n", "line 1: unexpected field '7'"));
    CHECK(rejects("5 B 1 99 10\n4 B 2 99 10\n", "line 2: source time went backwards"));
    CHECK(rejects("1 B 1 99 10\n2 B 1 99 10\n", "line 2: invalid limit order or reused ID"));
    CHECK(rejects("B 1 99 10\n", "line 1: expected <time_ns>"));

    // Exact CSV text for a two-order run with no faults and zero latency.
    const Scenario sc{"tiny", "inline", 0, 0, 0, {}, {Policy::naive}, 0, 0, 16, 0, 3'000'000};
    const RunResult run = run_replay(tape, sc, Policy::naive);
    CHECK(trace_csv(run.trace) ==
          "time_ns,class,seq,occurrence,source_seq,ref_bid_ticks,ref_ask_ticks,status,bid_ticks,ask_ticks,"
          "applied,observed,state,action\n"
          "1000000,source,1,1,1,99,,ONE_SIDED,,,0,0,WRONG,source\n"
          "1000000,delivery,1,1,1,99,,ONE_SIDED,99,,1,1,CORRECT,applied\n"
          "2000000,source,2,1,2,99,103,ONE_SIDED,99,,1,1,WRONG,source\n"
          "2000000,delivery,2,1,2,99,103,PUBLISHING,99,103,2,2,CORRECT,applied\n");
    CHECK(deliveries_csv(run.deliveries) == "seq,occurrence,delivery_time_ns\n1,1,1000000\n2,1,2000000\n");

    // Summary: run, control and their difference; absent moments are null, not invented.
    Scenario faulted = sc;
    faulted.faults = {{FaultType::drop, 1}};
    const Summary lossy = run_replay(tape, faulted, Policy::naive).summary;
    const json s = json::parse(summary_json(faulted, lossy, run.summary));
    CHECK(s.at("policy") == "naive" && s.at("units").at("time") == "ns");
    CHECK(s.at("control").at("wrong_ns") == 0 && s.at("control").at("first_wrong_ns").is_null());
    // Losing the 99 bid: wrong over [1, 2) ms (nothing yet) and [2, 3) ms (only the 103 ask).
    CHECK(s.at("run").at("wrong_ns") == 2'000'000 && s.at("run").at("first_wrong_ns") == 1'000'000);
    CHECK(s.at("effect_vs_control").at("wrong_ns") == 2'000'000);
    CHECK(s.at("effect_vs_control").at("correct_ns") == -2'000'000);
    const json& gap = s.at("run").at("gaps").at(0);
    CHECK(gap.at("seq") == 1 && gap.at("detected_ns") == 2'000'000 && gap.at("detection_delay_ns") == 1'000'000);
    CHECK(gap.at("trusted_ns").is_null() && gap.at("recovery_time_ns").is_null() && gap.at("outcome") == "NOT_ACTED_ON");
    CHECK(summary_json(faulted, lossy, run.summary) == summary_json(faulted, lossy, run.summary));

    const json m = json::parse(manifest_json({"abc123", "AppleClang 17", "Debug", "-g"}, "scenarios/x.json",
                                             1, 2, 0x10));
    CHECK(m.at("git_commit") == "abc123" && m.at("scenario_fnv1a64") == "0000000000000001");
    CHECK(m.at("delivery_schedule_fnv1a64") == "0000000000000010" && m.at("format_version") == 1);
    std::cout << "PASS: FNV-1a vectors, timed command parsing, exact CSV text, summary and manifest JSON\n";
} catch (const std::exception& e) {std::cerr << "FAIL: " << e.what() << '\n'; return 1;}
