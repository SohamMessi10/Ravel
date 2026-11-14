#pragma once
#include <stdexcept>
#include <string>

namespace ravel::test {
// Thrown by CHECK. It derives from std::logic_error, not std::invalid_argument,
// so a test that catches an expected std::invalid_argument never swallows it.
struct CheckFailure : std::logic_error {
    using std::logic_error::logic_error;
};

[[noreturn]] inline void fail(const char* expr, const char* file, int line) {
    throw CheckFailure(std::string(file) + ":" + std::to_string(line) + ": CHECK(" + expr + ") failed");
}
}

// Variadic so that expressions containing commas need no extra parentheses.
#define CHECK(...) ((__VA_ARGS__) ? void() : ::ravel::test::fail(#__VA_ARGS__, __FILE__, __LINE__))
