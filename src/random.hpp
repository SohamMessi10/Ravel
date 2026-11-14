#pragma once
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace ravel {
// splitmix64, written from the reference implementation by Sebastiano Vigna,
// which is dedicated to the public domain: https://prng.di.unimi.it/splitmix64.c
// The increment, shift amounts and multipliers match the reference exactly;
// tests/random_test.cpp checks outputs printed by the compiled reference.
// Only unsigned 64-bit arithmetic is used (wraparound is defined by the
// standard), so every compiler and standard library produces the same sequence.
class SplitMix64 {
    std::uint64_t state_;
public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {}

    std::uint64_t next() {
        std::uint64_t z = (state_ += 0x9e3779b97f4a7c15u);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
        return z ^ (z >> 31);
    }

    // Uniform integer in [0, bound) without modulo bias. Draws below
    // threshold = 2^64 mod bound are rejected, so the accepted range
    // [threshold, 2^64) holds an exact multiple of bound values.
    std::uint64_t below(std::uint64_t bound) {
        if (bound == 0) throw std::invalid_argument("bound must be positive");
        const std::uint64_t threshold = (0 - bound) % bound;
        while (true) {
            const std::uint64_t r = next();
            if (r >= threshold) return r % bound;
        }
    }
};

// Fisher-Yates: for i = n..2, swap element i-1 with a uniform position in [0, i).
template <class RandomIt>
void shuffle(RandomIt first, RandomIt last, SplitMix64& rng) {
    using Diff = typename std::iterator_traits<RandomIt>::difference_type;
    for (auto i = static_cast<std::uint64_t>(last - first); i > 1; --i) {
        const std::uint64_t j = rng.below(i);
        using std::swap;
        swap(first[static_cast<Diff>(i - 1)], first[static_cast<Diff>(j)]);
    }
}
}
