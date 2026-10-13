#include "ebm/Novelty.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ebm::v2 {
namespace {
// Aim to differ in more than just one incidental pitch. This threshold is a
// stated, testable design choice, not a published EBM authenticity threshold.
constexpr int kMinimumStructuralDistance = 4;
constexpr int kMaxCandidates = 16;
constexpr std::size_t kHistoryWindow = 8;

int countMotifBars(const Composition& c) {
    return c.settings.style==Style::MinimalBody?1:2;
}
std::array<int,32> signature(const Composition& c) {
    std::array<int,32> out{};
    out.fill(-999);
    for(int b=0;b<countMotifBars(c);++b) {
        for(const auto& event:c.bars[static_cast<std::size_t>(b)].notes)
            out[static_cast<std::size_t>(b*16+event.step)]=event.interval;
    }
    return out;
}
}
int motifDistance(const Composition& a,const Composition& b) {
    validate(a);validate(b);
    if(a.settings.style!=b.settings.style||a.settings.bars!=b.settings.bars)
        throw std::invalid_argument("motifDistance requires same style and bars");
    const auto x=signature(a),y=signature(b);
    int score=0;
    for(int step=0;step<countMotifBars(a)*16;++step) {
        const int av=x[static_cast<std::size_t>(step)];
        const int bv=y[static_cast<std::size_t>(step)];
        if(av==bv)continue;
        score+=(av==-999||bv==-999)?2:1;
    }
    return score;
}
NoveltyResult composeNovel(const Config& config,const std::vector<Composition>& previous) {
    // Force config validation even when the history is empty.
    NoveltyResult best{compose(config),0,false,1};
    std::vector<const Composition*> recent;
    // Search backwards for eight matching musical contexts. Interleaved styles
    // must not erase the last relevant motifs from the novelty memory.
    for(std::size_t i=previous.size();i>0&&recent.size()<kHistoryWindow;--i){
        const auto& p=previous[i-1];
        if(p.settings.style!=config.style||p.settings.bars!=config.bars)continue;
        validate(p);
        recent.push_back(&p);
    }
    if(recent.empty()) {best.minimumDistance=0;best.meetsTarget=true;return best;}
    int bestScore=-1;
    for(int i=0;i<kMaxCandidates;++i){
        // No unbounded retries and no allocation inside the audio callback.
        // Candidate 0 always corresponds to the caller's exact seed.
        Config current=config;
        current.seed+=static_cast<std::uint64_t>(i)*0x9e3779b97f4a7c15ULL;
        Composition candidate=i==0?std::move(best.composition):compose(current);
        int minDistance=std::numeric_limits<int>::max();
        for(const auto* p:recent)minDistance=std::min(minDistance,motifDistance(candidate,*p));
        if(minDistance>bestScore){
            bestScore=minDistance;
            best.composition=std::move(candidate);
            best.minimumDistance=minDistance;
        }
        best.candidatesEvaluated=i+1;
        if(minDistance>=kMinimumStructuralDistance) {
            best.meetsTarget=true;
            return best;
        }
    }
    best.meetsTarget=false;
    return best;
}
} // namespace ebm::v2
