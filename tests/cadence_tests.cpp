#include "ebm/Composer.hpp"
#include "ebm/MidiWriter.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if(!(x)) { std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << " " #x "\n"; return 1; } } while(false)

using namespace ebm;
using namespace ebm::v2;

static const Event* find(const Bar& b,int id) {
    auto it=std::find_if(b.notes.begin(),b.notes.end(),[id](const Event& e){return e.id==id;});
    return it==b.notes.end()?nullptr:&*it;
}

template<typename F> static bool rejects(F&& f) {
    try { f(); return false; } catch(const std::invalid_argument&) { return true; }
}

int main(){
    std::array<int,3> arrivals{},barsCount{},heldBreaths{};
    for(int style=0;style<3;++style){
        for(int seed=0;seed<300;++seed){
            for(int length:{4,8})for(int intensity:{0,85}){
                Config cfg;cfg.style=static_cast<Style>(style);
                cfg.seed=static_cast<std::uint64_t>(314159+style*1019+seed);
                cfg.bars=length;cfg.variation=intensity;
                const Composition c=compose(cfg);
                validate(c);
                const auto rendered=render(c);
                CHECK(!toMidi(rendered).empty());
                // Editing a repeated later bar must not silently change the source motif.
                if(intensity==0){
                    const int span=style==0?1:2;
                    for(int b=span;b<length;++b)
                        CHECK(c.bars[static_cast<std::size_t>(b)].notes==
                              c.bars[static_cast<std::size_t>(b%span)].notes);
                }
                // Source motif alone (not later variations) is the cadence grammar.
                if(length==8 && intensity==0){
                    const int originalBars=style==0?1:2;
                    for(int b=0;b<originalBars;++b){
                        const auto& cell=c.bars[static_cast<std::size_t>(b)];
                        ++barsCount[style];
                        const bool chromatic=std::any_of(cell.notes.begin(),cell.notes.end(),
                            [](const Event& e){return e.role==Role::Tension && e.targetBarDelta==1;});
                        arrivals[style]+=chromatic;
                        if(cell.notes.back().step+cell.notes.back().duration < 16)
                            heldBreaths[style]++;
                        if(style==0){
                            const Event* closing=find(cell,6);
                            if(cell.device==Device::CrossPulse){
                                CHECK(closing && closing->role==Role::Tension && closing->interval==-1);
                            } else if(cell.device==Device::DelayedBackbeat){
                                CHECK(!closing);
                            } else {
                                CHECK(cell.device==Device::PedalGap);
                                CHECK(!closing || (closing->role==Role::Pickup && closing->interval==0 &&
                                      closing->targetId==-1));
                            }
                        } else if(style==1 && b==1){
                            const auto *closing=find(cell,109);
                            CHECK((cell.device==Device::BreakAndShout)==(!closing));
                        } else if(style==2 && b==1){
                            const auto *closing=find(cell,106);
                            CHECK((cell.device==Device::LateSuspension)==(!closing));
                        }
                    }
                }
            }
        }
    }
    // Deliberately selected fixed seeds: safety assertions, not EBM aesthetics ratings.
    CHECK(barsCount[0]==300 && barsCount[1]==600 && barsCount[2]==600);
    CHECK(arrivals[0]<150 && arrivals[1]<260 && arrivals[2]<570);
    CHECK(heldBreaths[0]>60 && heldBreaths[1]>50 && heldBreaths[2]>50);
    // Source identity must be independent of whether an optional closing tone exists.
    for(int style=0;style<3;++style){
        Config cfg;cfg.style=static_cast<Style>(style);cfg.variation=0;
        for(int seed=1;seed<40;++seed){
            cfg.seed=static_cast<std::uint64_t>(seed);
            Composition broken=compose(cfg);
            broken.bars[0].notes.front().id=999;
            CHECK(rejects([&]{validate(broken);}));
            broken=compose(cfg);
            broken.bars[0].notes.front().step=1;
            CHECK(rejects([&]{validate(broken);}));
        }
    }
    // Reject manufactured mandatory leading tones on devices requiring musical air.
    for(int style=0;style<3;++style){
        Config cfg;cfg.style=static_cast<Style>(style);cfg.variation=0;
        bool found=false;
        for(int seed=1;seed<500 && !found;++seed){
            cfg.seed=static_cast<std::uint64_t>(seed);
            Composition bad=compose(cfg);
            if(style==0){
                if(bad.bars[0].device!=Device::DelayedBackbeat)continue;
                for(auto& bar:bad.bars)
                    bar.notes.push_back({6,15,-1,1,88,Role::Tension,1,1});
            } else if(style==1){
                if(bad.bars[0].device!=Device::BreakAndShout)continue;
                for(std::size_t b=1;b<bad.bars.size();b+=2)
                    bad.bars[b].notes.push_back({109,15,-1,1,88,Role::Tension,1,1});
            } else {
                if(bad.bars[0].device!=Device::LateSuspension)continue;
                for(std::size_t b=1;b<bad.bars.size();b+=2)
                    bad.bars[b].notes.push_back({106,15,-1,1,88,Role::Tension,1,1});
            }
            found=true;
            CHECK(rejects([&]{validate(bad);}));
        }
        CHECK(found);
    }
    for(int style=0;style<3;++style){
        std::cout<<styleName(static_cast<Style>(style))<<" chromatic bar transitions "
                 <<arrivals[style]<<'/'<<barsCount[style]<<", bars with intentional last-grid silence "
                 <<heldBreaths[style]<<'/'<<barsCount[style]<<"\n";
    }
    std::cout<<"PASS: cadences, structural silence, optional pickups, bad endings, stable source IDs\n";
}
