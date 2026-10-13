#include "ebm/Engine.hpp"
#include "ebm/MidiWriter.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 2) { std::cerr << "Usage: ebm_batch OUTPUT_DIRECTORY\n"; return 2; }
        const std::filesystem::path folder(argv[1]);
        std::filesystem::create_directories(folder);
        std::ofstream catalog(folder / "catalog.csv", std::ios::binary);
        if (!catalog) throw std::runtime_error("cannot create catalog");
        catalog << "style,seed,bars,root_midi,notes,filename\n";
        // 30 distinct seeds per style; 8-bar raw MIDI; no embedded synth audio.
        for (int s = 0; s != 3; ++s) {
            const auto style = static_cast<ebm::Style>(s);
            for (int i = 0; i != 30; ++i) {
                ebm::Config cfg;
                cfg.style = style;
                cfg.bars = 8;
                cfg.seed = static_cast<std::uint64_t>(10001 + s * 1000 + i);
                cfg.rootMidi = 36;
                const auto phrase = ebm::generate(cfg);
                const auto bytes = ebm::toMidi(phrase);
                std::ostringstream filename;
                filename << ebm::styleName(style) << "_" << std::setw(2)
                         << std::setfill('0') << i + 1 << ".mid";
                std::ofstream f(folder / filename.str(), std::ios::binary);
                if (!f.write(reinterpret_cast<const char*>(bytes.data()),
                             static_cast<std::streamsize>(bytes.size())))
                    throw std::runtime_error("MIDI write failed");
                catalog << ebm::styleName(style) << ',' << cfg.seed << ',' << cfg.bars << ','
                        << cfg.rootMidi << ',' << phrase.notes.size() << ',' << filename.str() << '\n';
            }
        }
        std::cout << "Wrote 90 MIDI preview phrases to " << folder << '\n';
        return 0;
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
