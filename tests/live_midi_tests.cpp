#include "ebm/LiveMidi.hpp"
#include "ebm/Composer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <climits>
#include <vector>

#define CHECK(x) do { if(!(x)){ std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1; } }while(false)
using namespace ebm;
using namespace ebm::live;
struct Heard {int64_t sample;int pitch;int velocity;bool on;};
struct Sink {
    std::array<Heard,1024> data{};
    int count=0;
    int64_t origin=0;
    bool failNextOff=false;
    bool operator()(Output e) noexcept {
        if(!e.on && failNextOff){ failNextOff=false;return false; }
        if(count>=static_cast<int>(data.size()))return false;
        data[static_cast<std::size_t>(count++)]={origin+e.offset,e.pitch,e.velocity,e.on};
        return true;
    }
};
static int64_t sampleOf(int64_t sixteenth,double rate) {
    return static_cast<int64_t>(std::ceil(sixteenth/rate-1.e-7));
}
static bool compareCycle(const Phrase& phrase,double sampleRate,int blockSamples) {
    Pattern p;
    if(!prepare(phrase,p))return false;
    Runtime rt;
    Sink out;
    const double tempo=120.0;
    const double rate=tempo*4./(60.*sampleRate);
    const auto total=sampleOf(p.lengthSteps*2+2,rate);
    for(int64_t origin=0;origin<total;origin+=blockSamples){
        Block b;
        b.samples=static_cast<int>(std::min<int64_t>(blockSamples,total-origin));
        b.sampleRate=sampleRate;b.tempo=tempo;
        b.projectPpq=origin*tempo/(60.*sampleRate);
        b.playing=true;b.ppqValid=true;b.tempoValid=true;b.revision=8;
        out.origin=origin;
        if(!rt.process(p,b,nullptr,0,out))return false;
    }
    std::vector<Heard> expected;
    for(int cycle=0;cycle<3;++cycle){
        for(const Note& n:phrase.notes){
            auto s=sampleOf(cycle*p.lengthSteps+n.step,rate);
            auto e=sampleOf(cycle*p.lengthSteps+n.step+n.duration,rate);
            if(s<total) expected.push_back({s,n.pitch,n.velocity,true});
            if(e<total) expected.push_back({e,n.pitch,0,false});
        }
    }
    std::stable_sort(expected.begin(),expected.end(),[](const Heard& a,const Heard& b){
        if(a.sample!=b.sample)return a.sample<b.sample;
        return !a.on && b.on; // NoteOff precedes NoteOn at the same boundary
    });
    if(out.count!=static_cast<int>(expected.size())){
        std::fprintf(stderr,"Count at rate %.0f block %d: got %d expected %zu\n",sampleRate,blockSamples,out.count,expected.size());
        for(int ii=0,jj=0;ii<out.count&&jj<(int)expected.size();){ auto a=out.data[(size_t)ii];auto b=expected[(size_t)jj]; if(a.sample!=b.sample||a.pitch!=b.pitch||a.on!=b.on){ std::fprintf(stderr,"  diff actual %d at %lld pitch%d %d | expect %d at %lld pitch%d %d\n",ii,(long long)a.sample,a.pitch,a.on,jj,(long long)b.sample,b.pitch,b.on); if(a.sample<=b.sample)++ii; else ++jj;}else{++ii;++jj;} }
        return false;
    }
    for(int i=0;i<out.count;++i){
        const auto& a=out.data[static_cast<std::size_t>(i)];
        const auto& b=expected[static_cast<std::size_t>(i)];
        if(a.sample!=b.sample || a.pitch!=b.pitch || a.velocity!=b.velocity || a.on!=b.on){
            std::fprintf(stderr,"Mismatched @%d sr=%.0f bs=%d got (%lld,%d,%d,%d) expected (%lld,%d,%d,%d)\n",i,sampleRate,blockSamples,static_cast<long long>(a.sample),a.pitch,a.velocity,a.on,static_cast<long long>(b.sample),b.pitch,b.velocity,b.on);
            return false;
        }
    }
    // Transport stop releases once and remains silent through further stopped blocks.
    Block stop;stop.samples=257;stop.sampleRate=sampleRate;stop.tempo=tempo;
    stop.playing=false;stop.ppqValid=false;stop.tempoValid=false;stop.revision=8;
    out.origin=total;
    if(!rt.process(p,stop,nullptr,0,out))return false;
    int held=-1;
    for(const auto& e:expected){if(e.on)held=e.pitch;else if(e.pitch==held)held=-1;}
    if(out.count!=static_cast<int>(expected.size())+(held>=0?1:0))return false;
    if(held>=0){const auto& e=out.data[static_cast<std::size_t>(out.count-1)];
        if(e.on||e.pitch!=held||e.sample!=total)return false;}
    return true;
}
int main(){
    Config cfg;cfg.seed=20261014;cfg.variation=85;cfg.bars=8;
    int matrices=0;
    for(int style=0;style<3;++style){
        cfg.style=static_cast<Style>(style);
        const Phrase phrase=v2::render(v2::compose(cfg));
        for(double sr:{44100.,48000.,96000.})
            for(int bs:{1,31,127,257,512}) {
                CHECK(compareCycle(phrase,sr,bs)); ++matrices;
            }
    }
    // Invalid or overlapping source MUST NOT corrupt prepared pattern.
    Pattern p;
    Phrase fixed{4,{{0,36,100,2},{4,38,95,1},{63,40,90,1}}};
    CHECK(prepare(fixed,p));const auto original=p;
    Phrase bad=fixed;bad.notes[1].step=1;
    CHECK(!prepare(bad,p));bad=fixed;bad.notes[0].duration=INT_MAX;CHECK(!prepare(bad,p));for(std::size_t i=0;i<p.cells.size();++i){CHECK(p.cells[i].onPitch==original.cells[i].onPitch && p.cells[i].offPitch==original.cells[i].offPitch && p.cells[i].velocity==original.cells[i].velocity);}
    // Custom trigger: start at sample 7, release on matching NoteOff, never on wrong pitch.
    Runtime midi;
    midi.setMode(Mode::MidiNote);
    Sink out;out.origin=0;
    Block b;b.samples=160;b.sampleRate=160.;b.tempo=120.;b.revision=1;b.songRoot=65;
    const Trigger events[]={{7,65,true},{55,64,false},{108,65,false}};
    CHECK(midi.process(p,b,events,3,out));
    CHECK(out.count>=3);
    CHECK(out.data[0].on && out.data[0].sample==7 && out.data[0].pitch==36);
    CHECK(!midi.triggerHeld() && midi.activePitch()==-1);
    for(int i=0;i<out.count;++i)CHECK(out.data[static_cast<std::size_t>(i)].sample<=108);
    // Transport host seek must flush held note (without a hanging note).
    Runtime transport;
    Sink sink;
    Block seek;seek.samples=100;seek.sampleRate=1000.;seek.tempo=120.;
    seek.projectPpq=0.;seek.playing=true;seek.ppqValid=true;seek.tempoValid=true;seek.revision=2;
    CHECK(transport.process(p,seek,nullptr,0,sink));CHECK(transport.activePitch()==36);
    seek.projectPpq=10.;sink.origin=100;
    CHECK(transport.process(p,seek,nullptr,0,sink));
    CHECK(!sink.data[1].on && sink.data[1].sample==100);
    // Held trigger survives an edit without truncating the already ringing note.
    Runtime live;
    live.setMode(Mode::MidiNote);
    Sink edits;
    Block liveBlock;liveBlock.samples=30;liveBlock.sampleRate=1000.;liveBlock.tempo=120.;liveBlock.revision=10;liveBlock.songRoot=60;
    const Trigger start[]={{0,60,true}};
    CHECK(live.process(p,liveBlock,start,1,edits));
    CHECK(live.triggerHeld() && live.activePitch()==36);
    const int before=edits.count;
    liveBlock.revision=11;edits.origin=30;
    CHECK(live.process(p,liveBlock,nullptr,0,edits));
    CHECK(live.triggerHeld() && edits.count==before);
    CHECK(live.activePitch()==36); // edit does not cut the held note
    // Simulated output bus rejection: retry NoteOff and only then move onward.
    Runtime backpressure;
    Sink reject;
    Block f;f.samples=20;f.sampleRate=1000.;f.tempo=120.;f.projectPpq=0.;
    f.playing=true;f.ppqValid=true;f.tempoValid=true;f.revision=1;
    CHECK(backpressure.process(p,f,nullptr,0,reject));
    CHECK(backpressure.activePitch()==36);
    reject.failNextOff=true;reject.origin=20;
    f.playing=false;f.ppqValid=false;f.tempoValid=false;
    CHECK(!backpressure.process(p,f,nullptr,0,reject));CHECK(backpressure.activePitch()==36);
    CHECK(backpressure.process(p,f,nullptr,0,reject));CHECK(backpressure.activePitch()==-1);
    CHECK(reject.count==2 && !reject.data[1].on);
    // QA-01: over-limit input must release an active note, with NoteOff
    // backpressure retried on the following callback; no duplicate release.
    Runtime overflow;
    Sink overflowSink;
    Block ob;ob.samples=20;ob.sampleRate=1000.;ob.tempo=120.;
    ob.projectPpq=0.;ob.playing=true;ob.ppqValid=true;ob.tempoValid=true;ob.revision=1;
    CHECK(overflow.process(p,ob,nullptr,0,overflowSink));
    CHECK(overflow.activePitch()==36);
    overflowSink.failNextOff=true;
    ob.playing=false;ob.ppqValid=false;ob.tempoValid=false;
    CHECK(!overflow.process(p,ob,nullptr,Runtime::maxTriggers+1,overflowSink));
    CHECK(overflow.activePitch()==36);
    CHECK(!overflow.process(p,ob,nullptr,Runtime::maxTriggers+1,overflowSink));
    CHECK(overflow.activePitch()==-1);
    const int releasedCount=overflowSink.count;
    CHECK(releasedCount==2 && !overflowSink.data[1].on);
    CHECK(!overflow.process(p,ob,nullptr,Runtime::maxTriggers+1,overflowSink));
    CHECK(overflowSink.count==releasedCount);
    std::printf("PASS: %d timing matrices + trigger matching, seek, live edit, NoteOff backpressure, invalid source\n",matrices);
}
