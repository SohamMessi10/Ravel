#include "../src/replay.hpp"
#include "check.hpp"
#include <iostream>
using namespace ravel;

namespace {
constexpr Time ms(Time v) { return v * 1'000'000; }

// Bids 99 then 100, ask 103; the 100 bid is cancelled at 10 ms (sequence 4);
// then a deep bid at 13 ms and a deep ask at 15 ms.
std::vector<Event> tape() {
    Exchange ex;
    std::vector<Event> t;
    auto add = [&](const std::vector<Event>& events) { t.insert(t.end(), events.begin(), events.end()); };
    add(ex.submit({1, Side::buy, 99, 10}, ms(1)));
    add(ex.submit({2, Side::buy, 100, 5}, ms(2)));
    add(ex.submit({3, Side::sell, 103, 8}, ms(3)));
    t.push_back(ex.cancel(2, ms(10)));
    add(ex.submit({4, Side::buy, 98, 9}, ms(13)));
    add(ex.submit({5, Side::sell, 105, 7}, ms(15)));
    return t;
}

Scenario scenario(std::vector<Fault> faults, Time latency, Time end) {
    return {"test", "inline", 0, 0, latency, std::move(faults), {Policy::naive},
            0, ms(12), 4096, 0, end};
}

// The four states always partition the observation window.
Summary run(const Scenario& s, Policy p) {
    const RunResult r = run_replay(tape(), s, p);
    const Summary& m = r.summary;
    CHECK(m.correct_ns + m.latent_ns + m.wrong_ns + m.withheld_ns == s.observation_end);
    CHECK(m.publishing_ns + m.one_sided_ns + m.withheld_ns == s.observation_end);
    CHECK(m.reconstruction_failures == 0);
    return m;
}
}

int main() try {
    CHECK(tape().size() == 6 && tape()[3].kind == Kind::cancel && tape()[3].time == ms(10));
    const Scenario lost_cancel = scenario({{FaultType::drop, 4}}, 0, ms(40));

    // Snapshot + replay: wrong [10, 13), withheld [13, 25), correct otherwise.
    {
        const Summary m = run(lost_cancel, Policy::snapshot_replay);
        CHECK(m.wrong_ns == ms(3) && m.withheld_ns == ms(12) && m.correct_ns == ms(25) && m.latent_ns == 0);
        CHECK(m.first_wrong == ms(10));
        CHECK(m.gaps.size() == 1);
        const GapReport& g = m.gaps[0];
        CHECK(g.seq == 4 && g.source_time == ms(10) && g.wrong == ms(10) && g.detected == ms(13) && g.trusted == ms(25));
        CHECK(g.outcome == GapOutcome::resolved);
        CHECK(m.reconstruction_checks == 1);
        CHECK(m.max_spread_error_ticks == 1 && m.spread_error_tick_ns == ms(3));
        CHECK(m.one_sided_ns == ms(3) && m.publishing_ns == ms(25));
    }
    // Naive: wrong from 10 ms to the end; detectable at 13 ms but not acted on.
    {
        const Summary m = run(lost_cancel, Policy::naive);
        CHECK(m.wrong_ns == ms(30) && m.correct_ns == ms(10) && m.withheld_ns == 0 && m.latent_ns == 0);
        CHECK(m.gaps[0].detected == ms(13) && !m.gaps[0].trusted && m.gaps[0].wrong == ms(10));
        CHECK(m.gaps[0].outcome == GapOutcome::not_acted_on);
        CHECK(m.spread_error_tick_ns == ms(30));
    }
    // Gap-aware buffering without repair: withheld from detection to the end.
    {
        const Summary m = run(lost_cancel, Policy::gap_aware);
        CHECK(m.wrong_ns == ms(3) && m.withheld_ns == ms(27) && m.correct_ns == ms(10));
        CHECK(m.gaps[0].outcome == GapOutcome::unresolved_at_end && !m.gaps[0].trusted);
    }
    // Losing the deep 99 bid: naive is wrong [1, 2), latent [2, 10) and wrong again from 10 ms.
    {
        const Summary m = run(scenario({{FaultType::drop, 1}}, 0, ms(40)), Policy::naive);
        CHECK(m.correct_ns == ms(1) && m.wrong_ns == ms(31) && m.latent_ns == ms(8));
        CHECK(m.gaps[0].detected == ms(2) && m.gaps[0].wrong == ms(1));
    }
    // No-fault controls. Zero latency: always correct. 1 ms latency: each top-of-book change
    // (1, 2, 3, 10 ms) is wrong for 1 ms and each deep change (13, 15 ms) is latent for 1 ms.
    for (Policy p : {Policy::naive, Policy::gap_aware, Policy::snapshot_replay}) {
        CHECK(run(scenario({}, 0, ms(40)), p).correct_ns == ms(40));
        const Summary m = run(scenario({}, ms(1), ms(40)), p);
        CHECK(m.wrong_ns == ms(4) && m.latent_ns == ms(2) && m.correct_ns == ms(34) && m.withheld_ns == 0);
    }
    // Observation end before the revealing delivery: the loss is undetected, later items unprocessed.
    {
        const Summary m = run(scenario({{FaultType::drop, 4}}, 0, ms(12)), Policy::snapshot_replay);
        CHECK(m.gaps[0].outcome == GapOutcome::undetected_at_end && !m.gaps[0].detected);
        CHECK(m.wrong_ns == ms(2) && m.unprocessed_items == 4);
    }
    // Determinism: the same inputs give identical deliveries, traces and summaries.
    {
        const RunResult a = run_replay(tape(), lost_cancel, Policy::snapshot_replay);
        const RunResult b = run_replay(tape(), lost_cancel, Policy::snapshot_replay);
        CHECK(a.deliveries == b.deliveries && a.trace == b.trace && a.summary == b.summary);
        CHECK(a.deliveries.size() == 5);
    }
    // Not implemented yet: refused explicitly instead of producing a wrong result.
    {
        bool threw = false;
        try { run_replay(tape(), lost_cancel, Policy::retransmit); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
        Scenario hb = lost_cancel;
        hb.heartbeat_interval = ms(5);
        threw = false;
        try { run_replay(tape(), hb, Policy::naive); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
    }
    std::cout << "PASS: replay intervals, three moments per loss, latent divergence, controls, observation end, determinism\n";
} catch (const std::exception& e) {std::cerr << "FAIL: " << e.what() << '\n'; return 1;}
