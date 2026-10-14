#include "ebm/Composer.hpp"
#include "ebm/MidiWriter.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#define CHECK(expr) do { if(!(expr)) {std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" " #expr "\n";return 1;}} while(false)
int main() {
    using namespace ebm;
    using namespace ebm::v2;
    int total=0,trimmed=0,untouched=0,loopLeads=0;
    // Cross both phrase lengths, all grammars, all variation bands, and
    // all five intentional VARIATE alternatives (one-shot and loop output).
    for(int s=0;s<3;++s) for(int bars:{4,8})
      for(int seed=0;seed<300;++seed) for(int v:{0,15,55,85})
        for(int alt=0;alt<5;++alt) {
            if(v==0 && alt>0)continue; // neutral level explicitly has no alternatives
            Config cfg;cfg.style=static_cast<Style>(s);cfg.bars=bars;
            cfg.seed=static_cast<std::uint64_t>(10000+seed);cfg.variation=v;
            const auto composition=composeAlternative(cfg,alt);
            const auto loop=render(composition,RenderEnding::Loopable);
            const auto def=render(composition);
            CHECK(loop.notes==def.notes);
            const auto single=render(composition,RenderEnding::OneShot);
            CHECK(loop.bars==single.bars && loop.bars==bars);
            // Independently derive which note(s) are addressed to the next
            // phrase rather than present in the finite MIDI output.
            std::vector<Note> expected;
            int crossLead=0;
            for(std::size_t bi=0;bi<composition.bars.size();++bi) {
                for(const auto& e:composition.bars[bi].notes) {
                    const Note note{static_cast<int>(bi)*16+e.step,
                                    cfg.rootMidi+e.interval,e.velocity,e.duration};
                    if(bi+1==composition.bars.size() &&
                       e.role==Role::Tension && e.targetBarDelta==1) {
                        CHECK(e.step>=14 && e.step<=15 && e.duration==1);
                        ++crossLead;
                    } else expected.push_back(note);
                }
            }
            CHECK(single.notes==expected);
            CHECK(static_cast<int>(loop.notes.size()-single.notes.size())==crossLead);
            CHECK(crossLead<=1); // no accidental polyphonic final embellishments
            if(crossLead) {++trimmed;loopLeads+=crossLead;} else ++untouched;
            // Neither mode causes malformed notes or truncates the phrase.
            CHECK(!single.notes.empty());
            (void)toMidi(single);(void)toMidi(loop);
            ++total;
        }
    CHECK(total==28800 && loopLeads==trimmed && untouched+trimmed==total);
    // The export choice does not alter the source graph or edit lineage.
    Config cfg;cfg.style=Style::DarkElectro;cfg.seed=10042;cfg.variation=85;
    auto original=compose(cfg);
    const auto preserved=original;
    (void)render(original,RenderEnding::OneShot);
    CHECK(original.bars==preserved.bars && original.edits.size()==preserved.edits.size());
    bool refused=false;
    try{(void)render(original,static_cast<RenderEnding>(99));}catch(const std::invalid_argument&){refused=true;}
    CHECK(refused);
    // Corruption cannot be hidden by switching to the one-shot path.
    original.bars[0].notes.front().duration=99;
    refused=false;
    try{(void)render(original,RenderEnding::OneShot);}catch(const std::invalid_argument&){refused=true;}
    CHECK(refused);
    std::cout<<"PASS: "<<total<<" cases, one-shot trims "<<trimmed
             <<" valid cross-loop pickups, leaves "<<untouched
             <<" phrases unmodified, preserves loop mode, MIDI validity and ancestry\n";
}
