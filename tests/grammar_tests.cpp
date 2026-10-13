#include "ebm/Composer.hpp"
#include "ebm/MidiWriter.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ' ' << #x << '\n'; return 1; } } while(false)
using namespace ebm;
using namespace ebm::v2;

const Event& findRole(const Bar& b,int id) {
    const auto it=std::find_if(b.notes.begin(),b.notes.end(),[id](const Event& e){return e.id==id;});
    if(it==b.notes.end())throw std::runtime_error("test fixture missing event");
    return *it;
}
std::string rhythmOf(const Composition& c){
    std::string result;
    const int motifBars=c.settings.style==Style::MinimalBody?1:2;
    for(int b=0;b<motifBars;++b){
        for(const auto& e:c.bars[static_cast<std::size_t>(b)].notes){
            result += std::to_string(e.step)+",";
        }
        result += ";";
    }
    return result;
}
template<class F> bool rejects(F&& f){try{f();return false;}catch(const std::invalid_argument&){return true;}}

int main(){
    constexpr std::array<int,3> expectedRhythmFloor{{60,149,70}};
    constexpr std::array<int,3> expectedMidiFloor{{220,260,275}};
    for(int style=0;style<3;++style){
        std::set<Device> devices;
        std::set<std::string> rhythms;
        std::set<std::vector<std::uint8_t>> midiFiles;
        for(int n=0;n<300;++n){
            Config cfg;cfg.style=static_cast<Style>(style);cfg.seed=static_cast<std::uint64_t>(314159+style*1019+n);
            cfg.variation=85;
            const auto c=compose(cfg);
            CHECK(c.settings.bars==8);
            CHECK(c.bars.size()==8);
            CHECK(c.bars[0].motif=='A');
            CHECK(!c.bars[0].transformed);
            CHECK(!c.bars[1].transformed);
            devices.insert(c.bars[0].device);
            rhythms.insert(rhythmOf(c));
            midiFiles.insert(toMidi(render(c)));
            for(const auto& bar:c.bars)CHECK(bar.device==c.bars[0].device);
            if(style==0){
                CHECK(findRole(c.bars[0],1).step==0);
                const auto& source=c.bars[0];
                if(source.device==Device::PedalGap){
                    CHECK(findRole(source,4).step==8);
                    CHECK(findRole(source,5).step>=10);
                } else if(source.device==Device::DelayedBackbeat){
                    CHECK(findRole(source,4).step==12);
                    CHECK(findRole(source,5).step>=13);
                } else if(source.device==Device::CrossPulse){
                    CHECK(findRole(source,4).step==8);
                    CHECK(findRole(source,5).step>=12);
                } else CHECK(false);
            } else if(style==1){
                const auto& a=c.bars[0];const auto& b=c.bars[1];
                CHECK(findRole(a,6).step==12); // Anchors still own the kick on beat 4.
                CHECK(findRole(b,108).step==12);
                switch(a.device){
                    case Device::MotorLock:
                        CHECK(findRole(a,5).step==10);
                        CHECK(findRole(b,106).step==9);
                        CHECK(findRole(b,107).step==11);
                        break;
                    case Device::BreakAndShout:
                        CHECK(findRole(a,5).step==9);
                        CHECK(findRole(b,106).step==8);
                        CHECK(findRole(b,107).step==10);
                        break;
                    case Device::OctaveInterlock:
                        CHECK(findRole(a,5).step==8);
                        CHECK(findRole(b,106).step==9);
                        CHECK(findRole(b,107).step==11);
                        break;
                    default: CHECK(false);
                }
            } else {
                const auto& a=c.bars[0];const auto& b=c.bars[1];
                const int answerStep=findRole(b,104).step;
                const int resolutionStep=findRole(a,5).step;
                switch(a.device){
                    case Device::ClimbRelease:CHECK(resolutionStep==12 && answerStep==10);break;
                    case Device::LateSuspension:CHECK(resolutionStep==13 && answerStep==11);break;
                    case Device::BrokenAnswer:CHECK(resolutionStep==11 && answerStep==10);break;
                    default: CHECK(false);
                }
                CHECK(findRole(a,4).targetId==5);
                CHECK(findRole(a,6).targetId==101);
                CHECK(findRole(b,106).targetId==1);
            }
        }
        CHECK(devices.size()==3); // Every claimed grammar is actually reachable.
        CHECK(static_cast<int>(rhythms.size())>=expectedRhythmFloor[style]);
        CHECK(static_cast<int>(midiFiles.size())>=expectedMidiFloor[style]);
        std::cout << styleName(static_cast<Style>(style)) << ": 3/3 devices, "
                  << rhythms.size() << " onset signatures, "
                  << midiFiles.size() << " unique MIDI results in 300 seeds\n";
        Config cfg;cfg.style=static_cast<Style>(style);cfg.seed=105;
        auto bad=compose(cfg);
        const int grammar=static_cast<int>(bad.bars.front().device);
        for(auto& bar:bad.bars){
            bar.device=static_cast<Device>((grammar/3)*3+((grammar+1)%3));
        }
        CHECK(rejects([&]{validate(bad);})); // Re-labelled notes cannot fake another device.
        bad=compose(cfg);
        bad.bars.back().device=static_cast<Device>((static_cast<int>(bad.bars.front().device)/3)*3+((static_cast<int>(bad.bars.front().device)+1)%3));
        CHECK(rejects([&]{validate(bad);})); // An unexplained device switch is forbidden.
    }
    std::cout << "PASS: nine composition devices, deliberate rhythmic identities and adversarial device forgeries\n";
}
