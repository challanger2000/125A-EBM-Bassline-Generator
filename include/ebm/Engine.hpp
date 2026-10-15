#pragma once
#include <cstdint>
#include <vector>
namespace ebm {
enum class Style { MinimalBody, ClassicEBM, DarkElectro };
struct Config {
    Style style=Style::ClassicEBM;
    int bars=8;
    int rootMidi=48; // New patterns: Studio One C2 (MIDI note 48).
    std::uint64_t seed=1;
    int variation=30;
};
struct Note {
    int step=0;
    int pitch=48;
    int velocity=100;
    int duration=1;
    bool operator==(const Note& o) const {
        return step==o.step && pitch==o.pitch && velocity==o.velocity && duration==o.duration;
    }
};
struct Phrase {int bars=0;std::vector<Note> notes;};
Phrase generate(const Config&);
const char* styleName(Style);
} // namespace ebm
