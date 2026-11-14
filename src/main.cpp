#include "engine.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace ravel;
static std::string show(const Book& b) {
    auto s=b.spread();return s?std::to_string(*s):"NA";
}
int main(int argc,char** argv) try {
    if(argc!=2) throw std::invalid_argument("usage: ravel fixtures/session.txt");
    std::ifstream file(argv[1]); if(!file) throw std::runtime_error("cannot open fixture");
    Exchange ex; std::vector<Event> tape;
    std::string line;
    // The session fixture has no timestamps, so every event gets source time 0.
    // Timed scenarios replace this in milestone 1.
    const Time untimed=0;
    while(std::getline(file,line)){
        std::istringstream row(line); char side; Id id; Price price; Qty qty;
        if(!(row>>side>>id)) throw std::runtime_error("invalid command");
        if(side=='C') tape.push_back(ex.cancel(id,untimed));
        else if((side=='B'||side=='S') && row>>price>>qty) {
            auto events=ex.submit({id,side=='B'?Side::buy:Side::sell,price,qty},untimed);
            tape.insert(tape.end(),events.begin(),events.end());
        } else throw std::runtime_error("invalid command");
        if(row>>line) throw std::runtime_error("unexpected field");
    }
    Book truth, naive; Receiver receiver; Snapshot checkpoint{};
    std::cout<<"seq,event,reference_spread,naive_spread,published_spread,applied,recovery\n";
    for(auto e:tape) {
        truth.apply(e);
        if(e.seq!=6) {naive.apply(e);receiver.receive(e);}
        if(e.seq==8) checkpoint={e.seq,truth};
        if(e.seq==10) receiver.restore(checkpoint);
        std::cout<<e.seq<<','<<(e.kind==Kind::add?"add":e.kind==Kind::cancel?"cancel":"execute")
                 <<','<<show(truth)<<','<<show(naive)<<','<<(receiver.fresh()?show(receiver.book):"STALE")
                 <<','<<receiver.applied<<','<<(e.seq==10?"snapshot@8":"-")<<'\n';
        if(e.seq>=10 && !(receiver.book==truth)) throw std::logic_error("recovery mismatch");
    }
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
