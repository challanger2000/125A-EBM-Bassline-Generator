#include "ebm/MidiWriter.hpp"
#include <algorithm>
#include <stdexcept>

namespace ebm {
namespace {
void be16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}
void be32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}
void vlq(std::vector<std::uint8_t>& out, std::uint32_t v) {
    std::uint8_t b[5]; int n = 0;
    b[n++] = static_cast<std::uint8_t>(v & 0x7f);
    while ((v >>= 7) != 0) b[n++] = static_cast<std::uint8_t>((v & 0x7f) | 0x80);
    while (n) out.push_back(b[--n]);
}
struct Event { int tick, priority, pitch, velocity; };
} // namespace

std::vector<std::uint8_t> toMidi(const Phrase& phrase) {
    if (phrase.bars <= 0 || phrase.bars > 1024) throw std::invalid_argument("invalid bars");
    std::vector<Event> events;
    events.reserve(phrase.notes.size() * 2);
    int priorEnd = 0;
    for (const auto& note : phrase.notes) {
        if (note.step < 0 || note.duration <= 0 || note.step + note.duration > phrase.bars * 16 ||
            note.pitch < 0 || note.pitch > 127 || note.velocity < 1 || note.velocity > 127)
            throw std::invalid_argument("invalid MIDI note");
        if (note.step < priorEnd) throw std::invalid_argument("notes overlap or are unsorted");
        priorEnd = note.step + note.duration;
        events.push_back({note.step * 120, 1, note.pitch, note.velocity});
        events.push_back({priorEnd * 120, 0, note.pitch, 0});
    }
    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.priority < b.priority; // NoteOff before NoteOn at same tick.
    });
    std::vector<std::uint8_t> track;
    // Explicit 120 BPM tempo (500000 microseconds per beat), 4/4 meter.
    track.insert(track.end(), {0,0xff,0x51,3,0x07,0xa1,0x20,0,0xff,0x58,4,4,2,24,8});
    int lastTick = 0;
    for (const auto& e : events) {
        vlq(track, static_cast<std::uint32_t>(e.tick - lastTick));
        track.push_back(static_cast<std::uint8_t>(e.priority ? 0x90 : 0x80));
        track.push_back(static_cast<std::uint8_t>(e.pitch));
        track.push_back(static_cast<std::uint8_t>(e.velocity));
        lastTick = e.tick;
    }
    vlq(track, static_cast<std::uint32_t>(phrase.bars * 1920 - lastTick));
    track.insert(track.end(), {0xff,0x2f,0});
    std::vector<std::uint8_t> midi;
    midi.insert(midi.end(), {'M','T','h','d'});
    be32(midi, 6); be16(midi, 0); be16(midi, 1); be16(midi, 480);
    midi.insert(midi.end(), {'M','T','r','k'});
    be32(midi, static_cast<std::uint32_t>(track.size()));
    midi.insert(midi.end(), track.begin(), track.end());
    return midi;
}
} // namespace ebm
