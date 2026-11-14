#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <initializer_list>
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

// Reconstructed book. It only applies add, execute and cancel events and never matches,
// so it stays independent of the matcher. Each price level keeps its resting orders in
// FIFO (time priority) order, and equality compares that order (SEMANTICS.md section 5).
class Book {
    Ladder bids_, asks_;
    std::map<Id, std::pair<Side,Price>> where_; // order ID -> its level; derived from the ladders
    Ladder& ladder(Side s) {return s==Side::buy ? bids_ : asks_;}
public:
    bool operator==(const Book& other) const {return bids_==other.bids_ && asks_==other.asks_;}
    void apply(const Event& e) {
        const Order& o=e.order;
        if (e.kind == Kind::add) {
            if (!where_.emplace(o.id, std::pair{o.side,o.price}).second) throw std::logic_error("duplicate add");
            ladder(o.side)[o.price].push_back(o);
            return;
        }
        auto found=where_.find(o.id);
        if (found==where_.end()) throw std::out_of_range("unknown order");
        auto [side,price]=found->second;
        if (side!=o.side || price!=o.price) throw std::logic_error("invalid removal");
        auto& book_side=ladder(side);
        auto level=book_side.find(price);
        if (level==book_side.end()) throw std::logic_error("book index out of sync");
        auto it=std::find_if(level->second.begin(), level->second.end(), [&](const Order& r){return r.id==o.id;});
        if (it==level->second.end()) throw std::logic_error("book index out of sync");
        if (o.qty<=0 || o.qty>it->qty || (e.kind==Kind::cancel && o.qty!=it->qty))
            throw std::logic_error("invalid removal");
        it->qty-=o.qty; // a partial execution keeps the order's place in the queue
        if (!it->qty) {
            level->second.erase(it);
            if (level->second.empty()) book_side.erase(level);
            where_.erase(found);
        }
    }
    const Ladder& bids() const {return bids_;}
    const Ladder& asks() const {return asks_;}
    std::optional<Price> best_bid() const {
        if (bids_.empty()) return {};
        return bids_.rbegin()->first;
    }
    std::optional<Price> best_ask() const {
        if (asks_.empty()) return {};
        return asks_.begin()->first;
    }
    std::optional<Price> spread() const {
        auto bid=best_bid(), ask=best_ask();
        if (!bid || !ask) return {};
        return *ask-*bid;
    }
    // Every resting order keyed by ID, for comparisons that ignore queue order.
    std::map<Id,Order> orders() const {
        std::map<Id,Order> all;
        for (const Ladder* side : {&bids_, &asks_})
            for (const auto& [price,level] : *side)
                for (const auto& o : level) all.emplace(o.id,o);
        return all;
    }
};
struct Snapshot {Seq seq; Book book;};
class Receiver {
    std::size_t capacity_;
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
    explicit Receiver(std::size_t capacity=4096) : capacity_(capacity) {}
    Book book;
    Seq applied=0;
    bool fresh() const {return applied==high_;}
    Seq observed() const {return high_;}
    void receive(Event e) {
        if (!e.seq) throw std::invalid_argument("zero sequence");
        if (e.seq<=applied) return;
        high_=std::max(high_,e.seq);
        if(e.seq==applied+1) {book.apply(e); ++applied; drain(); return;}
        if (pending_.size()<capacity_ || pending_.contains(e.seq)) {
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
