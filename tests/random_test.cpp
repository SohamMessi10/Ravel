#include "../src/random.hpp"
#include "check.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>
using namespace ravel;

int main() try {
    // Known answers printed by the compiled reference implementation
    // (https://prng.di.unimi.it/splitmix64.c). The seed 1234567 values also match
    // the published Rosetta Code task "Pseudo-random numbers/Splitmix64".
    {
        SplitMix64 rng(1234567);
        const std::array<std::uint64_t, 5> expected{
            6457827717110365317u, 3203168211198807973u, 9817491932198370423u,
            4593380528125082431u, 16408922859458223821u};
        for (auto value : expected) CHECK(rng.next() == value);
    }
    {
        SplitMix64 rng(0);
        const std::array<std::uint64_t, 3> expected{
            16294208416658607535u, 7960286522194355700u, 487617019471545679u};
        for (auto value : expected) CHECK(rng.next() == value);
    }

    // below(): stays in range, reaches every value of a small bound, rejects 0.
    {
        SplitMix64 rng(7);
        CHECK(rng.below(1) == 0);
        std::array<int, 5> counts{};
        for (int i = 0; i < 10000; i++) {
            const auto v = rng.below(5);
            CHECK(v < 5);
            ++counts[v];
        }
        for (int c : counts) CHECK(c > 0);
        bool threw = false;
        try { rng.below(0); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
        // A bound just above 2^63 rejects about half of all draws, exercising the retry loop.
        const std::uint64_t big = (std::uint64_t{1} << 63) + 12345;
        for (int i = 0; i < 1000; i++) CHECK(rng.below(big) < big);
    }

    // shuffle(): deterministic for a seed, always a permutation, safe on tiny ranges.
    {
        std::vector<int> identity(100);
        std::iota(identity.begin(), identity.end(), 0);
        auto a = identity, b = identity;
        SplitMix64 r1(99), r2(99);
        ravel::shuffle(a.begin(), a.end(), r1);
        ravel::shuffle(b.begin(), b.end(), r2);
        CHECK(a == b);
        CHECK(a != identity);
        auto sorted = a;
        std::sort(sorted.begin(), sorted.end());
        CHECK(sorted == identity);
        std::vector<int> empty, one{5};
        ravel::shuffle(empty.begin(), empty.end(), r1);
        ravel::shuffle(one.begin(), one.end(), r1);
        CHECK(empty.empty() && one == std::vector<int>{5});
    }
    std::cout << "PASS: splitmix64 known answers, unbiased bounded draws and Fisher-Yates shuffle\n";
} catch (const std::exception& e) {std::cerr << "FAIL: " << e.what() << '\n'; return 1;}
