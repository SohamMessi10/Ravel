#include "../src/scheduler.hpp"
#include "../src/random.hpp"
#include "check.hpp"
#include <iostream>
#include <vector>
using namespace ravel;

namespace {
bool throws_logic_error(auto&& action) {
    try { action(); } catch (const std::logic_error&) { return true; }
    return false;
}
}

int main() try {
    // Exact timestamp tie: every class at the same nanosecond, inserted in reverse rank order.
    {
        Scheduler<int> s;
        const Time t = 10'000'000;
        s.schedule({t, EventClass::timer, 1, 1}, 5);
        s.schedule({t, EventClass::heartbeat, 1, 1}, 4);
        s.schedule({t, EventClass::snapshot, 1, 1}, 3);
        s.schedule({t, EventClass::retransmission, 1, 1}, 2);
        s.schedule({t, EventClass::delivery, 1, 1}, 1);
        s.schedule({t, EventClass::source, 1, 1}, 0);
        for (int expected = 0; expected <= 5; ++expected) CHECK(s.pop().second == expected);
        CHECK(s.empty());
    }
    // Time beats class; at equal time and class, sequence decides; then occurrence.
    {
        Scheduler<int> s;
        s.schedule({2, EventClass::source, 9, 1}, 3);
        s.schedule({1, EventClass::timer, 9, 1}, 0);
        s.schedule({2, EventClass::delivery, 4, 2}, 5);
        s.schedule({2, EventClass::delivery, 4, 1}, 4);
        s.schedule({2, EventClass::delivery, 3, 7}, 6);
        CHECK(s.pop().second == 0);
        CHECK(s.pop().second == 3);
        CHECK(s.pop().second == 6);
        CHECK(s.pop().second == 4);
        CHECK(s.pop().second == 5);
    }
    // A duplicate key is rejected and leaves the queue unchanged.
    {
        Scheduler<int> s;
        s.schedule({5, EventClass::delivery, 7, 1}, 1);
        CHECK(throws_logic_error([&] { s.schedule({5, EventClass::delivery, 7, 1}, 2); }));
        CHECK(s.size() == 1 && s.pop().second == 1);
    }
    // Processing never goes backwards: after popping a key, only strictly greater keys are accepted.
    {
        Scheduler<int> s;
        s.schedule({10, EventClass::delivery, 7, 1}, 1);
        CHECK(s.pop().first == (ScheduleKey{10, EventClass::delivery, 7, 1}));
        CHECK(throws_logic_error([&] { s.schedule({10, EventClass::delivery, 7, 1}, 2); }));
        CHECK(throws_logic_error([&] { s.schedule({10, EventClass::source, 8, 1}, 2); }));
        CHECK(throws_logic_error([&] { s.schedule({9, EventClass::timer, 1, 1}, 2); }));
        s.schedule({10, EventClass::retransmission, 6, 1}, 3);
        s.schedule({10, EventClass::delivery, 7, 2}, 4);
        CHECK(s.pop().second == 4 && s.pop().second == 3 && s.empty());
        CHECK(throws_logic_error([&] { s.pop(); }));
        CHECK(throws_logic_error([&] { s.next_key(); }));
    }
    // Insertion order never matters. The expected order comes from nested loops in
    // (time, class, sequence, occurrence) order, not from ScheduleKey's comparison.
    {
        std::vector<ScheduleKey> expected;
        for (Time t = 0; t < 4; ++t)
            for (int c = 0; c <= 5; ++c)
                for (Seq q = 1; q <= 3; ++q)
                    for (std::uint32_t o = 1; o <= 2; ++o)
                        expected.push_back({t, static_cast<EventClass>(c), q, o});
        for (std::uint64_t seed = 1; seed <= 20; ++seed) {
            auto keys = expected;
            SplitMix64 rng(seed);
            ravel::shuffle(keys.begin(), keys.end(), rng);
            Scheduler<std::size_t> s;
            for (std::size_t i = 0; i < keys.size(); ++i) s.schedule(keys[i], i);
            std::vector<ScheduleKey> popped;
            while (!s.empty()) {
                const auto [key, index] = s.pop();
                CHECK(keys[index] == key);
                popped.push_back(key);
            }
            CHECK(popped == expected);
        }
    }
    std::cout << "PASS: scheduler total order, exact timestamp ties, duplicate and backwards keys rejected, insertion order irrelevant\n";
} catch (const std::exception& e) {std::cerr << "FAIL: " << e.what() << '\n'; return 1;}
