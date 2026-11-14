#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ravel {
using Id = std::uint64_t;
using Seq = std::uint64_t;
using Price = std::int64_t; // integer ticks; the tick size is recorded per dataset
using Qty = std::int64_t;   // integer lots
using Time = std::int64_t;  // simulated nanoseconds since the start of the run
enum class Side { buy, sell };
enum class Kind { add, cancel, execute };
struct Order {
    Id id; Side side; Price price; Qty qty;
    bool operator==(const Order&) const = default;
};
struct Event {
    Seq seq; Time time; Kind kind; Order order;
    bool operator==(const Event&) const = default;
};
using Level = std::deque<Order>;
using Ladder = std::map<Price, Level>;

// Each submitted ID is single-use for a session. Price is integer ticks.
// Source times must not decrease; every event carries the time of the call that produced it.
class Exchange {
    Ladder bids_, asks_;
    std::map<Id, std::pair<Side,Price>> live_;
    std::set<Id> used_;
    Seq seq_ = 0;
    Time time_ = 0;
public:
    std::vector<Event> submit(Order o, Time time) {
        if (!o.id || o.price <= 0 || o.qty <= 0 || o.qty > 1000000 || used_.contains(o.id))
            throw std::invalid_argument("invalid limit order or reused ID");
        if (time < time_) throw std::invalid_argument("source time went backwards");
        time_ = time;
        used_.insert(o.id);
        auto& opposite = o.side == Side::buy ? asks_ : bids_;
        std::vector<Event> events;
        while (o.qty && !opposite.empty()) {
            auto level = o.side == Side::buy ? opposite.begin() : std::prev(opposite.end());
            if (o.side == Side::buy ? level->first > o.price : level->first < o.price) break;
            auto& maker = level->second.front();
            Order fill = maker;
            fill.qty = std::min(o.qty, maker.qty);
            events.push_back({++seq_, time, Kind::execute, fill});
            o.qty -= fill.qty;
            maker.qty -= fill.qty;
            if (!maker.qty) {
                live_.erase(maker.id);
                level->second.pop_front();
                if (level->second.empty()) opposite.erase(level);
            }
        }
        if (o.qty) {
            (o.side == Side::buy ? bids_ : asks_)[o.price].push_back(o);
            live_[o.id] = {o.side, o.price};
            events.push_back({++seq_, time, Kind::add, o});
        }
        return events;
    }
    Event cancel(Id id, Time time) {
        if (time < time_) throw std::invalid_argument("source time went backwards");
        auto [side,price] = live_.at(id);
        time_ = time;
        auto& ladder = side == Side::buy ? bids_ : asks_;
        auto& level = ladder.at(price);
        auto it = std::find_if(level.begin(), level.end(), [=](Order o){return o.id == id;});
        Order removed = *it;
        level.erase(it);
        if (level.empty()) ladder.erase(price);
        live_.erase(id);
        return {++seq_, time, Kind::cancel, removed};
    }
};

// Reference reconstruction deliberately scans order IDs instead of mirroring the matcher.
struct Book {
    std::map<Id,Order> orders;
    bool operator==(const Book&) const = default;
    void apply(Event e) {
        auto o=e.order;
        if (e.kind == Kind::add) {
            if (!orders.emplace(o.id,o).second) throw std::logic_error("duplicate add");
        } else {
            auto& old=orders.at(o.id);
            if (old.price!=o.price || old.side!=o.side || o.qty<=0 || o.qty>old.qty ||
                (e.kind==Kind::cancel && old.qty!=o.qty)) throw std::logic_error("invalid removal");
            old.qty-=o.qty;
            if (!old.qty) orders.erase(o.id);
        }
    }
    std::optional<Price> spread() const {
        std::optional<Price> bid,ask;
        for (const auto& [id,o]:orders) {
            if(o.side==Side::buy) bid=std::max(bid.value_or(o.price),o.price);
            else ask=std::min(ask.value_or(o.price),o.price);
        }
        if (!bid || !ask) return {};
        return *ask-*bid;
    }
};
struct Snapshot {Seq seq; Book book;};
class Receiver {
    static constexpr std::size_t capacity=4096;
    std::map<Seq,Event> pending_;
    Seq high_=0;
    void drain() {
        while (true) {
            auto it=pending_.find(applied+1);
            if(it==pending_.end()) return;
            book.apply(it->second); ++applied; pending_.erase(it);
        }
    }
public:
    Book book;
    Seq applied=0;
    bool fresh() const {return applied==high_;}
    void receive(Event e) {
        if (!e.seq) throw std::invalid_argument("zero sequence");
        if (e.seq<=applied) return;
        high_=std::max(high_,e.seq);
        if(e.seq==applied+1) {book.apply(e); ++applied; drain(); return;}
        if (pending_.size()<capacity || pending_.contains(e.seq)) {
            auto [it,inserted]=pending_.emplace(e.seq,e);
            if (!inserted && !(it->second==e)) throw std::logic_error("conflicting sequence");
        }
    }
    void restore(const Snapshot& snap) {
        if (snap.seq<applied) throw std::invalid_argument("obsolete snapshot");
        book=snap.book; applied=snap.seq; high_=std::max(high_,applied);
        pending_.erase(pending_.begin(),pending_.upper_bound(applied)); drain();
    }
};
}
