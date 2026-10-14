#include "ebm/Composer.hpp"
#include "ebm/MidiWriter.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    if(argc!=2) {std::cerr << "Usage: ebm_boundary_demo OUTPUT_DIRECTORY\n";return 2;}
    try {
        const std::filesystem::path dir(argv[1]);
        std::filesystem::create_directories(dir);
        for(int style=0;style<3;++style) {
            ebm::Config cfg;
            cfg.style=static_cast<ebm::Style>(style);
            cfg.seed=10000;
            cfg.bars=8;
            cfg.variation=85;
            cfg.rootMidi=style==2?33:36;
            const auto c=ebm::v2::compose(cfg);
            bool hasLoopLead=false;
            for(const auto& e:c.bars.back().notes)
                if(e.role==ebm::v2::Role::Tension && e.targetBarDelta==1)
                    hasLoopLead=true;
            if(!hasLoopLead)throw std::logic_error("demo must exhibit a cross-loop lead");
            for(const auto ending:{ebm::v2::RenderEnding::Loopable,
                                   ebm::v2::RenderEnding::OneShot}) {
                const auto phrase=ebm::v2::render(c,ending);
                const auto bytes=ebm::toMidi(phrase);
                const std::string suffix=ending==ebm::v2::RenderEnding::Loopable?"LOOP":"ONE_SHOT";
                const auto file=dir/(std::string(ebm::styleName(cfg.style))+"_"+suffix+".mid");
                std::ofstream f(file,std::ios::binary);
                if(!f.write(reinterpret_cast<const char*>(bytes.data()),
                            static_cast<std::streamsize>(bytes.size())))
                    throw std::runtime_error("MIDI write failed");
                std::cout<<file.string()<<" notes="<<phrase.notes.size()<<"\n";
            }
        }
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
