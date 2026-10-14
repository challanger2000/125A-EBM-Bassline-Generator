#include "ebm/Novelty.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ebm::v2 {
namespace {
constexpr int kMinimumStructuralDistance = 4;
constexpr int kMinimumRhythmicDistance = 2;
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
int motifRhythmDistance(const Composition& a,const Composition& b) {
    validate(a);validate(b);
    if(a.settings.style!=b.settings.style||a.settings.bars!=b.settings.bars)
        throw std::invalid_argument("motifRhythmDistance requires same style and bars");
    const auto x=signature(a),y=signature(b);
    int changes=0;
    for(int step=0;step<countMotifBars(a)*16;++step)
        changes+=((x[static_cast<std::size_t>(step)]==-999) !=
                  (y[static_cast<std::size_t>(step)]==-999));
    return changes;
}
NoveltyResult composeNovel(const Config& config,const std::vector<Composition>& previous) {
    // Even with empty history, invalid options must not pass through.
    NoveltyResult best{compose(config)};
    std::vector<const Composition*> recent;
    for(std::size_t i=previous.size();i>0&&recent.size()<kHistoryWindow;--i){
        const auto& p=previous[i-1];
        if(p.settings.style!=config.style||p.settings.bars!=config.bars)continue;
        validate(p);
        recent.push_back(&p);
    }
    if(recent.empty()) {
        best.candidatesEvaluated=1;
        best.meetsTarget=true;
        best.meetsRhythmicTarget=true;
        return best; // First independent idea has nothing to compare against.
    }
    // Two independent gates: distinct MIDI pitch contour alone cannot certify
    // a new rhythmic idea. Never call a forced repetition 'novel'.
    // If no candidate meets both gates, report failure and return the candidate
    // with the strongest rhythmic separation, then the existing structure score.
    int bestRhythm=-1,bestStructure=-1;
    for(int i=0;i<kMaxCandidates;++i) {
        Config current=config;
        current.seed+=static_cast<std::uint64_t>(i)*0x9e3779b97f4a7c15ULL;
        Composition candidate=i==0?std::move(best.composition):compose(current);
        int minStructure=std::numeric_limits<int>::max();
        int minRhythm=std::numeric_limits<int>::max();
        for(const auto* p:recent) {
            minStructure=std::min(minStructure,motifDistance(candidate,*p));
            minRhythm=std::min(minRhythm,motifRhythmDistance(candidate,*p));
        }
        const bool rhythmPass=minRhythm>=kMinimumRhythmicDistance;
        const bool bothPass=rhythmPass&&minStructure>=kMinimumStructuralDistance;
        best.candidatesEvaluated=i+1;
        if(bothPass) {
            best.composition=std::move(candidate);
            best.minimumDistance=minStructure;
            best.minimumRhythmicDistance=minRhythm;
            best.meetsRhythmicTarget=true;
            best.meetsTarget=true;
            return best;
        }
        if(i==0 || minRhythm>bestRhythm ||
           (minRhythm==bestRhythm&&minStructure>bestStructure)) {
            bestRhythm=minRhythm;
            bestStructure=minStructure;
            best.composition=std::move(candidate);
            best.minimumDistance=minStructure;
            best.minimumRhythmicDistance=minRhythm;
            best.meetsRhythmicTarget=rhythmPass;
        }
    }
    best.meetsTarget=false;
    return best;
}
} // namespace ebm::v2
