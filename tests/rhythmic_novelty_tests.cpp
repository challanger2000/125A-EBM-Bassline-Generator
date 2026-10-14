#include "ebm/Session.hpp"
#include "ebm/MidiWriter.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace ebm;
using namespace ebm::v2;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" " #x "\n";return 1;}}while(false)
int main(){
    int preexistingFalseNovel=0;
    int selected=0;
    int accepted=0;
    int failed=0;
    // Previously: structurally new by pitch alone but with identical source
    // onsets. Independent test, not an assertion based on the implementation.
    for(int style=1;style<3;++style) {
        Config cfg;cfg.style=static_cast<Style>(style);cfg.bars=8;cfg.variation=50;
        std::vector<Composition> preceding;
        for(int i=1;i<=700;++i) {
            cfg.seed=static_cast<std::uint64_t>(i)*1009ULL;
            const auto candidate=composeNovel(cfg,preceding);
            CHECK(candidate.candidatesEvaluated>=1&&candidate.candidatesEvaluated<=16);
            CHECK(candidate.composition.settings.seed!=0 || cfg.seed==0);
            CHECK(candidate.minimumDistance>=0&&candidate.minimumRhythmicDistance>=0);
            validate(candidate.composition);
            int refs=0;
            for(std::size_t j=preceding.size();j>0&&refs<8;--j){
                const auto& older=preceding[j-1];++refs;
                if(candidate.meetsTarget) {
                    CHECK(motifDistance(candidate.composition,older)>=4);
                    CHECK(motifRhythmDistance(candidate.composition,older)>=2);
                }
            }
            if(candidate.meetsTarget){CHECK(candidate.meetsRhythmicTarget);++accepted;}
            else {CHECK(!candidate.meetsRhythmicTarget||candidate.minimumDistance<4);++failed;}
            preceding.push_back(candidate.composition);
            if(preceding.size()>48)preceding.erase(preceding.begin());
            ++selected;
        }
    }
    // Find at least one empirically reproducible counterexample of a prior
    // structurally accepted pair with identical source-onset fingerprints.
    for(int style=1;style<3;++style){
        Config cfg;cfg.style=static_cast<Style>(style);cfg.variation=50;
        std::vector<Composition> existing;
        for(int i=1;i<=400;++i){
            cfg.seed=static_cast<std::uint64_t>(i)*1009ULL;
            auto phrase=compose(cfg);
            int seen=0;
            for(std::size_t k=existing.size();k>0&&seen<8;--k,++seen){
                if(motifRhythmDistance(phrase,existing[k-1])==0 &&
                   motifDistance(phrase,existing[k-1])>=4){
                    const auto checked=composeNovel(cfg,{existing[k-1]});
                    CHECK(checked.candidatesEvaluated>=2||!checked.meetsTarget);
                    if(checked.meetsTarget)
                        CHECK(motifRhythmDistance(checked.composition,existing[k-1])>=2);
                    ++preexistingFalseNovel;
                    break;
                }
            }
            existing.push_back(std::move(phrase));
        }
    }
    CHECK(preexistingFalseNovel>0);
    // Changing pitch globally or arrangement variation is NOT source-rhythm novelty.
    Config cfg;cfg.style=Style::DarkElectro;cfg.seed=1510;cfg.variation=85;
    auto first=compose(cfg);
    cfg.rootMidi=43;
    CHECK(motifRhythmDistance(first,compose(cfg))==0);
    cfg.rootMidi=36;cfg.variation=0;
    CHECK(motifRhythmDistance(first,compose(cfg))==0);
    cfg.style=Style::ClassicEBM;
    bool rejected=false;
    try {(void)motifRhythmDistance(first,compose(cfg));}
    catch(const std::invalid_argument&){rejected=true;}
    CHECK(rejected);
    // End-to-end session must not mislabel its seed-selection result.
    cfg.style=Style::DarkElectro;cfg.seed=27;cfg.variation=50;
    Session session(cfg);
    for(int i=0;i<32;++i){
        const auto p=session.generate();
        const auto snapshot=session.snapshot();
        Session replay(cfg);replay.restore(snapshot);
        CHECK(toMidi(render(session.current()))==toMidi(render(replay.current())));
        CHECK(p.meetsNoveltyTarget == (p.meetsRhythmicTarget &&
            (snapshot.history.size()==1||p.minimumDistance>=4)));
        CHECK(p.selectedSeed==session.current().settings.seed);
    }
    std::cout<<"PASS independent rhythmic novelty: selected="<<selected
             <<" accepted="<<accepted<<" fallback="<<failed
             <<" prior-rhythm-duplicates="<<preexistingFalseNovel<<"\n";
    return 0;
}
