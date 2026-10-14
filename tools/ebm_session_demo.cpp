#include "ebm/Session.hpp"
#include "ebm/MidiWriter.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    if(argc!=2){std::cerr<<"Usage: ebm_session_demo OUTPUT_DIRECTORY\n";return 2;}
    try {
        const std::filesystem::path out(argv[1]);
        std::filesystem::create_directories(out);
        for(int s=0;s<3;++s) {
            ebm::Config cfg;
            cfg.style=static_cast<ebm::Style>(s);
            cfg.seed=static_cast<std::uint64_t>(20261014+s*19);
            cfg.variation=0;
            cfg.bars=8;
            cfg.rootMidi=(s==2?33:36);
            ebm::v2::Session session(cfg);
            const auto selected=session.generate();
            const auto base=ebm::v2::render(session.current());
            const bool developed=session.variate(85);
            if(!developed)throw std::runtime_error("demo variation must change events");
            const auto midi=ebm::toMidi(ebm::v2::render(session.current()));
            const std::filesystem::path file=out/(std::string(ebm::styleName(cfg.style))+"_SESSION.mid");
            std::ofstream stream(file,std::ios::binary);
            if(!stream)throw std::runtime_error("could not create MIDI file");
            stream.write(reinterpret_cast<const char*>(midi.data()),static_cast<std::streamsize>(midi.size()));
            if(!stream)throw std::runtime_error("MIDI write failed");
            std::cout<<file.string()<<" seed="<<selected.selectedSeed
                     <<" sourceNotes="<<base.notes.size()
                     <<" variationNotes="<<ebm::v2::render(session.current()).notes.size()
                     <<" noveltyTarget="<<(selected.meetsNoveltyTarget?"yes":"no")<<"\n";
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
