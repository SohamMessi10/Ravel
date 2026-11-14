#include "../src/engine.hpp"
#include "../src/random.hpp"
#include "check.hpp"
#include <iostream>
using namespace ravel;
int main() try {
    Exchange ex;
    auto a=ex.submit({1,Side::sell,101,3}).at(0);
    auto b=ex.submit({2,Side::sell,101,4}).at(0);
    auto c=ex.submit({3,Side::sell,100,2}).at(0);
    auto fills=ex.submit({4,Side::buy,102,7});
    CHECK(fills.size()==3 && fills[0].order.id==3 && fills[1].order.id==1 &&
          fills[2].order.id==2 && fills[2].order.qty==2);
    auto cancel=ex.cancel(2);CHECK(cancel.order.qty==2);
    Book truth;Receiver rx;
    std::vector<Event> events{a,b,c};events.insert(events.end(),fills.begin(),fills.end());events.push_back(cancel);
    for(auto e:events) truth.apply(e);
    rx.receive(a);rx.receive(c);CHECK(!rx.fresh());rx.receive(c);rx.receive(b);CHECK(rx.fresh());
    Book at3=rx.book;rx.receive(fills[1]);rx.receive(cancel);CHECK(!rx.fresh());
    at3.apply(fills[0]);rx.restore({4,at3});CHECK(!rx.fresh() && rx.applied==5);
    rx.receive(fills[2]);CHECK(rx.fresh() && rx.book==truth && rx.applied==7);
    try{rx.restore({2,{}});CHECK(false);}catch(const std::invalid_argument&){}
    SplitMix64 rng(42);
    for(int i=0;i<1000;i++){
        ravel::shuffle(events.begin(),events.end(),rng);Receiver shuffled;
        for(auto e:events){shuffled.receive(e);shuffled.receive(e);}
        CHECK(shuffled.fresh() && shuffled.book==truth && shuffled.applied==7);
    }
    // Independent oracle: scan every resting order. Increasing IDs give arrival order here.
    Exchange tested; Book reconstructed; std::map<Id,Order> oracle; Seq sequence=0;
    for(Id id=1;id<=10000;++id){
        Order incoming{id,rng.below(2)?Side::buy:Side::sell,98+int(rng.below(5)),1+int(rng.below(12))};
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
            expected.push_back({++sequence,Kind::execute,fill});
            incoming.qty-=fill.qty;best->second.qty-=fill.qty;
            if(!best->second.qty)oracle.erase(best);
        }
        if(incoming.qty){oracle.emplace(id,incoming);expected.push_back({++sequence,Kind::add,incoming});}
        auto actual=tested.submit(original);CHECK(actual==expected);
        for(auto e:actual)reconstructed.apply(e);
        CHECK(reconstructed.orders==oracle);
    }
    std::cout<<"PASS: 10000 seeded submissions matched independent price/time oracle event by event\n";
    std::cout<<"PASS: price priority, FIFO, partial fills, cancel, gap buffering, snapshot catch-up, obsolete snapshot and 1000 shuffled duplicate deliveries\n";
} catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
