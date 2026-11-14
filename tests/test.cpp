#include "../src/engine.hpp"
#include "../src/random.hpp"
#include "check.hpp"
#include <iostream>
using namespace ravel;
int main() try {
    Exchange ex;
    auto a=ex.submit({1,Side::sell,101,3},0).at(0);
    auto b=ex.submit({2,Side::sell,101,4},0).at(0);
    auto c=ex.submit({3,Side::sell,100,2},0).at(0);
    auto fills=ex.submit({4,Side::buy,102,7},0);
    CHECK(fills.size()==3 && fills[0].order.id==3 && fills[1].order.id==1 &&
          fills[2].order.id==2 && fills[2].order.qty==2);
    auto cancel=ex.cancel(2,0);CHECK(cancel.order.qty==2);
    Book truth;Receiver rx;
    std::vector<Event> events{a,b,c};events.insert(events.end(),fills.begin(),fills.end());events.push_back(cancel);
    for(auto e:events) truth.apply(e);
    rx.receive(a);rx.receive(c);CHECK(!rx.fresh());rx.receive(c);rx.receive(b);CHECK(rx.fresh());
    Book at3=rx.book;rx.receive(fills[1]);rx.receive(cancel);CHECK(!rx.fresh());
    at3.apply(fills[0]);rx.restore({4,at3});CHECK(!rx.fresh() && rx.applied==5);
    rx.receive(fills[2]);CHECK(rx.fresh() && rx.book==truth && rx.applied==7);
    try{rx.restore({2,{}});CHECK(false);}catch(const std::invalid_argument&){}

    // Source time: carried on every event, never decreasing, and a rejected call changes nothing.
    {
        Exchange clock;
        CHECK(clock.submit({1,Side::buy,100,1},5).at(0).time==5);
        bool threw=false;
        try{clock.submit({2,Side::buy,100,1},4);}catch(const std::invalid_argument&){threw=true;}
        CHECK(threw);
        auto same=clock.submit({2,Side::buy,100,1},5);
        CHECK(same.at(0).seq==2 && same.at(0).time==5);
        threw=false;
        try{clock.cancel(1,4);}catch(const std::invalid_argument&){threw=true;}
        CHECK(threw);
        auto later=clock.cancel(1,9);
        CHECK(later.seq==3 && later.time==9 && later.order.id==1);
    }
    // Prices and quantities are 64-bit: values beyond the 32-bit range survive a round trip.
    {
        const Price high=3'000'000'000;
        Exchange wide; Book book;
        for(auto e:wide.submit({1,Side::sell,high,5},0)) book.apply(e);
        for(auto e:wide.submit({2,Side::buy,high-10,5},0)) book.apply(e);
        CHECK(book.orders().at(1).price==high && book.spread()==Price{10});
    }
    // FIFO: queue order within a price level is part of the book (issue #2).
    {
        auto add=[](Seq s,Id id){return Event{s,0,Kind::add,{id,Side::buy,100,5}};};
        Book first, second;
        first.apply(add(1,10)); first.apply(add(2,11));
        second.apply(add(1,11)); second.apply(add(2,10));
        CHECK(first.orders()==second.orders());
        CHECK(!(first==second));
        first.apply({3,0,Kind::execute,{10,Side::buy,100,2}});
        const auto& level=first.bids().at(100);
        CHECK(level.size()==2 && level[0].id==10 && level[0].qty==3 && level[1].id==11);
        CHECK(first.best_bid()==Price{100} && !first.best_ask() && !first.spread());
    }

    SplitMix64 rng(42);
    for(int i=0;i<1000;i++){
        ravel::shuffle(events.begin(),events.end(),rng);Receiver shuffled;
        for(auto e:events){shuffled.receive(e);shuffled.receive(e);}
        CHECK(shuffled.fresh() && shuffled.book==truth && shuffled.applied==7);
    }
    // Independent oracle: scan every resting order. Increasing IDs give arrival order here.
    // Each submission gets source time = its ID in nanoseconds.
    Exchange tested; Book reconstructed; std::map<Id,Order> oracle; Seq sequence=0;
    for(Id id=1;id<=10000;++id){
        const Time time=static_cast<Time>(id);
        Order incoming{id,rng.below(2)?Side::buy:Side::sell,98+Price(rng.below(5)),1+Qty(rng.below(12))};
        const Order original=incoming;
        std::vector<Event> expected;
        while(incoming.qty){
            auto best=oracle.end();
            for(auto it=oracle.begin();it!=oracle.end();++it){
                const auto& maker=it->second;
                bool buy=incoming.side==Side::buy;
                if(maker.side==incoming.side || (buy?maker.price>incoming.price:maker.price<incoming.price))continue;
                if(best==oracle.end() || (buy?maker.price<best->second.price:maker.price>best->second.price))best=it;
            }
            if(best==oracle.end())break;
            Order fill=best->second;fill.qty=std::min(fill.qty,incoming.qty);
            expected.push_back({++sequence,time,Kind::execute,fill});
            incoming.qty-=fill.qty;best->second.qty-=fill.qty;
            if(!best->second.qty)oracle.erase(best);
        }
        if(incoming.qty){oracle.emplace(id,incoming);expected.push_back({++sequence,time,Kind::add,incoming});}
        auto actual=tested.submit(original,time);CHECK(actual==expected);
        for(auto e:actual)reconstructed.apply(e);
        CHECK(reconstructed.orders()==oracle);
        // The oracle is keyed by ID and IDs follow arrival, so its ID order is FIFO order.
        Ladder fifo_bids, fifo_asks;
        for(const auto& [oid,o]:oracle)(o.side==Side::buy?fifo_bids:fifo_asks)[o.price].push_back(o);
        CHECK(reconstructed.bids()==fifo_bids && reconstructed.asks()==fifo_asks);
    }
    std::cout<<"PASS: 10000 seeded submissions matched independent price/time oracle event by event\n";
    std::cout<<"PASS: price priority, FIFO, partial fills, cancel, source time, 64-bit prices, gap buffering, snapshot catch-up, obsolete snapshot and 1000 shuffled duplicate deliveries\n";
} catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
