#include "ebm/Session.hpp"
#include "ebm/MidiWriter.hpp"
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

using namespace ebm;
using namespace ebm::v2;
#define CHECK(x) do { if(!(x)){std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" " #x "\n";return 1;}} while(false)
template<typename F> bool invalid(F f){try{f();return false;}catch(const std::invalid_argument&){return true;}}
template<typename F> bool empty(F f){try{f();return false;}catch(const std::logic_error&){return true;}}
static bool sameSource(const Composition& lhs,const Composition& rhs) {
    if(lhs.settings.style!=rhs.settings.style||lhs.settings.bars!=rhs.settings.bars)return false;
    const int sourceBars=lhs.settings.style==Style::MinimalBody?1:2;
    for(int i=0;i<sourceBars;++i)if(lhs.bars[static_cast<std::size_t>(i)].notes!=rhs.bars[static_cast<std::size_t>(i)].notes)return false;
    return true;
}
int main() {
    int generations=0,variations=0,recallChecks=0,strictNovel=0;
    for(int style=0;style<3;++style)for(int bars:{4,8})for(int seed=1;seed<=10;++seed) {
        Config config;config.style=static_cast<Style>(style);config.bars=bars;
        config.rootMidi=36;config.variation=0;config.seed=static_cast<std::uint64_t>(seed*7919);
        Session session(config);
        CHECK(!session.hasCurrent());
        CHECK(empty([&]{(void)session.current();}));
        CHECK(empty([&]{(void)session.variate(15);}));
        std::set<std::uint64_t> chosenSeeds;
        for(int i=0;i<24;++i) {
            const auto info=session.generate();
            CHECK(session.hasCurrent());
            CHECK(info.selectedSeed==session.current().settings.seed);
            CHECK(info.candidatesEvaluated>=1&&info.candidatesEvaluated<=16);
            CHECK(info.minimumDistance>=0);
            CHECK(info.minimumRhythmicDistance>=0);
            if(info.meetsNoveltyTarget)CHECK(info.meetsRhythmicTarget);
            if(info.meetsNoveltyTarget)++strictNovel;
            chosenSeeds.insert(info.selectedSeed);
            const auto generated=session.current();
            const auto base=render(generated);
            CHECK(!base.notes.empty());
            const auto saved=session.snapshot();
            Session recalled;
            recalled.restore(saved);
            CHECK(toMidi(render(recalled.current()))==toMidi(base));
            const auto selectedSeed=generated.settings.seed;
            CHECK(session.variate(15));
            CHECK(session.current().settings.seed==selectedSeed);
            CHECK(sameSource(session.current(),generated));
            CHECK(render(session.current()).notes!=base.notes);
            CHECK(!session.variate(15)); // Intensity band, not a false 'new' variant.
            CHECK(session.variate(85));
            CHECK(sameSource(session.current(),generated));
            CHECK(session.variate(0));
            CHECK(toMidi(render(session.current()))==toMidi(base));
            session.setRootMidi(43);
            const auto transposed=render(session.current());
            CHECK(transposed.notes.size()==base.notes.size());
            for(std::size_t n=0;n<base.notes.size();++n){
                CHECK(transposed.notes[n].pitch==base.notes[n].pitch+7);
                CHECK(transposed.notes[n].step==base.notes[n].step);
                CHECK(transposed.notes[n].velocity==base.notes[n].velocity);
                CHECK(transposed.notes[n].duration==base.notes[n].duration);
            }
            session.setRootMidi(36);
            CHECK(toMidi(render(session.current()))==toMidi(base));
            // Generate with the currently displayed settings and the retained
            // history, not a new random phrase on VARIATE or transposition.
            Session resumed;
            resumed.restore(session.snapshot());
            CHECK(toMidi(render(resumed.current()))==toMidi(base));
            CHECK(session.options().variation==0);
            CHECK(resumed.options().variation==0);
            ++generations;++recallChecks;
            variations+=3;
        }
        CHECK(chosenSeeds.size()==24);
        CHECK(session.snapshot().history.size()==24);
        for(int j=0;j<40;++j)(void)session.generate();
        CHECK(session.snapshot().history.size()==48);
        Session replay;replay.restore(session.snapshot());
        for(int j=0;j<5;++j){
            const auto a=session.generate(),b=replay.generate();
            CHECK(a.selectedSeed==b.selectedSeed);
            CHECK(a.meetsNoveltyTarget==b.meetsNoveltyTarget);
            CHECK(a.minimumRhythmicDistance==b.minimumRhythmicDistance);
            CHECK(a.candidatesEvaluated==b.candidatesEvaluated);
            CHECK(toMidi(render(session.current()))==toMidi(render(replay.current())));
        }
        // A malformed state must fail atomically (including motif/history/cursor).
        const auto correct=toMidi(render(session.current()));
        const auto state=session.snapshot();
        SessionState bad=state;bad.version=99;
        CHECK(invalid([&]{session.restore(bad);}));
        bad=state;bad.current.seed+=1;
        CHECK(invalid([&]{session.restore(bad);}));
        bad=state;bad.current.rootMidi=100;
        CHECK(invalid([&]{session.restore(bad);}));
        bad=state;bad.history.back().bars=3;
        CHECK(invalid([&]{session.restore(bad);}));
        bad=state;bad.history.insert(bad.history.end(),2,state.history.back());
        CHECK(invalid([&]{session.restore(bad);}));
        CHECK(toMidi(render(session.current()))==correct);
        CHECK(session.snapshot().options.seed==state.options.seed);
        session.clearCurrent();
        CHECK(!session.hasCurrent()&&!session.snapshot().history.empty());
        CHECK(empty([&]{(void)session.variate(50);}));
        (void)session.generate();
        CHECK(session.hasCurrent());
        session.setStyle(static_cast<Style>((style+1)%3));
        CHECK(!session.hasCurrent());
        (void)session.generate();
        session.setBars(bars==8?4:8);
        CHECK(!session.hasCurrent());
        (void)session.generate();
        CHECK(session.current().settings.bars==(bars==8?4:8));
        const auto last=toMidi(render(session.current()));
        CHECK(invalid([&]{session.setBars(3);}));
        CHECK(invalid([&]{session.setRootMidi(100);}));
        CHECK(invalid([&]{session.setStyle(static_cast<Style>(99));}));
        CHECK(invalid([&]{(void)session.variate(200);}));
        CHECK(toMidi(render(session.current()))==last);
    }
    CHECK(generations==1440&&recallChecks==1440&&variations==4320);
    std::cout<<"PASS GENERATE="<<generations<<" VARIATE="<<variations<<
       " save/restore="<<recallChecks<<" noveltyAccepted="<<strictNovel<<
       " + invalid-state/transport-style/transpose/retained-history regressions\n";
    return 0;
}
