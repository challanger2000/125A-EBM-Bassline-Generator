#pragma once
#include "ebm/EditPattern.hpp"
#include <algorithm>

namespace ebm::harmony {
// The trigger is the diatonic CHORD ROOT inside the fixed song scale.
// Example C Major: C2 -> C E G; E2 -> E G B; A2 -> A C E.
// No runtime allocations, changing MIDI event positions or musical phase.
inline int floorDiv(int a,int b) noexcept {return a>=0?a/b:-((-a+b-1)/b);}
inline int degreeSize(edit::Scale scale) noexcept {
    return scale==edit::Scale::MinorPentatonic?5:scale==edit::Scale::Chromatic?12:7;
}
inline int interval(edit::Scale scale,int i) noexcept {
    static constexpr int tones[10][12]={
      {0,1,2,3,4,5,6,7,8,9,10,11},
      {0,2,4,5,7,9,11}, {0,2,3,5,7,8,10},
      {0,2,3,5,7,8,11}, {0,2,3,5,7,9,11},
      {0,2,3,5,7,9,10}, {0,1,3,5,7,8,10},
      {0,2,4,5,7,9,10}, {0,2,4,6,7,9,11},
      {0,3,5,7,10}
    };
    return tones[std::clamp(static_cast<int>(scale),0,9)][i];
}
inline int degreeOf(int pitch,int tonic,edit::Scale scale) noexcept {
    if(scale==edit::Scale::Chromatic)return pitch-tonic;
    // Out-of-scale triggers resolve to the nearest diatonic note.
    // A tie prefers the lower note (E over C minor -> Eb, not F).
    int fixed=std::clamp(pitch,0,127);
    if(!edit::inScale(fixed,tonic,scale)){
        for(int distance=1;distance<=12;++distance){
            if(fixed-distance>=0 && edit::inScale(fixed-distance,tonic,scale)){
                fixed-=distance;break;
            }
            if(fixed+distance<=127 && edit::inScale(fixed+distance,tonic,scale)){
                fixed+=distance;break;
            }
        }
    }
    const int delta=fixed-tonic;
    const int oct=floorDiv(delta,12),semi=delta-12*oct;
    const int n=degreeSize(scale);
    for(int i=0;i<n;++i)if(interval(scale,i)==semi)return oct*n+i;
    return oct*n; // unreachable for a valid scale; safe fallback
}
inline int pitchAt(int tonic,int degree,edit::Scale scale) noexcept {
    const int n=degreeSize(scale),oct=floorDiv(degree,n);
    return std::clamp(tonic+12*oct+interval(scale,degree-oct*n),0,127);
}
inline int chordPitch(int sourcePitch,int songRoot,int triggerRoot,edit::Scale scale) noexcept {
    if(scale==edit::Scale::Chromatic)return std::clamp(sourcePitch+triggerRoot-songRoot,0,127);
    return pitchAt(songRoot,degreeOf(sourcePitch,songRoot,scale)+degreeOf(triggerRoot,songRoot,scale),scale);
}
} // namespace ebm::harmony
