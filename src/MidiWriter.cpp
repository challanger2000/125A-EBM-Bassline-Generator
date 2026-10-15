#include "ebm/MidiWriter.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
namespace ebm {
namespace {
void be16(std::vector<std::uint8_t>& out,std::uint16_t v){out.push_back(std::uint8_t(v>>8));out.push_back(std::uint8_t(v));}
void be32(std::vector<std::uint8_t>& out,std::uint32_t v){out.push_back(std::uint8_t(v>>24));out.push_back(std::uint8_t(v>>16));out.push_back(std::uint8_t(v>>8));out.push_back(std::uint8_t(v));}
void vlq(std::vector<std::uint8_t>& out,std::uint32_t v){
    std::uint8_t b[5];int n=0;b[n++]=std::uint8_t(v&127u);
    while((v>>=7u)!=0)b[n++]=std::uint8_t((v&127u)|128u);
    while(n>0)out.push_back(b[--n]);
}
struct Event {int tick,priority,pitch,velocity;};
}
std::vector<std::uint8_t> toMidi(const Phrase& phrase,int gatePercent){
    if(gatePercent<0||gatePercent>100)throw std::invalid_argument("invalid gate");
    if(phrase.bars<=0||phrase.bars>1024)throw std::invalid_argument("invalid bars");
    std::vector<Event> events;events.reserve(phrase.notes.size()*2);
    int priorEnd=0;
    for(const auto& n:phrase.notes){
        if(n.step<0||n.duration<=0||n.step+n.duration>phrase.bars*16||n.pitch<0||n.pitch>127||n.velocity<1||n.velocity>127)
            throw std::invalid_argument("invalid MIDI event");
        if(n.step<priorEnd)throw std::invalid_argument("unsorted/overlapping notes");
        priorEnd=n.step+n.duration;
        if(gatePercent>0){
            const int begin=n.step*120;
            const int gateTicks=std::max(1,(n.duration*120*gatePercent+50)/100);
            events.push_back({begin,1,n.pitch,n.velocity});
            events.push_back({begin+gateTicks,0,n.pitch,0});
        }
    }
    std::sort(events.begin(),events.end(),[](const Event& a,const Event& b){
        if(a.tick!=b.tick) return a.tick<b.tick;
        return a.priority<b.priority;
    });
    std::vector<std::uint8_t> track;
    track.insert(track.end(),{0,0xff,0x51,3,0x07,0xa1,0x20,0,0xff,0x58,4,4,2,24,8});
    int lastTick=0;
    for(const auto& e:events){
        vlq(track,std::uint32_t(e.tick-lastTick));
        track.push_back(std::uint8_t(e.priority?0x90:0x80));
        track.push_back(std::uint8_t(e.pitch));track.push_back(std::uint8_t(e.velocity));
        lastTick=e.tick;
    }
    vlq(track,std::uint32_t(phrase.bars*1920-lastTick));
    track.insert(track.end(),{0xff,0x2f,0});
    std::vector<std::uint8_t> midi;
    midi.insert(midi.end(),{'M','T','h','d'});
    be32(midi,6);be16(midi,0);be16(midi,1);be16(midi,480);
    midi.insert(midi.end(),{'M','T','r','k'});be32(midi,std::uint32_t(track.size()));
    midi.insert(midi.end(),track.begin(),track.end());return midi;
}
} // namespace ebm
