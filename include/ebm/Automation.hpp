#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace ebm::host {
// Fixed-capacity host automation plan. Stepped gate/mode changes take effect
// at their exact sample offset; no dynamic storage or callback-side sorting allocation.
struct AutomationPoint {int offset=0;bool gate=false;double value=0.;};
struct AutomationPlan {
    static constexpr int capacity=256;
    std::array<AutomationPoint,capacity> points{};
    int count=0;
    bool add(int offset,bool gate,double value,int samples) noexcept {
        if(count==capacity||offset<0||(samples==0?offset!=0:offset>=samples)||
           !std::isfinite(value)||value<0.||value>1.)return false;
        points[static_cast<unsigned>(count++)]={offset,gate,value};return true;
    }
    void sort() noexcept {
        // Stable bounded insertion sort preserves the final point for duplicate offsets.
        for(int i=1;i<count;++i){auto v=points[static_cast<unsigned>(i)];int j=i;
            while(j>0&&points[static_cast<unsigned>(j-1)].offset>v.offset){
                points[static_cast<unsigned>(j)]=points[static_cast<unsigned>(j-1)];--j;}
            points[static_cast<unsigned>(j)]=v;
        }
    }
};
}
