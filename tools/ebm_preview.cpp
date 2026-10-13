#include "ebm/Composer.hpp"
#include "ebm/MidiWriter.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    if(argc!=2) {std::cerr << "Usage: ebm_preview OUTPUT_DIRECTORY\n";return 2;}
    try {
        std::filesystem::create_directories(argv[1]);
        for(int i=0;i<3;++i) {
            ebm::Config cfg;
            cfg.style=static_cast<ebm::Style>(i);
            cfg.seed=static_cast<std::uint64_t>(0x125a + i*173);
            cfg.variation=70;
            const auto composition=ebm::v2::compose(cfg);
            const auto midi=ebm::toMidi(ebm::v2::render(composition));
            auto filename=std::filesystem::path(argv[1])/(std::string(ebm::styleName(cfg.style))+"_v02.mid");
            std::ofstream output(filename,std::ios::binary);
            if(!output.write(reinterpret_cast<const char*>(midi.data()),static_cast<std::streamsize>(midi.size())))
                throw std::runtime_error("writing preview MIDI failed");
            std::cout << filename.string() << " bars=" << composition.bars.size()
                      << " edits=" << composition.edits.size() << '\n';
        }
        return 0;
    } catch(const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
