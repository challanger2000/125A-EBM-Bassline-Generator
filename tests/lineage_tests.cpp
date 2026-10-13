#include "ebm/Composer.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if(!(x)) {std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ' ' << #x << '\n'; return 1;}}while(false)

using namespace ebm;
using namespace ebm::v2;

template <class Fun>
static bool rejected(Fun f) {
    try { f(); return false; }
    catch(const std::invalid_argument&) { return true; }
}

int main() {
    // First verify real generated history can be reconstructed for all types
    // and all musical intensity levels, on both bar counts.
    int validated=0;
    for(int style=0;style<3;++style)for(int seed=0;seed<100;++seed)
        for(int bars : {4,8})for(int level : {0,15,30,55,85,100}) {
            Config cfg;
            cfg.style=static_cast<Style>(style);
            cfg.seed=static_cast<std::uint64_t>(314159 + 1019*style + seed);
            cfg.bars=bars; cfg.variation=level;
            const Composition composition=compose(cfg);
            validate(composition);
            ++validated;
        }
    CHECK(validated==3600);

    Config cfg;
    cfg.style=Style::DarkElectro;
    cfg.seed=7;
    cfg.bars=8;
    cfg.variation=85;
    const Composition base=compose(cfg);
    CHECK(base.bars.size()==8 && base.edits.size()==4);

    // An unlogged and valid MIDI-pitch mutation in a plain repeat was accepted
    // by the preceding validator. It must now fail independently of MIDI bounds.
    auto x=base;
    const int rootInterval=x.bars[2].notes[1].interval;
    ++x.bars[2].notes[1].interval;
    CHECK(x.bars[2].notes[1].interval==rootInterval+1);
    CHECK(rejected([&]{validate(x);}));

    // Sound/articulation mutations also change the musical result even when
    // note count, onset positions, velocity bounds and tension links remain legal.
    x=base; x.bars[2].notes[1].velocity+=1;
    CHECK(rejected([&]{validate(x);}));
    x=base; x.bars[2].notes[0].duration-=1;
    CHECK(rejected([&]{validate(x);}));
    x=base; x.bars[2].notes[1].role=Role::Drive;
    CHECK(rejected([&]{validate(x);}));

    // A transformed bar cannot contain unrelated, unlogged changes.
    x=base; x.bars[3].notes[1].velocity+=1;
    CHECK(rejected([&]{validate(x);}));

    // The edit history cannot be fabricated or silently reduced.
    x=base; x.edits.clear();
    CHECK(rejected([&]{validate(x);}));
    x=base; x.edits.front().eventId=999;
    CHECK(rejected([&]{validate(x);}));
    x=base; x.edits.front().previousInterval+=1;
    CHECK(rejected([&]{validate(x);}));
    x=base; x.edits.front().newInterval+=1;
    CHECK(rejected([&]{validate(x);}));
    x=base; x.edits.push_back(x.edits.front());
    CHECK(rejected([&]{validate(x);}));
    x=base; x.edits.front().operation=Operation::Repeat;
    CHECK(rejected([&]{validate(x);}));

    // Even a valid note edit cannot be placed in the untouched source motif.
    x=base; x.edits.front().bar=0;
    CHECK(rejected([&]{validate(x);}));
    std::cout << "PASS: " << validated << " valid lineage replays and 12 hostile event/history edits rejected\n";
}
