#include "ebm/Composer.hpp"
#include "ebm/Session.hpp"
#include "ebm/MidiWriter.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

using namespace ebm;
using namespace ebm::v2;
#define CHECK(x) do { if(!(x)) {std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" "#x"\n"; return 1;} } while(false)
template<class F> bool rejects(F f){try{f();return false;}catch(const std::invalid_argument&){return true;}}
static bool sameSource(const Composition& a,const Composition& b) {
    const int n=a.settings.style==Style::MinimalBody?1:2;
    for(int i=0;i<n;++i)if(a.bars[static_cast<std::size_t>(i)].notes!=b.bars[static_cast<std::size_t>(i)].notes)return false;
    return true;
}
int main(){
    int compositions=0,cycles=0,recalls=0;
    for(int style=0;style<3;++style)for(int bars:{4,8})
        for(int seed=1;seed<=55;++seed)for(int strength:{15,35,60,85}){
            Config cfg;cfg.style=static_cast<Style>(style);cfg.bars=bars;
            cfg.seed=static_cast<std::uint64_t>(seed*12017+style*17);
            cfg.variation=strength;cfg.rootMidi=36;
            const auto base=compose(cfg);
            CHECK(render(base).notes==render(composeAlternative(cfg,0)).notes);
            std::set<std::vector<std::uint8_t>> distinct;
            for(int alternative=0;alternative<=4;++alternative){
                const auto c=composeAlternative(cfg,alternative);
                CHECK(c.settings.seed==cfg.seed);
                CHECK(sameSource(c,base));
                validate(c);
                const auto midi=toMidi(render(c));
                if(!distinct.insert(midi).second){std::cerr<<"DUP style="<<style<<" bars="<<bars<<" seed="<<seed<<" strength="<<strength<<" alternative="<<alternative<<"\n";return 1;} // Five genuinely different phrases.
                CHECK(c.edits.size()==base.edits.size()+(alternative?1U:0U));
                if(alternative)CHECK(c.edits.back().bar>= (style==0?1:2));
                ++compositions;
            }
            Session session(cfg);
            const auto info=session.generate();
            CHECK(info.selectedSeed==session.current().settings.seed);
            const auto selected=session.current().settings;
            std::set<std::vector<std::uint8_t>> seen;
            seen.insert(toMidi(render(session.current())));
            for(int i=1;i<=4;++i){
                CHECK(session.nextVariant());
                CHECK(session.alternative()==static_cast<std::uint32_t>(i));
                CHECK(session.current().settings.seed==selected.seed);
                CHECK(sameSource(session.current(),compose(selected)));
                CHECK(seen.insert(toMidi(render(session.current()))).second);
                const auto saved=session.snapshot();
                Session restored;
                restored.restore(saved);
                CHECK(toMidi(render(restored.current()))==toMidi(render(session.current())));
                CHECK(restored.alternative()==session.alternative());
                CHECK(restored.nextVariant());
                const auto want=composeAlternative(selected,(i+1)%5);
                CHECK(toMidi(render(restored.current()))==toMidi(render(want)));
                ++recalls;
                // Exact transpose and inverse, retaining chosen alternative.
                const auto unshifted=render(session.current());
                session.setRootMidi(43);
                const auto shift=render(session.current());
                CHECK(shift.notes.size()==unshifted.notes.size());
                for(std::size_t k=0;k<shift.notes.size();++k) {
                    CHECK(shift.notes[k].pitch==unshifted.notes[k].pitch+7);
                    CHECK(shift.notes[k].step==unshifted.notes[k].step);
                    CHECK(shift.notes[k].duration==unshifted.notes[k].duration);
                    CHECK(shift.notes[k].velocity==unshifted.notes[k].velocity);
                }
                session.setRootMidi(36);
                CHECK(toMidi(render(session.current()))==toMidi(unshifted));
                ++cycles;
            }
            CHECK(session.nextVariant());
            CHECK(session.alternative()==0);
            CHECK(toMidi(render(session.current()))==toMidi(render(compose(selected))));
            // Same numeric intensity setter restores the canonical variant.
            CHECK(session.nextVariant());
            CHECK(session.variate(strength));
            CHECK(session.alternative()==0);
            CHECK(toMidi(render(session.current()))==toMidi(render(compose(selected))));
            // Malformed snapshot fails atomically including cursor and melody.
            const auto good=session.snapshot();
            const auto before=toMidi(render(session.current()));
            auto bad=good;bad.alternative=5;
            CHECK(rejects([&]{session.restore(bad);}));
            CHECK(toMidi(render(session.current()))==before);
            CHECK(session.snapshot().alternative==0);
            bad=good;bad.version=3;
            CHECK(rejects([&]{session.restore(bad);}));
            CHECK(toMidi(render(session.current()))==before);
            bad=good;bad.alternative=2;bad.current.variation=0;
            CHECK(rejects([&]{session.restore(bad);}));
            CHECK(toMidi(render(session.current()))==before);
            ++recalls;
        }
    // Neutral intensity may neither rewrite the riff nor silently raise strength.
    for(int style=0;style<3;++style){
        Config c;c.style=static_cast<Style>(style);c.variation=0;
        Session s(c);s.generate();
        const auto before=toMidi(render(s.current()));
        CHECK(!s.nextVariant());CHECK(s.alternative()==0);
        CHECK(toMidi(render(s.current()))==before);
        CHECK(s.snapshot().options.variation==0);
        CHECK(rejects([&]{(void)composeAlternative(c,1);}));
        auto old=s.snapshot();old.version=1;
        Session migrate;migrate.restore(old);
        CHECK(toMidi(render(migrate.current()))==before);
        CHECK(migrate.snapshot().version==2);
        old.alternative=3;
        CHECK(rejects([&]{migrate.restore(old);}));
        s.variate(35);CHECK(s.nextVariant());
        s.variate(0);CHECK(s.alternative()==0);
        CHECK(toMidi(render(s.current()))==before);
    }
    Config invalid;invalid.variation=50;
    CHECK(rejects([&]{(void)composeAlternative(invalid,-1);}));
    CHECK(rejects([&]{(void)composeAlternative(invalid,5);}));
    CHECK(compositions==3*2*55*4*5);
    std::cout<<"PASS alternative compositions="<<compositions<<" variant cycles="<<cycles
             <<" restore/negative checks="<<recalls<<"; source identity, MIDI,\n"
             <<" 0% neutral, v1->v2 migration, transpose, safety preserved\n";
}
