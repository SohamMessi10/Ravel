#pragma once
#include "engine.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ravel {
// FNV-1a 64-bit (RFC 9923 / draft-eastlake-fnv): xor each byte, then multiply by the prime.
// An identity check for input files, not a security hash.
std::uint64_t fnv1a64(std::string_view bytes);
std::string hex64(std::uint64_t value); // 16 lowercase hex digits

std::string read_file(const std::string& path);

// Timed synthetic commands, one per line, times in integer nanoseconds:
//   <time_ns> B <id> <price_ticks> <qty>    buy limit order
//   <time_ns> S <id> <price_ticks> <qty>    sell limit order
//   <time_ns> C <id>                        cancel
// Anything else is an error naming the line number. Commands go through the matcher,
// which assigns sequence numbers and stamps the command's time on every event.
std::vector<Event> parse_commands(std::string_view text);
}
