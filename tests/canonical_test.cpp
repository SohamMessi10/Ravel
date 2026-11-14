#include "../src/input.hpp"
#include "../src/replay.hpp"
#include "../src/scenario.hpp"
#include "check.hpp"
#include <algorithm>
#include <iostream>
using namespace ravel;

// The hand-calculated acceptance case from docs/HAND_CHECK.md. argv[1] is the repository root.
int main(int argc, char** argv) try {
    if (argc != 2) throw std::invalid_argument("usage: canonical-test <repo_root>");
    const std::string root = argv[1];
    const Scenario sc = load_scenario(root + "/scenarios/canonical_lost_cancel.json");
    const std::string input = read_file(root + "/" + sc.input_path);
    CHECK(fnv1a64(input) == sc.input_fnv1a64);
    const std::vector<Event> tape = parse_commands(input);
    CHECK(tape.size() == 30 && tape[5].kind == Kind::cancel && tape[5].order.price == 100);
    CHECK(tape[5].time == 10'000'000 && tape[6].time == 13'000'000);
    constexpr Time ms = 1'000'000;

    {
        const RunResult r = run_replay(tape, sc, Policy::snapshot_replay);
        const Summary& m = r.summary;
        CHECK(m.wrong_ns == 3 * ms && m.withheld_ns == 12 * ms && m.correct_ns == 35 * ms && m.latent_ns == 0);
        const GapReport& g = m.gaps.at(0);
        CHECK(g.seq == 6 && g.wrong == 10 * ms && g.detected == 13 * ms && g.trusted == 25 * ms);
        CHECK(g.outcome == GapOutcome::resolved);
        CHECK(m.reconstruction_checks == 1 && m.reconstruction_failures == 0);
        CHECK(m.max_spread_error_ticks == 1 && m.spread_error_tick_ns == 3 * ms);
        // The snapshot was captured at detection (13 ms), so it covers sequences 1-7; 8-11 are replayed.
        const auto snap = std::find_if(r.trace.begin(), r.trace.end(),
                                       [](const TraceRow& row) { return row.cls == EventClass::snapshot; });
        CHECK(snap != r.trace.end() && snap->time == 25 * ms && snap->seq == 7 && snap->applied == 11);
        CHECK(r.trace.back().applied == 30 && r.trace.back().state == State::correct);
    }
    {
        const Summary m = run_replay(tape, sc, Policy::naive).summary;
        CHECK(m.wrong_ns == 40 * ms && m.correct_ns == 10 * ms && m.withheld_ns == 0);
        CHECK(m.gaps.at(0).outcome == GapOutcome::not_acted_on && m.gaps.at(0).detected == 13 * ms);
    }
    {
        const Summary m = run_replay(tape, sc, Policy::gap_aware).summary;
        CHECK(m.wrong_ns == 3 * ms && m.withheld_ns == 37 * ms && m.correct_ns == 10 * ms);
        CHECK(m.gaps.at(0).outcome == GapOutcome::unresolved_at_end);
    }
    Scenario control = sc;
    control.faults.clear();
    for (Policy p : sc.policies) CHECK(run_replay(tape, control, p).summary.correct_ns == 50 * ms);
    std::cout << "PASS: canonical lost cancel matches docs/HAND_CHECK.md for every policy and the control\n";
} catch (const std::exception& e) {std::cerr << "FAIL: " << e.what() << '\n'; return 1;}
