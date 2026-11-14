#include "input.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ravel {
std::uint64_t fnv1a64(std::string_view bytes) {
    std::uint64_t hash = 0xcbf29ce484222325u;
    for (unsigned char c : bytes) {
        hash ^= c;
        hash *= 0x100000001b3u;
    }
    return hash;
}

std::string hex64(std::uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i, value >>= 4) out[static_cast<std::size_t>(i)] = digits[value & 0xf];
    return out;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open " + path);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

std::vector<Event> parse_commands(std::string_view text) {
    Exchange ex;
    std::vector<Event> tape;
    std::istringstream lines{std::string(text)};
    std::string line;
    for (int number = 1; std::getline(lines, line); ++number) {
        auto error = [&](const std::string& what) {
            return std::runtime_error("commands line " + std::to_string(number) + ": " + what);
        };
        std::istringstream row(line);
        Time time; char side; Id id;
        if (!(row >> time >> side >> id)) throw error("expected <time_ns> <B|S|C> <id> ...");
        Price price = 0; Qty qty = 0;
        if (side == 'B' || side == 'S') {
            if (!(row >> price >> qty)) throw error("expected <price_ticks> <qty>");
        } else if (side != 'C') {
            throw error("unknown command");
        }
        std::string extra;
        if (row >> extra) throw error("unexpected field '" + extra + "'");
        try {  // matcher errors (reused ID, unknown cancel, time going backwards) get the line number too
            if (side == 'C') {
                tape.push_back(ex.cancel(id, time));
            } else {
                auto events = ex.submit({id, side == 'B' ? Side::buy : Side::sell, price, qty}, time);
                tape.insert(tape.end(), events.begin(), events.end());
            }
        } catch (const std::exception& e) {
            throw error(e.what());
        }
    }
    return tape;
}
}
