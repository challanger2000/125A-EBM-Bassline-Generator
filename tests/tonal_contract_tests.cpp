#include "ebm/Composer.hpp"
#include "ebm/MidiWriter.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#define CHECK(expr) do{ if(!(expr)){std::cerr<<"FAIL "<<__LINE__<<" " #expr "\n"; return 1;}}while(false)
template<class F>bool rejects(F&& fn){try{fn();return false;}catch(const std::invalid_argument&){return true;}}
int main(){
    using namespace ebm;
    using namespace ebm::v2;
    constexpr std::array<int,6> minor{{0,3,5,7,10,12}};
    int verified=0;
    for(int seed=1;seed<=400;++seed){
        for(int bars:{4,8}){
            for(int variation:{0,15,30,55,85}){
                Config cfg;cfg.style=Style::DarkElectro;cfg.bars=bars;cfg.variation=variation;
                cfg.seed=static_cast<std::uint64_t>(seed);
                const auto c=compose(cfg);
                validate(c);
                const auto rendered=render(c);
                const auto bytes=toMidi(rendered);
                CHECK(bytes.size()>22);
                const auto findId=[](const Bar& bar,int id)->const Event&{
                    return *std::find_if(bar.notes.begin(),bar.notes.end(),[id](const Event& e){return e.id==id;});
                };
                CHECK(findId(c.bars[0],3).interval>findId(c.bars[1],101).interval);
                for(const auto& bar:c.bars){
                    for(const auto& e:bar.notes)
                        if(e.role!=Role::Tension)
                            CHECK(std::find(minor.begin(),minor.end(),e.interval)!=minor.end());
                    if(bar.motif=='B')
                        CHECK(findId(bar,102).interval>findId(bar,101).interval);
                }
                ++verified;
            }
        }
    }
    // Negative 1: an out-of-scale response, still a syntactically legal MIDI pitch.
    Config cfg;cfg.style=Style::DarkElectro;cfg.seed=1;
    Composition bad=compose(cfg);
    for(auto& e:bad.bars[1].notes)if(e.id==102)e.interval=9;
    CHECK(rejects([&]{validate(bad);}));
    // Negative 2: upper answer sinks below the explicitly selected target.
    bad=compose(cfg);
    int lowerTarget=0;
    for(const auto& e:bad.bars[1].notes)if(e.id==101)lowerTarget=e.interval;
    for(auto& e:bad.bars[1].notes)if(e.id==102)e.interval=lowerTarget;
    CHECK(rejects([&]{validate(bad);}));
    // Negative 3: source summit doesn't climb above its later target.
    bad=compose(cfg);
    for(auto& e:bad.bars[0].notes)if(e.id==3)e.interval=lowerTarget;
    CHECK(rejects([&]{validate(bad);}));
    std::cout<<"PASS: "<<verified<<" dark compositions; natural-minor role constraints, directed peak/answer, three adversarial cases\n";
}
