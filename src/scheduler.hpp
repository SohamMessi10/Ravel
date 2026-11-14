#pragma once
#include "engine.hpp"
#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ravel {
// Event classes, in tie-break rank order at equal time (docs/SEMANTICS.md section 3).
enum class EventClass : std::uint8_t {source = 0, delivery = 1, retransmission = 2, snapshot = 3, heartbeat = 4, timer = 5};

// Total order of scheduled items: time, then class, then sequence, then occurrence.
// The defaulted <=> compares the members in declaration order, so the member order is the rule.
struct ScheduleKey {
    Time time;
    EventClass cls;
    Seq seq;
    std::uint32_t occurrence;
    auto operator<=>(const ScheduleKey&) const = default;
};

// One event queue for a run. Keys are unique, and processing never goes backwards:
// a new key must be greater than the last key popped.
template <class Payload>
class Scheduler {
    std::map<ScheduleKey, Payload> queue_;
    std::optional<ScheduleKey> last_;
public:
    void schedule(const ScheduleKey& key, Payload payload) {
        if (last_ && key <= *last_) throw std::logic_error("scheduled at or before the last processed key");
        if (!queue_.emplace(key, std::move(payload)).second) throw std::logic_error("duplicate schedule key");
    }
    bool empty() const {return queue_.empty();}
    std::size_t size() const {return queue_.size();}
    const ScheduleKey& next_key() const {
        if (queue_.empty()) throw std::logic_error("scheduler is empty");
        return queue_.begin()->first;
    }
    std::pair<ScheduleKey, Payload> pop() {
        if (queue_.empty()) throw std::logic_error("scheduler is empty");
        auto node = queue_.extract(queue_.begin());
        last_ = node.key();
        return {node.key(), std::move(node.mapped())};
    }
};
}
