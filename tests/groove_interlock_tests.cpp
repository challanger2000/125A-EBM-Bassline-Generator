#include "ebm/Composer.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#define CHECK(x) do { if (!(x)) {std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" " #x "\n";return 1;} }while(false)

using namespace ebm;
using namespace ebm::v2;
static const Event& eventById(const Bar& bar,int id) {
    const auto it=std::find_if(bar.notes.begin(),bar.notes.end(),[id](const Event& e){return e.id==id;});
    if(it==bar.notes.end())throw std::runtime_error("missing test event");
    return *it;
}
template<typename Fn> static bool rejects(Fn fn) {
    try {fn();return false;} catch(const std::invalid_argument&) {return true;}
}

int main() {
    int pairsChecked=0, silentChecks=0;
    for(int style=0;style<3;++style)
      for(int seed=1;seed<=300;++seed)
        for(int bars:{4,8})
          for(int level:{0,15,30,55,85,100}) {
            Config c;c.style=static_cast<Style>(style);
            c.seed=static_cast<std::uint64_t>(seed*7187+style*101);
            c.bars=bars;c.variation=level;
            const Composition out=compose(c);
            validate(out);
            const auto& final=out.bars.back();
            const int pairs=static_cast<int>(std::count_if(out.edits.begin(),out.edits.end(),
                [](const Edit& e){return e.operation==Operation::Reaccent;}));
            if(style==0||level<75) {
                CHECK(pairs==0);
                ++silentChecks;
                continue;
            }
            CHECK(pairs==1);
            const Edit& e=out.edits.back();
            CHECK(e.operation==Operation::Reaccent && e.bar==bars-1);
            CHECK(e.eventId==(style==1?108:101) && e.partnerEventId==(style==1?107:104));
            CHECK(e.newVelocity==e.partnerPreviousVelocity);
            CHECK(e.partnerNewVelocity==e.previousVelocity);
            CHECK(e.newVelocity+e.partnerNewVelocity==e.previousVelocity+e.partnerPreviousVelocity);
            CHECK(e.previousVelocity>e.partnerPreviousVelocity);
            const auto& strong=eventById(final,e.eventId);
            const auto& answer=eventById(final,e.partnerEventId);
            CHECK(strong.step%4==0 && answer.step%4!=0);
            CHECK(strong.role==Role::Anchor||strong.role==Role::Resolution);
            CHECK(answer.role==Role::Answer);
            CHECK(strong.velocity==e.newVelocity && answer.velocity==e.partnerNewVelocity);
            CHECK(strong.velocity<answer.velocity);
            const auto& source=out.bars[1];
            CHECK(eventById(source,e.eventId).velocity==e.previousVelocity);
            CHECK(eventById(source,e.partnerEventId).velocity==e.partnerPreviousVelocity);
            for(const auto& n:final.notes)
                if(n.id!=e.eventId && n.id!=e.partnerEventId)
                    CHECK(n.velocity==eventById(source,n.id).velocity);
            ++pairsChecked;
          }
    CHECK(pairsChecked==2400 && silentChecks==8400);
    // All paired-transfer details must be authenticated by the independent motif replay.
    Config cfg;cfg.style=Style::DarkElectro;cfg.bars=8;cfg.seed=1103;cfg.variation=85;
    const Composition good=compose(cfg);
    const auto checkForgery=[&](const auto& modify) {
        Composition bad=good;
        modify(bad);
        return rejects([&]{validate(bad);});
    };
    CHECK(checkForgery([](Composition& x){x.edits.back().partnerEventId=999;}));
    CHECK(checkForgery([](Composition& x){++x.edits.back().previousVelocity;}));
    CHECK(checkForgery([](Composition& x){++x.edits.back().newVelocity;}));
    CHECK(checkForgery([](Composition& x){++x.edits.back().partnerPreviousVelocity;}));
    CHECK(checkForgery([](Composition& x){++x.edits.back().partnerNewVelocity;}));
    CHECK(checkForgery([](Composition& x){x.edits.back().newStep=8;}));
    CHECK(checkForgery([](Composition& x){x.edits.back().previousStep=1;}));
    CHECK(checkForgery([](Composition& x){x.edits.back().bar=5;}));
    CHECK(checkForgery([](Composition& x){x.edits.pop_back();}));
    CHECK(checkForgery([](Composition& x){x.bars.back().notes[0].velocity-=1;}));
    CHECK(checkForgery([](Composition& x){x.edits.back().operation=Operation::Repeat;}));
    CHECK(checkForgery([](Composition& x){++x.edits.back().previousInterval;}));
    CHECK(checkForgery([](Composition& x){x.edits.front().partnerEventId=101;}));
    CHECK(checkForgery([](Composition& x){x.settings.variation=55;}));
    std::cout<<"PASS: "<<pairsChecked<<" paired accent transfers, "<<silentChecks
             <<" unchanged non-target cases; fourteen adversarial forgeries rejected\n";
}
