#pragma once
#include <algorithm>
namespace ebm::vst3::ui {
// Static native 1100 x 590 pixels. Zoom 150% is 1650 x 885.
constexpr int width=1100,height=590;
enum class Target {None, Style, Bars, Root, Variation, Trigger, Zoom,
                   PrevBar, NextBar, Generate, Variate, ExportLoop, ExportOneShot};
struct Hit {Target target=Target::None;};
constexpr bool inside(int x,int y,int l,int t,int w,int h) noexcept {
    return x>=l&&x<l+w&&y>=t&&y<t+h;
}
constexpr Hit hit(int x,int y) noexcept {
    if(x<0||y<0||x>=width||y>=height)return {};
    if(inside(x,y,974,25,102,35))return {Target::Zoom};
    if(inside(x,y,24,102,242,65))return {Target::Style};
    if(inside(x,y,278,102,143,65))return {Target::Bars};
    if(inside(x,y,433,102,157,65))return {Target::Root};
    if(inside(x,y,602,102,275,65))return {Target::Variation};
    if(inside(x,y,889,102,187,65))return {Target::Trigger};
    if(inside(x,y,27,212,44,38))return {Target::PrevBar};
    if(inside(x,y,160,212,44,38))return {Target::NextBar};
    if(inside(x,y,24,492,254,53))return {Target::Generate};
    if(inside(x,y,292,492,175,53))return {Target::Variate};
    if(inside(x,y,481,492,276,53))return {Target::ExportLoop};
    if(inside(x,y,771,492,305,53))return {Target::ExportOneShot};
    return {};
}
constexpr int variationValue(int x) noexcept {
    const int xx=x<618?618:(x>858?858:x);
    return ((xx-618)*100+120)/240;
}
constexpr int defaultValue(Target what) noexcept {
    if(what==Target::Variation)return 30;
    if(what==Target::Bars)return 8;
    if(what==Target::Root)return 36;
    return 0;
}
}
