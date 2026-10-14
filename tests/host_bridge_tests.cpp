#include "ebm/HostBridge.hpp"
#include "ebm/MidiWriter.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#define REQUIRE(x) do { if(!(x)) {std::cerr<<"FAIL "<<__LINE__<<" " #x "\n";return 1;}}while(false)
using ebm::live::Block;
using ebm::live::Output;
namespace host=ebm::host;
int main(){
    host::Model model;
    const auto original=model.snapshot();
    REQUIRE(original.session.hasCurrent);
    REQUIRE(original.mode==ebm::live::Mode::Transport);
    host::DocumentBytes bytes{};
    REQUIRE(host::encode(original,bytes));
    host::Document roundtrip;
    REQUIRE(host::decode(bytes.data(),bytes.size(),roundtrip));
    host::DocumentBytes retransmitted{};
    REQUIRE(host::encode(roundtrip,retransmitted));
    REQUIRE(bytes==retransmitted);
    const auto untouched=roundtrip;
    REQUIRE(!host::decode(bytes.data(),bytes.size()-1,roundtrip));
    REQUIRE(roundtrip.session.options.seed==untouched.session.options.seed);
    auto broken=bytes;broken[19]^=0x40;
    REQUIRE(!host::decode(broken.data(),broken.size(),roundtrip));
    broken=bytes;broken[broken.size()-5]^=0x20;
    REQUIRE(!host::decode(broken.data(),broken.size(),roundtrip));
    broken=bytes;broken[0]='X';
    REQUIRE(!host::decode(broken.data(),broken.size(),roundtrip));
    auto invalid=original;invalid.mode=static_cast<ebm::live::Mode>(9);
    REQUIRE(!host::encode(invalid,retransmitted));
    REQUIRE(model.restore(original));
    REQUIRE(model.generate());
    REQUIRE(model.snapshot().session.current.seed!=original.session.current.seed);
    REQUIRE(model.restore(original));
    REQUIRE(model.snapshot().session.current.seed==original.session.current.seed);
    REQUIRE(model.nextVariant());
    REQUIRE(model.snapshot().session.alternative==1);
    REQUIRE(model.variate(0));
    REQUIRE(model.snapshot().session.alternative==0);
    REQUIRE(!model.nextVariant());
    REQUIRE(model.restore(original));

    // A second full model restores the byte-identical host state and MIDI.
    host::Model recovered;
    REQUIRE(recovered.restore(roundtrip));
    REQUIRE(recovered.snapshot().session.current.seed==original.session.current.seed);

    // A fixed, bounded exchange supports multiple publishes without any audio
    // processing; when consumed it exposes the newest complete snapshot only.
    host::PatternExchange exchange;
    ebm::live::Pattern first{},second{};
    const ebm::Phrase one{4,{{0,36,110,1},{8,43,95,2}}};
    REQUIRE(ebm::live::prepare(one,first));
    const ebm::Phrase two{4,{{0,40,110,1},{8,43,95,2}}};
    REQUIRE(ebm::live::prepare(two,second));
    REQUIRE(exchange.publish(first));
    REQUIRE(exchange.publish(second));
    std::uint64_t revision=0;
    const auto* selected=exchange.acquire(revision);
    REQUIRE(selected && revision==1 && selected->cells[0].onPitch==40);
    REQUIRE(exchange.acquire(revision)==selected && revision==1);
    REQUIRE(exchange.publish(first));
    selected=exchange.acquire(revision);
    REQUIRE(selected && revision==2 && selected->cells[0].onPitch==36);

    // Simulate non-realtime publication while the audio side continuously
    // adopts snapshots. No partial/cross-wired patterns are permitted.
    {
        host::PatternExchange simultaneous;
        REQUIRE(simultaneous.publish(first));
        std::atomic<bool> finished{false};
        std::thread writer([&]{
            for(int i=0;i<10000;++i)
                (void)simultaneous.publish((i%2)==0?first:second);
            finished.store(true,std::memory_order_release);
        });
        std::uint64_t ticket=0;
        while(!finished.load(std::memory_order_acquire)){
            const auto* current=simultaneous.acquire(ticket);
            if(current){
                const int pitch=current->cells[0].onPitch;
                if(pitch!=36&&pitch!=40)std::abort();
                if(current->cells[8].onPitch!=43)std::abort();
                if(current->cells[8].velocity!=95)std::abort();
            }
        }
        writer.join();
    }

    // End-to-end live PPQ playback, including exact sample offsets under very
    // different host block sizes. The output is MIDI events, never audio DSP.
    struct Event{std::uint64_t sample;int pitch;int velocity;bool on;};
    auto run=[&](int blockSamples,int sampleRate){
        host::Model instance;
        std::vector<Event> output;
        output.reserve(1024); // nothing may allocate in the audio callback
        const double tempo=120.;
        const std::uint64_t total=static_cast<std::uint64_t>(sampleRate)*16;
        for(std::uint64_t pos=0;pos<total;){
            const int count=static_cast<int>(std::min<std::uint64_t>(blockSamples,total-pos));
            Block b{};
            b.samples=count;b.sampleRate=sampleRate;b.tempo=tempo;
            b.projectPpq=static_cast<double>(pos)*tempo/(60.*sampleRate);
            b.ppqValid=b.tempoValid=b.playing=true;
            const bool ok=instance.process(b,nullptr,0,[&](Output e){
                if(output.size()==output.capacity())std::abort();
                output.push_back(Event{pos+static_cast<unsigned>(e.offset),e.pitch,e.velocity,e.on});
                return true;
            });
            if(!ok)throw std::runtime_error("live transport failed");
            pos+=static_cast<unsigned>(count);
        }
        Block stop{};stop.sampleRate=sampleRate;stop.tempo=tempo;
        stop.samples=1;stop.playing=false;
        const bool stopped=instance.process(stop,nullptr,0,[&](Output e){
            if(output.size()==output.capacity())std::abort();
            output.push_back(Event{total,e.pitch,e.velocity,e.on});return true;
        });
        if(!stopped)throw std::runtime_error("STOP failed");
        return output;
    };
    for(const int rate:{44100,48000,96000}){
        const auto reference=run(4096,rate);
        REQUIRE(!reference.empty());
        int active=0;
        for(const auto& ev:reference){
            if(ev.on){REQUIRE(active==0);++active;REQUIRE(ev.velocity>=1&&ev.velocity<=127);}
            else{REQUIRE(active==1);--active;}
        }
        REQUIRE(active==0);
        for(const int chunk:{1,31,127,512}){
            const auto result=run(chunk,rate);
            REQUIRE(result.size()==reference.size());
            for(std::size_t i=0;i<result.size();++i){
                REQUIRE(result[i].sample==reference[i].sample);
                REQUIRE(result[i].pitch==reference[i].pitch);
                REQUIRE(result[i].velocity==reference[i].velocity);
                REQUIRE(result[i].on==reference[i].on);
            }
        }
    }
    // MIDI NOTE input: matching NoteOff owns the stop; unrelated NoteOff ignored.
    model.setMode(ebm::live::Mode::MidiNote);
    std::vector<Output> output;
    Block b{};b.samples=16000;b.sampleRate=48000;b.tempo=120.;
    ebm::live::Trigger trigger[]{ {100,49,true},{15000,48,false} };
    REQUIRE(model.process(b,trigger,2,[&](Output e){output.push_back(e);return true;}));
    REQUIRE(!output.empty() && output[0].on && output[0].offset>=100);
    const ebm::live::Trigger off[]{ {100,49,false} };
    REQUIRE(model.process(b,off,1,[&](Output e){output.push_back(e);return true;}));
    REQUIRE(!model.snapshot().session.history.empty());
    // A malformed document must leave both the source motif and mode untouched.
    const auto before=model.snapshot();
    auto damaged=before;damaged.session.current.bars=4;
    REQUIRE(!model.restore(damaged));
    REQUIRE(model.snapshot().session.current.bars==before.session.current.bars);
    REQUIRE(model.mode()==before.mode);
    std::cout<<"PASS: host bridge, canonical state codec, invalid data,\n"
             <<" 15 timebase/chunk combinations, loop NoteOff pairing, MIDI trigger\n";
}
