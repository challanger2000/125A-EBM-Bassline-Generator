#include "ebm/Composer.hpp"
#include "ebm/Novelty.hpp"
#include "ebm/MidiWriter.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#define REQUIRE(x) do { if (!(x)) {std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" " #x "\n";return 1;} }while(false)

template <class Fun> bool rejects(Fun&& f) {
    try {f();return false;} catch(const std::invalid_argument&) {return true;}
}
int main() {
    using namespace ebm;
    using namespace ebm::v2;
    std::set<std::vector<std::uint8_t>> sampleHashes;
    int comparedIntensityCases=0;
    for(int s=0;s<3;++s) {
        for(int n=0;n<100;++n) {
            for(int bars : {4,8}) {
                for(int intensity : {0,15,30,55,85}) {
                    Config cfg;cfg.style=static_cast<Style>(s);cfg.seed=uint64_t(314159+s*1019+n);
                    cfg.bars=bars;cfg.variation=intensity;
                    const auto composed=compose(cfg), repeated=compose(cfg);
                    REQUIRE(composed.bars==repeated.bars);
                    REQUIRE(composed.settings.bars==bars);
                    REQUIRE(composed.bars.size()==static_cast<std::size_t>(bars));
                    REQUIRE(composed.edits.size()==repeated.edits.size());
                    validate(composed);
                    const auto phrase=render(composed);
                    REQUIRE(phrase.bars==bars);
                    REQUIRE(!phrase.notes.empty());
                    int end=0;
                    for(const auto& note:phrase.notes){
                        REQUIRE(note.step>=end);
                        REQUIRE(note.duration>0 && note.step+note.duration<=bars*16);
                        REQUIRE(note.pitch>=0 && note.pitch<=127);
                        REQUIRE(note.velocity>=1 && note.velocity<=127);
                        end=note.step+note.duration;
                    }
                    REQUIRE(phrase.notes==render(repeated).notes);
                    const auto midi=toMidi(phrase);
                    REQUIRE(std::string(midi.begin(),midi.begin()+4)=="MThd");
                    REQUIRE(std::string(midi.begin()+14,midi.begin()+18)=="MTrk");
                    if(n<30 && bars==8 && intensity==85)
                        (void)sampleHashes.insert(midi);
                    cfg.rootMidi=43;
                    const auto transposed=compose(cfg);
                    const auto shifted=render(transposed);
                    REQUIRE(phrase.notes.size()==shifted.notes.size());
                    for(std::size_t j=0;j<phrase.notes.size();++j) {
                        REQUIRE(shifted.notes[j].pitch-phrase.notes[j].pitch==7);
                        REQUIRE(shifted.notes[j].step==phrase.notes[j].step);
                        REQUIRE(shifted.notes[j].duration==phrase.notes[j].duration);
                        REQUIRE(shifted.notes[j].velocity==phrase.notes[j].velocity);
                    }
                    cfg.rootMidi=36;
                    REQUIRE(render(compose(cfg)).notes==phrase.notes); // C -> G -> C restore
                    if(intensity>0) {
                        const int expectedEdits=(intensity<25?1:(intensity<50?2:(intensity<75?3:4)))+((intensity>=75&&s!=0)?1:0);
                        REQUIRE(static_cast<int>(composed.edits.size())==expectedEdits);
                    }
                    if(intensity==0) {
                        REQUIRE(composed.edits.empty());
                        const int motifBars=s==0?1:2;
                        for(int b=motifBars;b<bars;++b)
                            REQUIRE(composed.bars[static_cast<std::size_t>(b)].notes==
                                    composed.bars[static_cast<std::size_t>(b%motifBars)].notes);
                    } else {
                        REQUIRE(!composed.edits.empty());
                        REQUIRE(composed.bars[0].notes==repeated.bars[0].notes);
                        const int sourceMotifBars=s==0?1:2;
                        for(int j=0;j<sourceMotifBars;++j) {
                            Config unvaried=cfg;unvaried.variation=0;
                            const auto baseline=compose(unvaried);
                            REQUIRE(composed.bars[static_cast<std::size_t>(j)].notes==
                                    baseline.bars[static_cast<std::size_t>(j)].notes);
                        }
                    }
                    for(const auto& edit:composed.edits) {
                        REQUIRE(edit.bar>=0&&edit.bar<bars);
                        if(edit.operation==Operation::Anticipate) {
                            REQUIRE(edit.newStep==edit.previousStep-1);
                            const auto& notes=composed.bars[static_cast<std::size_t>(edit.bar)].notes;
                            REQUIRE(std::none_of(notes.begin(),notes.end(),[&](const Event& e){return e.step==edit.previousStep;}));
                        }
                    }
                }
            }
        }
    }
    for(int s=0;s<3;++s)for(int n=0;n<100;++n)for(int bars:{4,8}) {
        Config cfg;cfg.style=static_cast<Style>(s);cfg.seed=uint64_t(314159+s*1019+n);
        cfg.bars=bars;
        std::vector<Phrase> levels;
        for(int v:{0,15,30,55,85}) { cfg.variation=v; levels.push_back(render(compose(cfg))); }
        for(std::size_t j=1;j<levels.size();++j) {
            REQUIRE(levels[j].notes!=levels[j-1].notes);
            ++comparedIntensityCases;
        }
    }
    REQUIRE(comparedIntensityCases==2400);
    // The future GENERATE action must compare musical structures, not MIDI bytes.
    // Same notes transposed are not a different motif; different contours are.
    for(int s=0;s<3;++s){
        Config cfg;cfg.style=static_cast<Style>(s);cfg.variation=50;
        std::vector<Composition> recent;
        int approved=0;
        for(int i=1;i<=100;++i){
            cfg.seed=static_cast<std::uint64_t>(i*1237);
            const auto chosen=composeNovel(cfg,recent);
            const auto replay=composeNovel(cfg,recent);
            REQUIRE(render(chosen.composition).notes==render(replay.composition).notes);
            REQUIRE(chosen.meetsTarget==replay.meetsTarget);
            REQUIRE(chosen.candidatesEvaluated>=1&&chosen.candidatesEvaluated<=16);
            if(chosen.meetsTarget){
                REQUIRE(recent.empty()||(chosen.minimumDistance>=4&&chosen.minimumRhythmicDistance>=2));
                ++approved;
            }
            if(!recent.empty()){
                REQUIRE(chosen.minimumDistance>=0);
                int naiveMinimum=1000000;
                const std::size_t start=recent.size()>8?recent.size()-8:0;
                for(std::size_t h=start;h<recent.size();++h)
                    naiveMinimum=std::min(naiveMinimum,motifDistance(compose(cfg),recent[h]));
                // A dual-rhythm/contour selector may deliberately reject the naive pitch-only result.
                // Preserve direct first-candidate equivalence, and assert both independent
                // novelty gates when an accepted candidate is returned.
                if(chosen.candidatesEvaluated==1)
                    REQUIRE(chosen.minimumDistance==naiveMinimum);
                else {
                    REQUIRE(chosen.minimumDistance>=0);
                    REQUIRE(chosen.minimumRhythmicDistance>=0);
                    if(chosen.meetsTarget)
                        REQUIRE(chosen.minimumDistance>=4&&chosen.minimumRhythmicDistance>=2);
                }
            }
            recent.push_back(chosen.composition);
        }
        REQUIRE(approved>=98); // Empirical gate on known fixed-seed 100-example fixture.
        if(s==0){
            Config variant=cfg;variant.rootMidi=43;
            REQUIRE(motifDistance(compose(cfg),compose(variant))==0);
        }
    }
    // Intensity affects a later arrangement but does not rewrite source motif.
    // The structural measure intentionally ignores velocity and transposition.
    {
        Config a;a.style=Style::MinimalBody;a.seed=1234;a.variation=0;
        const auto p=compose(a);
        a.variation=85;
        const auto altered=compose(a);
        REQUIRE(motifDistance(p,altered)==0);
        a.rootMidi=43;
        REQUIRE(motifDistance(p,compose(a))==0);
        // Older same-style phrases remain in the window when other styles are
        // interleaved, even if the immediate last eight items do not match.
        std::vector<Composition> interleaved{p};
        for(int j=0;j<20;++j){
            Config c;c.style=Style::ClassicEBM;c.seed=static_cast<unsigned>(j+500);
            interleaved.push_back(compose(c));
        }
        Config again;again.style=Style::MinimalBody;again.seed=1234;
        const auto selected=composeNovel(again,interleaved);
        REQUIRE(selected.minimumDistance>=4);
        REQUIRE(selected.candidatesEvaluated>=2);
        REQUIRE(selected.meetsTarget);
        REQUIRE(motifDistance(selected.composition,p)==selected.minimumDistance);
    }
    // Unique MIDI content is a diagnostic over seeds, not guaranteed by a finite grammar.
    // Reject style-collapse: the deliberately varied 90-sample corpus must contain at least 85 distinct phrases.
    REQUIRE(sampleHashes.size()>=85);
    Config cfg;cfg.style=Style::DarkElectro;
    Composition c=compose(cfg);
    // Real adversarial tests: invalid placement, interval/pitch target, operation record.
    c.bars[0].notes.front().duration=16;
    REQUIRE(rejects([&]{validate(c);}));
    c=compose(cfg);
    auto tension=std::find_if(c.bars[0].notes.begin(),c.bars[0].notes.end(),
         [](const Event& e){return e.role==Role::Tension;});
    REQUIRE(tension!=c.bars[0].notes.end());
    tension->targetId=999;
    REQUIRE(rejects([&]{validate(c);}));
    c=compose(cfg);c.bars[0].notes.front().id=999;
    REQUIRE(rejects([&]{validate(c);}));
    c=compose(cfg);c.kickSteps[1]=5;
    REQUIRE(rejects([&]{validate(c);}));
    c=compose(cfg);c.settings.bars=4;
    REQUIRE(rejects([&]{validate(c);}));
    c=compose(cfg);c.bars[0].notes.front().step=1;
    REQUIRE(rejects([&]{validate(c);}));
    for(int invalid : {0,1,3,16}) {
        cfg.bars=invalid;REQUIRE(rejects([&]{(void)compose(cfg);}));
    }
    cfg.bars=8;cfg.rootMidi=23;REQUIRE(rejects([&]{(void)compose(cfg);}));
    cfg.rootMidi=61;REQUIRE(rejects([&]{(void)compose(cfg);}));
    cfg.rootMidi=36;cfg.variation=-1;REQUIRE(rejects([&]{(void)compose(cfg);}));
    cfg.variation=101;REQUIRE(rejects([&]{(void)compose(cfg);}));
    cfg.variation=30;cfg.style=static_cast<Style>(99);
    REQUIRE(rejects([&]{(void)compose(cfg);}));
    REQUIRE(rejects([&]{(void)toMidi(Phrase{4,{{0,36,100,3},{2,36,100,1}}});}));
    std::cout<<"PASS: 3000 compositions (3 styles x100 seeds x2 lengths x5 variation levels),"
                " 2400 adjacent intensity checks, musical role/resolution invariants, real transforms, >=85/90 distinctive preview seeds, transpose/return, MIDI headers, negative tests\n";
    return 0;
}
