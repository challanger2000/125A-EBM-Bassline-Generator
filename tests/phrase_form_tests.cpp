#include "ebm/Composer.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <set>
#include <vector>

#define CHECK(x) do { if(!(x)) {std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" " #x "\n";return 1;} }while(false)

using namespace ebm;
using namespace ebm::v2;

static bool changed(const Composition& c,int bar) {
    const int source=c.settings.style==Style::MinimalBody?0:bar%2;
    return c.bars[static_cast<std::size_t>(bar)].notes!=
           c.bars[static_cast<std::size_t>(source)].notes;
}

int main() {
    int checked=0, difference=0;
    for(int style=0;style<3;++style)for(int seed=1;seed<=300;++seed)
        for(int length:{4,8}) {
            Config cfg;cfg.style=static_cast<Style>(style);
            cfg.seed=static_cast<std::uint64_t>(seed*7187+style*101);
            cfg.bars=length;
            std::vector<Composition> forms;
            for(int level:{0,15,30,55,85}) {
                cfg.variation=level;
                forms.push_back(compose(cfg));
                const Composition& c=forms.back();
                validate(c);
                const int sourceCount=style==0?1:2;
                for(int b=0;b<sourceCount;++b) {
                    CHECK(!changed(c,b));
                    CHECK(!c.bars[static_cast<std::size_t>(b)].transformed);
                }
                CHECK(static_cast<int>(c.edits.size())==(level==0?0:level==15?1:level==30?2:level==55?3:4));
                if(level==0) {
                    for(int b=0;b<length;++b) CHECK(!changed(c,b));
                } else {
                    CHECK(c.edits.front().bar==length-1);
                    CHECK(c.edits.front().operation==Operation::Answer);
                    CHECK(changed(c,length-1));
                    if(length==8) {
                        // At low intensity, no random earlier deviation.
                        if(level==15) for(int b=0;b<7;++b) CHECK(!changed(c,b));
                        if(level>=30) CHECK(c.edits[1].bar==3);
                        if(level>=55) CHECK(c.edits[2].bar==(style==2?5:6));
                        if(level>=85) CHECK(c.edits[3].bar==5);
                        // Each 8-bar arrangement preserves the introduction.
                        CHECK(!changed(c,0) && !changed(c,1));
                    } else {
                        if(level>=30) CHECK(c.edits[1].bar==3);
                        if(level>=55) CHECK(c.edits[2].bar==(style==2?3:2));
                        if(level>=85) CHECK(c.edits[3].bar==(style==0||style==2?2:3));
                    }
                }
                ++checked;
            }
            // Every intensity stage creates a distinct phrase at the same seed.
            for(std::size_t i=1;i<forms.size();++i) {
                CHECK(render(forms[i]).notes!=render(forms[i-1]).notes);
                ++difference;
            }
        }
    CHECK(checked==9000);
    CHECK(difference==7200);
    std::cout<<"PASS: "<<checked<<" arrangements; "<<difference
             <<" adjacent intensity contrasts; 4/8-bar phrase-arc placement preserved\n";
}
