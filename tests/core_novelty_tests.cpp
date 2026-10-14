#include "ebm/Novelty.hpp"
#include "ebm/Session.hpp"
#include "ebm/MidiWriter.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace ebm;
using namespace ebm::v2;
#define CHECK(test) do{if(!(test)){std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" " #test "\n";return 1;}}while(false)

// Independent bitmap definition for the intended hook/backbone, deliberately
// not a call to the same implementation used by the production selector.
static int backboneDifference(const Composition& a,const Composition& b) {
    const int count=a.settings.style==Style::MinimalBody?1:2;
    std::array<bool,32> x{},y{};
    for(int bar=0;bar<count;++bar) {
        for(const auto& e:a.bars[static_cast<std::size_t>(bar)].notes)
            if(e.role==Role::Anchor||e.role==Role::Drive||e.role==Role::Answer||e.role==Role::Resolution)
                x[static_cast<std::size_t>(bar*16+e.step)]=true;
        for(const auto& e:b.bars[static_cast<std::size_t>(bar)].notes)
            if(e.role==Role::Anchor||e.role==Role::Drive||e.role==Role::Answer||e.role==Role::Resolution)
                y[static_cast<std::size_t>(bar*16+e.step)]=true;
    }
    int diff=0;
    for(int k=0;k<count*16;++k)diff+=(x[static_cast<std::size_t>(k)]!=y[static_cast<std::size_t>(k)]);
    return diff;
}

int main(){
    // MSVC treats getenv() as C4996 under /WX. Use its checked CRT API.
    bool sanitizerSmoke=false;
#if defined(_MSC_VER)
    char* smokeEnvironment=nullptr;
    std::size_t smokeLength=0;
    const auto envStatus=_dupenv_s(&smokeEnvironment,&smokeLength,"EBM_CORE_SAN_SMOKE");
    sanitizerSmoke=(envStatus==0 && smokeEnvironment!=nullptr);
    std::free(smokeEnvironment);
#else
    sanitizerSmoke=std::getenv("EBM_CORE_SAN_SMOKE")!=nullptr;
#endif
    const int casesPerStyle=sanitizerSmoke?250:3000;
    int previousMislabels=0,strictAccepted=0,fallbacks=0,compared=0;
    for(int style=0;style<3;++style){
        Config cfg;cfg.style=static_cast<Style>(style);cfg.bars=8;cfg.variation=50;
        std::vector<Composition> recent;
        for(int n=1;n<=casesPerStyle;++n){
            cfg.seed=static_cast<std::uint64_t>(n)*1009ULL;
            const auto chosen=composeNovel(cfg,recent);
            CHECK(chosen.candidatesEvaluated>=1&&chosen.candidatesEvaluated<=16);
            int compareCount=0,minCore=999;
            for(std::size_t j=recent.size();j>0&&compareCount<8;--j,++compareCount){
                const auto& prior=recent[j-1];
                minCore=std::min(minCore,backboneDifference(chosen.composition,prior));
                if(chosen.meetsTarget){
                    CHECK(backboneDifference(chosen.composition,prior)>=2);
                    CHECK(motifRhythmDistance(chosen.composition,prior)>=2);
                    CHECK(motifDistance(chosen.composition,prior)>=4);
                }
            }
            if(compareCount>0){
                CHECK(chosen.meetsTarget==(chosen.meetsCoreRhythmicTarget&&chosen.meetsRhythmicTarget&&chosen.minimumDistance>=4));
                CHECK(chosen.minimumCoreRhythmicDistance==minCore);
                CHECK(chosen.meetsCoreRhythmicTarget==(minCore>=2));
            }else{
                CHECK(chosen.meetsTarget);
                CHECK(chosen.minimumCoreRhythmicDistance==0);
            }
            if(chosen.meetsTarget)++strictAccepted;else ++fallbacks;
            // Demonstrate the old omission from candidates, independent of
            // selection logic: different decorative notes but same backbone.
            if(!recent.empty() && n<=700){
                const auto initial=compose(cfg);
                for(std::size_t j=recent.size();j>0&&j+8>recent.size();--j){
                    const auto& prior=recent[j-1];
                    if(backboneDifference(initial,prior)==0&&
                       motifRhythmDistance(initial,prior)>=2){
                        ++previousMislabels;break;
                    }
                }
            }
            recent.push_back(chosen.composition);
            if(recent.size()>48)recent.erase(recent.begin());
            ++compared;
        }
    }
    CHECK(compared==3*casesPerStyle);
    CHECK(previousMislabels>0); // Negative fixture must actually occur, not a hypothetical.
    // A bona-fide pitch or transposition change never creates a new backbone.
    Config cfg;cfg.style=Style::ClassicEBM;cfg.seed=1307;
    const auto original=compose(cfg);
    cfg.rootMidi=43;
    CHECK(backboneDifference(original,compose(cfg))==0);
    CHECK(motifCoreRhythmDistance(original,compose(cfg))==0);
    cfg.rootMidi=36;cfg.variation=85;
    CHECK(motifCoreRhythmDistance(original,compose(cfg))==0);
    cfg.style=Style::MinimalBody;
    bool throws=false;
    try{(void)motifCoreRhythmDistance(original,compose(cfg));}
    catch(const std::invalid_argument&){throws=true;}
    CHECK(throws);
    // Session must preserve decisions across snapshot/restore, and expose the
    // strict-vs-fallback result without misrepresenting a musical quality score.
    cfg.style=Style::DarkElectro;cfg.seed=971;cfg.variation=50;
    Session left(cfg),right(cfg);
    for(int n=0;n<50;++n){
        const auto l=left.generate();
        right.restore(left.snapshot());
        CHECK(toMidi(render(left.current()))==toMidi(render(right.current())));
        CHECK(l.meetsNoveltyTarget==(l.meetsCoreRhythmicTarget&&l.meetsRhythmicTarget&&
             (left.snapshot().history.size()==1||l.minimumDistance>=4)));
        const auto nextLeft=left.generate();
        const auto nextRight=right.generate();
        CHECK(nextLeft.selectedSeed==nextRight.selectedSeed);
        CHECK(nextLeft.minimumCoreRhythmicDistance==nextRight.minimumCoreRhythmicDistance);
        CHECK(nextLeft.meetsCoreRhythmicTarget==nextRight.meetsCoreRhythmicTarget);
        CHECK(toMidi(render(left.current()))==toMidi(render(right.current())));
    }
    std::cout<<"PASS core-backbone novelty: generated="<<compared
             <<" strict="<<strictAccepted<<" fallback="<<fallbacks
             <<" adversarial pickup-only fixtures="<<previousMislabels<<"\n";
    return 0;
}
