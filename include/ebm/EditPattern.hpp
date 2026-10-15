#pragma once
#include "ebm/Engine.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace ebm::edit {
constexpr int kMaxBars=8, kStepsPerBar=16, kMaxSteps=kMaxBars*kStepsPerBar;
// Values 0..2 are part of the existing version-2 project state; never reorder.
enum class Scale : std::uint8_t {
    Chromatic=0, Major=1, NaturalMinor=2, HarmonicMinor=3,
    MelodicMinor=4, Dorian=5, Phrygian=6, Mixolydian=7,
    Lydian=8, MinorPentatonic=9
};
inline constexpr int kScaleCount=10;
// Studio One uses middle C (MIDI 60) = C3 in its piano roll.
inline constexpr int studioOneOctave(int midi) noexcept {return midi/12-2;}
inline const char* scaleName(Scale scale) noexcept {
    switch(scale){
    case Scale::Chromatic:return "CHROMATIC";
    case Scale::Major:return "MAJOR";
    case Scale::NaturalMinor:return "NATURAL MINOR";
    case Scale::HarmonicMinor:return "HARMONIC MINOR";
    case Scale::MelodicMinor:return "MELODIC MINOR";
    case Scale::Dorian:return "DORIAN";
    case Scale::Phrygian:return "PHRYGIAN";
    case Scale::Mixolydian:return "MIXOLYDIAN";
    case Scale::Lydian:return "LYDIAN";
    case Scale::MinorPentatonic:return "MINOR PENTATONIC";
    }
    return "INVALID";
}
struct Step {
    bool on=false;
    std::uint8_t pitch=48,velocity=100,length=1;
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
    if(p.songStartBar<1||p.songStartBar>9999||static_cast<int>(p.scale)>=kScaleCount)return false;
    for(const auto& s:p.steps)
        if(s.pitch>127||s.velocity<1||s.velocity>127||s.length<1||s.length>128)return false;
    return true;
}
inline bool inScale(int midi,int tonic,Scale scale) noexcept {
    if(midi<0||midi>127||static_cast<int>(scale)>=kScaleCount)return false;
    if(scale==Scale::Chromatic)return true;
    static constexpr int intervals[9][12] = {
      {0,2,4,5,7,9,11,-1,-1,-1,-1,-1},   // Major
      {0,2,3,5,7,8,10,-1,-1,-1,-1,-1},   // Natural minor
      {0,2,3,5,7,8,11,-1,-1,-1,-1,-1},   // Harmonic minor
      {0,2,3,5,7,9,11,-1,-1,-1,-1,-1},   // Melodic minor (ascending)
      {0,2,3,5,7,9,10,-1,-1,-1,-1,-1},   // Dorian
      {0,1,3,5,7,8,10,-1,-1,-1,-1,-1},   // Phrygian
      {0,2,4,5,7,9,10,-1,-1,-1,-1,-1},   // Mixolydian
      {0,2,4,6,7,9,11,-1,-1,-1,-1,-1},   // Lydian
      {0,3,5,7,10,-1,-1,-1,-1,-1,-1,-1} // Minor pentatonic
    };
    const int degree=((midi-(tonic%12))%12+12)%12;
    for(const int v:intervals[static_cast<int>(scale)-1])if(v==degree)return true;
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
// Seed-indexed, curated motoric EBM rhythms. The original v2 composer is
// intentionally untouched: existing EBM1/EBM2 projects and their saved seeds
// must never silently change sound on project recall. This new phrase treatment
// is applied only when a user presses GENERATE for CLASSIC EBM, before the
// existing per-step LOCK overlay and scale conformance. Control thread only.
// The eight motifs emphasize continuous eighth-note motion, selective 16th
// doubles and syncopated responses, while keeping the tonic dominant.
inline bool generateClassicDrive(Pattern& p,int root,std::uint64_t seed) noexcept {
    if(!valid(p)||root<24||root>60)return false;
    // Each 16-bit mask describes one bar at 16 steps/4 beats. Bit n is a NoteOn
    // at sixteenth n. A and B are related, but not carbon copies.
    static constexpr std::array<std::uint16_t,8> motifA{{
        0xD5D5u, // 0,2,4,6,7,8,10,12,14,15  (motor + turnaround)
        0xD55Du, // 0,2,3,4,6,8,10,12,14,15  (early push)
        0xD775u, // 0,2,4,5,6,8,9,10,12,14,15 (offbeat tension)
        0xD5D7u, // 0,1,2,4,6,7,8,10,12,14,15 (attack double)
        0x75D5u, // 0,2,4,6,7,8,10,12,13,14  (late double)
        0xD555u, // 0,2,4,6,8,10,12,14,15    (relentless eighths)
        0xD5DDu, // 0,2,3,4,6,7,8,10,12,14,15 (gallop response)
        0xD755u  // 0,2,4,6,8,9,10,12,14,15  (weak-beat drive)
    }};
    static constexpr std::array<std::uint16_t,8> motifB{{
        0x55D5u, // answer ends open for next downbeat
        0xD5B5u, // extra push on the second beat
        0xD5D5u, // straight motor answer
        0x55DDu, // clipped final sixteenth
        0xD755u, // repeated syncopated response
        0xD775u, // stronger two-beat push
        0xD555u, // stripped motor answer
        0xD5D7u  // machine double
    }};
    const int grammar=static_cast<int>(seed%motifA.size());
    const int answerVariant=static_cast<int>((seed/8)%motifB.size());
    const int rhythmicAccent=static_cast<int>((seed/8)%4);
    for(int bar=0;bar<p.bars;++bar){
        std::uint16_t mask=bar%2==0?motifA[static_cast<std::size_t>(grammar)]
                                     :motifB[static_cast<std::size_t>((grammar+answerVariant)%8)];
        // Four distinct seed-controlled weak-step treatments *per* grammar.
        // No RNG on the audio thread and no drift when a project is reloaded.
        if(rhythmicAccent>0){
            constexpr std::array<int,4> turnaround{{0,5,11,13}};
            mask=static_cast<std::uint16_t>(mask ^ (1u<<turnaround[static_cast<std::size_t>(rhythmicAccent)]));
        }
        // Build complete A/B phrases first. Four-/eight-bar arrangements keep
        // their pulse and add a single, deliberate turnaround in later bars.
        if(bar>1 && (bar%4)==3) mask=static_cast<std::uint16_t>(mask ^ 0x4000u);
        for(int step=0;step<16;++step){
            auto& s=p.steps[static_cast<std::size_t>(bar*16+step)];
            if(s.locked)continue;
            const bool active=(mask&(static_cast<std::uint16_t>(1u)<<step))!=0;
            s.on=active;
            if(!active)continue;
            const bool downbeat=(step%4)==0;
            const bool offbeat=(step%2)==1;
            int note=root;
            // One accent/answer tone at most per bar. Root remains >= 80%.
            if(step==10 && (bar+grammar)%3==0)note=root+7;
            if(step==14 && (bar+grammar)%4==2)note=root+12;
            s.pitch=static_cast<std::uint8_t>(std::clamp(note,0,127));
            const int shape=((grammar+bar+step)%3)*3;
            s.velocity=static_cast<std::uint8_t>(downbeat?115+shape:
                                                 offbeat?92+shape:104+shape);
            // Note-off occurs at/before next NoteOn. No overlapping MIDI bass notes.
            s.length=static_cast<std::uint8_t>(offbeat?1:2);
        }
    }
    p.enabled=true;
    return true;
}

// Generator phrases are composed chromatically by the original grammar.
// Quantize only unlocked, active notes on the control thread, never in process().
inline void conformToScale(Pattern& p,int tonic) noexcept {
    for(auto& st:p.steps)if(st.on&&!st.locked)
        st.pitch=static_cast<std::uint8_t>(nearestPitch(st.pitch,tonic,p.scale));
}

// GUI VARIATE operates on the ACTUAL selected 1/2/4/8-bar pattern, not on
// hidden source bars 3/4 of the four-bar composer. One visible edit per click,
// with four musical operations and a safe accent fallback. Entirely non-RT.
enum class VariationEdit {None,Accent,Pitch,Gate,Timing};
inline VariationEdit variateVisible(Pattern& p,int intensity,int turn,int tonic,int bar) noexcept {
    if(!valid(p)||intensity<=0||intensity>100||bar<0||bar>=p.bars)return VariationEdit::None;
    int active[16]{};int count=0;
    for(int col=0;col<16;++col){
        const int ix=bar*16+col;
        if(p.steps[static_cast<std::size_t>(ix)].on&&!p.steps[static_cast<std::size_t>(ix)].locked)
            active[count++]=ix;
    }
    if(!count)return VariationEdit::None;
    const int candidate=active[((turn-1)%count+count)%count];
    const auto tryEdit=[&](int op) noexcept -> VariationEdit {
        const auto& original=p.steps[static_cast<std::size_t>(candidate)];
        if(op==1){ // accent or de-accent: always an audible MIDI velocity change
            const int delta=std::max(1,(intensity*24+50)/100);
            const int before=original.velocity;
            int after=before+delta<=127?before+delta:before-delta;
            after=std::clamp(after,1,127);
            if(after==before)after=before==127?126:before+1;
            p.steps[static_cast<std::size_t>(candidate)].velocity=static_cast<std::uint8_t>(after);
            return VariationEdit::Accent;
        }
        if(op==2){
            for(int distance=1;distance<=12;++distance){
                for(int sign:{+1,-1}){
                    const int value=static_cast<int>(original.pitch)+sign*distance;
                    if(value!=original.pitch && inScale(value,tonic,p.scale)){
                        p.steps[static_cast<std::size_t>(candidate)].pitch=static_cast<std::uint8_t>(value);
                        return VariationEdit::Pitch;
                    }
                }
            }
        }
        if(op==3){
            int limit=p.bars*16-candidate;
            for(int next=candidate+1;next<p.bars*16;++next){
                if(p.steps[static_cast<std::size_t>(next)].on){limit=next-candidate;break;}
            }
            const int before=std::min<int>(original.length,limit);
            if(before<limit){p.steps[static_cast<std::size_t>(candidate)].length=static_cast<std::uint8_t>(before+1);return VariationEdit::Gate;}
            if(before>1){p.steps[static_cast<std::size_t>(candidate)].length=static_cast<std::uint8_t>(before-1);return VariationEdit::Gate;}
        }
        if(op==4){
            for(int offset:{1,-1}){
                const int next=candidate+offset;
                if(next/16==bar && next>=0 && next<p.bars*16){
                    const auto& target=p.steps[static_cast<std::size_t>(next)];
                    if(!target.on&&!target.locked){
                        p.steps[static_cast<std::size_t>(next)]=original;
                        p.steps[static_cast<std::size_t>(candidate)].on=false;
                        return VariationEdit::Timing;
                    }
                }
            }
        }
        return VariationEdit::None;
    };
    const int chosen=(turn-1)%4+1;
    if(const auto result=tryEdit(chosen);result!=VariationEdit::None)return result;
    return tryEdit(1);
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
