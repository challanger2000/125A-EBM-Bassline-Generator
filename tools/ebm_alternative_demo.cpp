#include "ebm/Session.hpp"
#include "ebm/MidiWriter.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

void save(const std::filesystem::path& path,const ebm::Phrase& notes) {
    const auto midi=ebm::toMidi(notes);
    std::ofstream o(path,std::ios::binary);
    if(!o)throw std::runtime_error("unable to open MIDI output");
    o.write(reinterpret_cast<const char*>(midi.data()),static_cast<std::streamsize>(midi.size()));
    if(!o)throw std::runtime_error("MIDI output write failed");
}
int main(int argc,char** argv) {
    if(argc!=2){std::cerr<<"Usage: ebm_alternative_demo OUTPUT_DIR\n";return 2;}
    try {
        const std::filesystem::path dir=argv[1];
        std::filesystem::create_directories(dir);
        for(int i=0;i<3;++i) {
            ebm::Config cfg;cfg.style=static_cast<ebm::Style>(i);
            cfg.bars=8;cfg.rootMidi=i==2?33:36;cfg.variation=55;
            cfg.seed=static_cast<std::uint64_t>(20261014+137*i);
            ebm::v2::Session session(cfg);
            const auto info=session.generate();
            const std::string name=ebm::styleName(cfg.style);
            save(dir/(name+"_ORIGINAL.mid"),ebm::v2::render(session.current()));
            if(!session.nextVariant()||session.alternative()!=1)
                throw std::runtime_error("first alternate was not selected");
            save(dir/(name+"_VARIATE.mid"),ebm::v2::render(session.current()));
            std::cout<<name<<" selectedSeed="<<info.selectedSeed
                <<" variation="<<cfg.variation<<" alternative="<<session.alternative()<<"\n";
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
