#pragma once
#include "ebm/Engine.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace ebm::edit {
constexpr int kMaxBars=8, kStepsPerBar=16, kMaxSteps=kMaxBars*kStepsPerBar;
enum class Scale : std::uint8_t { Chromatic=0, Major=1, NaturalMinor=2 };
struct Step {
    bool on=false;
    std::uint8_t pitch=36,velocity=100,length=1;
    bool locked=false;
};
struct Pattern {
    bool enabled=false; // False means legacy composed session without manual overlay.
    int bars=8, songStartBar=2;
    Scale scale=Scale::Chromatic;
    std::array<Step,kMaxSteps> steps{};
};
inline bool valid(const Pattern& p) noexcept {
    if(p.bars!=1&&p.bars!=2&&p.bars!=4&&p.bars!=8)return false;
    if(p.songStartBar<1||p.songStartBar>9999||static_cast<int>(p.scale)>2)return false;
    for(const auto& s:p.steps)
        if(s.pitch>127||s.velocity<1||s.velocity>127||s.length<1||s.length>128)return false;
    return true;
}
inline bool inScale(int midi,int tonic,Scale scale) noexcept {
    if(midi<0||midi>127)return false;
    if(scale==Scale::Chromatic)return true;
    static constexpr int major[]={0,2,4,5,7,9,11};
    static constexpr int minor[]={0,2,3,5,7,8,10};
    const int interval=(midi-tonic%12+120)%12;
    for(int n:scale==Scale::Major?major:minor)if(n==interval)return true;
    return false;
}
inline int nearestPitch(int midi,int tonic,Scale scale) noexcept {
    midi=std::clamp(midi,0,127);
    for(int distance=0;distance<=12;++distance){
        if(midi+distance<=127&&inScale(midi+distance,tonic,scale))return midi+distance;
        if(midi-distance>=0&&inScale(midi-distance,tonic,scale))return midi-distance;
    }
    return midi;
}
// Deliberate monophonic policy: a new NoteOn truncates the previous gate;
// every NoteOff is inside the selected exact export length.
inline Phrase toPhrase(const Pattern& p) {
    Phrase out{p.bars,{}};
    if(!valid(p))return Phrase{};
    const int count=p.bars*kStepsPerBar;
    for(int i=0;i<count;++i){
        const auto& s=p.steps[static_cast<std::size_t>(i)];
        if(!s.on)continue;
        int next=count;
        for(int j=i+1;j<count;++j)if(p.steps[static_cast<std::size_t>(j)].on){next=j;break;}
        out.notes.push_back(Note{i,s.pitch,s.velocity,std::min<int>({s.length,next-i,count-i})});
    }
    return out;
}
inline Pattern fromPhrase(const Phrase& phrase,int bars,Scale scale=Scale::Chromatic,int start=2){
    Pattern result{};
    result.enabled=true;result.bars=bars;result.scale=scale;result.songStartBar=start;
    for(const auto& n:phrase.notes){
        if(n.step<0||n.step>=bars*16||n.pitch<0||n.pitch>127||n.velocity<1||n.velocity>127)continue;
        auto& s=result.steps[static_cast<std::size_t>(n.step)];
        s.on=true;s.pitch=static_cast<std::uint8_t>(n.pitch);
        s.velocity=static_cast<std::uint8_t>(n.velocity);
        s.length=static_cast<std::uint8_t>(std::clamp(n.duration,1,128));
    }
    return result;
}
inline bool toggle(Pattern& p,int step) noexcept {
    if(!valid(p)||step<0||step>=p.bars*16||p.steps[static_cast<std::size_t>(step)].locked)return false;
    p.steps[static_cast<std::size_t>(step)].on=!p.steps[static_cast<std::size_t>(step)].on;
    p.enabled=true;return true;
}
inline bool modify(Pattern& p,int step,int pitch,int velocity,int length) noexcept {
    if(!valid(p)||step<0||step>=p.bars*16||pitch<0||pitch>127||velocity<1||velocity>127||length<1||length>128||p.steps[static_cast<std::size_t>(step)].locked)return false;
    auto& s=p.steps[static_cast<std::size_t>(step)];
    s.on=true;s.pitch=static_cast<std::uint8_t>(pitch);s.velocity=static_cast<std::uint8_t>(velocity);
    s.length=static_cast<std::uint8_t>(length);p.enabled=true;return true;
}
inline bool copyBar(Pattern& p,int from,int to) noexcept {
    if(!valid(p)||from<0||from>=p.bars||to<0||to>=p.bars)return false;
    for(int i=0;i<16;++i)if(p.steps[static_cast<std::size_t>(to*16+i)].locked)return false;
    const auto copy=p.steps;
    for(int i=0;i<16;++i)p.steps[static_cast<std::size_t>(to*16+i)]=copy[static_cast<std::size_t>(from*16+i)];
    p.enabled=true;return true;
}
inline bool clear(Pattern& p) noexcept {
    if(!valid(p))return false;
    for(auto& s:p.steps)if(!s.locked)s.on=false;
    p.enabled=true;return true;
}
} // namespace ebm::edit
