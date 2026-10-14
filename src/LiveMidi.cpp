#include "ebm/LiveMidi.hpp"

namespace ebm::live {
bool prepare(const Phrase& phrase,Pattern& dest) noexcept {
    if (phrase.bars!=4 && phrase.bars!=8) return false;
    const int length=phrase.bars*16;
    Pattern candidate;
    candidate.bars=phrase.bars;candidate.lengthSteps=length;
    int lastEnd=0;
    for (const Note& note:phrase.notes) {
        if (note.step<lastEnd || note.step<0 || note.step>=length ||
            note.duration<1 || note.duration>length-note.step ||
            note.pitch<0 || note.pitch>127 ||
            note.velocity<1 || note.velocity>127) return false;
        lastEnd=note.step+note.duration;
        auto& start=candidate.cells[static_cast<std::size_t>(note.step)];
        auto& end=candidate.cells[static_cast<std::size_t>((note.step+note.duration)%length)];
        if (start.onPitch!=-1 || end.offPitch!=-1)return false;
        start.onPitch=note.pitch;start.velocity=note.velocity;
        end.offPitch=note.pitch;
    }
    candidate.ready=true;
    dest=candidate;
    return true;
}
} // namespace ebm::live
