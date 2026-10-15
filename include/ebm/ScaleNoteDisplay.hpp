#pragma once
#include "ebm/EditPattern.hpp"
#include <array>

namespace ebm::edit {
// Read-only GUI pitch-class guide. Uses the SAME membership predicate as the
// harmonic MIDI trigger; it cannot drift away from the actual playable scale.
struct TriggerScaleNotes {
    std::array<const char*,12> names{};
    int count=0;
};

inline TriggerScaleNotes triggerScaleNotes(int rootMidi,Scale scale) noexcept {
    TriggerScaleNotes result{};
    if(rootMidi<0 || rootMidi>127 || static_cast<int>(scale)>=kScaleCount)return result;
    // Prefer readable flat spellings for flat-key modes (e.g. C natural minor).
    // The host uses MIDI pitch classes, so enharmonic spellings are equivalent.
    int relativeMajor=rootMidi%12;
    switch(scale){
    case Scale::NaturalMinor:case Scale::HarmonicMinor:
    case Scale::MelodicMinor:case Scale::MinorPentatonic:
        relativeMajor=(relativeMajor+3)%12;break;
    case Scale::Dorian:relativeMajor=(relativeMajor+10)%12;break;
    case Scale::Phrygian:relativeMajor=(relativeMajor+8)%12;break;
    case Scale::Lydian:relativeMajor=(relativeMajor+7)%12;break;
    case Scale::Mixolydian:relativeMajor=(relativeMajor+5)%12;break;
    default:break;
    }
    const bool flats=scale!=Scale::Chromatic &&
        (relativeMajor==1 || relativeMajor==3 || relativeMajor==5 ||
         relativeMajor==8 || relativeMajor==10);
    constexpr const char* sharpNames[12]={
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    constexpr const char* flatNames[12]={
        "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    for(int semitone=0;semitone<12;++semitone){
        const int pc=(rootMidi%12+semitone)%12;
        // Chromatic MIDI notation intentionally does not depend on octave.
        if(!inScale(60+pc,rootMidi,scale))continue;
        result.names[static_cast<std::size_t>(result.count++)]=
            flats?flatNames[pc]:sharpNames[pc];
    }
    return result;
}
}
