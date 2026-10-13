#pragma once
#include <cstdint>
#include <vector>

namespace ebm {
enum class Style { MinimalBody, ClassicEBM, DarkElectro };
struct Config {
    Style style = Style::ClassicEBM;
    int bars = 8;                 // Supported: 4 or 8.
    int rootMidi = 36;            // C2, valid 24..60.
    std::uint64_t seed = 1;       // Never use an implicit random seed.
    int variation = 30;          // 0..100, intensity of phrase development.
};
struct Note {
    int step = 0;               // Global 1/16-note index from phrase start.
    int pitch = 36;             // MIDI key.
    int velocity = 100;         // 1..127.
    int duration = 1;           // 1/16-note units, positive.
    bool operator==(const Note& other) const {
        return step == other.step && pitch == other.pitch &&
               velocity == other.velocity && duration == other.duration;
    }
};
struct Phrase { int bars = 0; std::vector<Note> notes; };
Phrase generate(const Config& config);
const char* styleName(Style style);
} // namespace ebm
