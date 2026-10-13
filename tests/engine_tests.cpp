#include "ebm/Composer.hpp"
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
    for(int s=0;s<3;++s) {
        for(int n=0;n<100;++n) {
            for(int bars : {4,8}) {
                for(int intensity : {0,30,75,100}) {
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
                    if(n<30 && bars==8 && intensity==75)
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
                    if(intensity==0) {
                        REQUIRE(composed.edits.empty());
                        const int motifBars=s==0?1:2;
                        for(int b=motifBars;b<bars;++b)
                            REQUIRE(composed.bars[static_cast<std::size_t>(b)].notes==
                                    composed.bars[static_cast<std::size_t>(b%motifBars)].notes);
                    } else {
                        REQUIRE(!composed.edits.empty());
                        REQUIRE(composed.bars[0].notes==repeated.bars[0].notes);
                        if(s==0) {
                            REQUIRE(composed.bars[3].notes.size()+1==composed.bars[0].notes.size());
                            if(bars==8) {
                                REQUIRE(std::any_of(composed.edits.begin(),composed.edits.end(),
                                  [](const Edit& e){return e.operation==Operation::Anticipate && e.bar==7;}));
                            }
                        }
                        if(s==1) {
                            REQUIRE(composed.bars[3].notes.size()+1==composed.bars[1].notes.size());
                            if(bars==8) {
                                REQUIRE(std::any_of(composed.edits.begin(),composed.edits.end(),
                                  [](const Edit& e){return e.operation==Operation::Anticipate && e.bar==6;}));
                            }
                        }
                        if(s==2) {
                            REQUIRE(composed.bars[3].notes.size()==composed.bars[1].notes.size());
                            REQUIRE(composed.bars[3].notes!=composed.bars[1].notes);
                            for(std::size_t k=0;k<composed.bars[1].notes.size();++k)
                                REQUIRE(composed.bars[3].notes[k].step==composed.bars[1].notes[k].step);
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
    std::cout<<"PASS: 2400 compositions (3 styles x100 seeds x2 lengths x4 variation levels),"
                " musical role/resolution invariants, real transforms, >=85/90 distinctive preview seeds, transpose/return, MIDI headers, negative tests\n";
    return 0;
}
