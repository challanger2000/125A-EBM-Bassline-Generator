#include "ebm/Engine.hpp"
#include "ebm/MidiWriter.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#define REQUIRE(condition) do { if (!(condition)) { \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << " " #condition "\n"; \
    return 1; } } while (false)

int main() {
    std::set<std::vector<unsigned char>> unique;
    for (int s = 0; s < 3; ++s) {
        const auto style = static_cast<ebm::Style>(s);
        for (int n = 0; n < 30; ++n) {
            ebm::Config cfg;
            cfg.style = style; cfg.seed = static_cast<unsigned>(10001 + s * 1000 + n);
            for (int bars : {4, 8}) {
                cfg.bars = bars;
                const auto a = ebm::generate(cfg);
                const auto b = ebm::generate(cfg);
                REQUIRE(a.bars == bars);
                REQUIRE(a.notes == b.notes); // deterministic seed across invocations.
                REQUIRE(!a.notes.empty());
                int end = 0;
                std::set<int> tones;
                for (const auto& e : a.notes) {
                    REQUIRE(e.step >= end);
                    REQUIRE(e.step + e.duration <= bars * 16);
                    REQUIRE(e.duration >= 1);
                    REQUIRE(e.velocity >= 1 && e.velocity <= 127);
                    REQUIRE(e.pitch >= 24 && e.pitch <= 84);
                    tones.insert(e.pitch);
                    end = e.step + e.duration;
                }
                REQUIRE(tones.size() >= 2);
                auto bytes = ebm::toMidi(a);
                REQUIRE(bytes.size() > 40);
                REQUIRE(std::string(bytes.begin(), bytes.begin() + 4) == "MThd");
                REQUIRE(std::string(bytes.begin() + 14, bytes.begin() + 18) == "MTrk");
                if (bars == 8) REQUIRE(unique.insert(bytes).second);
                // Pure transposition without changing rhythmic identities.
                cfg.rootMidi = 43;
                const auto transposed = ebm::generate(cfg);
                REQUIRE(a.notes.size() == transposed.notes.size());
                for (std::size_t i = 0; i < a.notes.size(); ++i) {
                    REQUIRE(transposed.notes[i].pitch - a.notes[i].pitch == 7);
                    REQUIRE(transposed.notes[i].step == a.notes[i].step);
                }
                cfg.rootMidi = 36;
            }
        }
    }
    // Variation must not destroy the source motif, even at maximum intensity.
    for (int s = 0; s < 3; ++s) {
        ebm::Config cfg;
        cfg.style = static_cast<ebm::Style>(s);
        cfg.bars = 8;
        cfg.seed = 45122;
        cfg.variation = 0;
        const auto frozen = ebm::generate(cfg);
        const int motifBars = s == 0 ? 1 : 2;
        for (const auto& n : frozen.notes) {
            if (n.step < motifBars * 16) continue;
            const int originStep = n.step % (motifBars * 16);
            const auto it = std::find_if(frozen.notes.begin(), frozen.notes.end(),
                [originStep](const ebm::Note& x) { return x.step == originStep; });
            REQUIRE(it != frozen.notes.end());
            REQUIRE(n.pitch == it->pitch);
            REQUIRE(n.velocity == it->velocity);
            REQUIRE(n.duration == it->duration);
        }
        cfg.variation = 100;
        const auto developed = ebm::generate(cfg);
        bool laterChanged = false;
        for (std::size_t i = 0; i < frozen.notes.size(); ++i) {
            if (frozen.notes[i].step < motifBars * 16) {
                REQUIRE(frozen.notes[i] == developed.notes[i]);
            } else if (!(frozen.notes[i] == developed.notes[i])) laterChanged = true;
            REQUIRE(frozen.notes[i].step == developed.notes[i].step);
        }
        REQUIRE(laterChanged);
    }
    // Explicitly reject invalid pitch bounds and enum values.
    for (int invalidRoot : {23, 61}) {
        ebm::Config cfg; cfg.rootMidi = invalidRoot;
        bool threw = false;
        try { (void)ebm::generate(cfg); } catch (const std::invalid_argument&) { threw = true; }
        REQUIRE(threw);
    }
    {
        ebm::Config cfg; cfg.style = static_cast<ebm::Style>(99);
        bool threw = false;
        try { (void)ebm::generate(cfg); } catch (const std::invalid_argument&) { threw = true; }
        REQUIRE(threw);
    }
    {
        ebm::Phrase phrase{4, {{0,36,100,3}, {2,36,100,1}}};
        bool threw = false;
        try { (void)ebm::toMidi(phrase); } catch (const std::invalid_argument&) { threw = true; }
        REQUIRE(threw);
    }
    REQUIRE(unique.size() == 90);
    ebm::Config bad;
    for (int invalid : {0, 1, 3, 16}) {
        bad.bars = invalid;
        bool threw = false;
        try { (void)ebm::generate(bad); } catch (const std::invalid_argument&) { threw = true; }
        REQUIRE(threw);
    }
    bad.bars = 8;
    for (int invalid : {-1, 101}) {
        bad.variation = invalid;
        bool threw = false;
        try { (void)ebm::generate(bad); } catch (const std::invalid_argument&) { threw = true; }
        REQUIRE(threw);
    }
    std::cout << "PASS: deterministic 3 styles x 30 seeds x 2 lengths; 90 unique 8-bar files, note bounds, monophony, transposition and MIDI header\n";
    return 0;
}
