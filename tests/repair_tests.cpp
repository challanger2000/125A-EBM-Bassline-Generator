#include "ebm/Automation.hpp"
#include "midi_phase_tests.hpp"
#include "ebm/HostBridge.hpp"
#include "ebm/MidiWriter.hpp"
#include "../src/vst3/UiGeometry.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include <numeric>
#include <thread>
#include <stdexcept>
#include <vector>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"REPAIR FAIL %d: %s\n",__LINE__,#x);return 1;}}while(false)
namespace {thread_local bool audio=false;thread_local unsigned allocations=0;}
void* operator new(std::size_t size){if(audio)++allocations;if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace ebm;
static unsigned vlq(const std::vector<std::uint8_t>& b,std::size_t& at){unsigned v=0;for(int n=0;n<4;++n){auto x=b.at(at++);v=(v<<7)|(x&127u);if(!(x&128u))return v;}throw std::runtime_error("VLQ");}
int main(int argc,char** argv){
 int value=77;
 if(argc==2&&std::strcmp(argv[1],"--negative-control")==0){
  CHECK(vst3::ui::integer("12x",0,127,value)); // Must fail even with NDEBUG.
  return 0;
 }
 CHECK(qa::midiPhaseTests()==0);
 for(auto bad:{"","12x","1.5"," 12","+12","-1","999999999999999999999"}){CHECK(!vst3::ui::integer(bad,0,127,value));CHECK(value==77);}
 CHECK(vst3::ui::integer("0",0,100,value)&&value==0);CHECK(vst3::ui::integer("127",1,127,value)&&value==127);
 edit::Pattern p;p.enabled=true;p.bars=1;
 CHECK(edit::modify(p,0,24,100,16));CHECK(edit::modify(p,2,60,80,1));
 CHECK(edit::effectiveLength(p,0)==2&&edit::maximumLength(p,0)==2&&p.steps[0].length==16);
 const auto original=p;
 CHECK(!edit::transpose(p,-36));CHECK(p.steps[0].pitch==24&&p.steps[2].pitch==60);
 CHECK(edit::transpose(p,12));CHECK(edit::transpose(p,-12));CHECK(edit::toPhrase(p).notes==edit::toPhrase(original).notes);
 p.steps[0].locked=true;p.steps[1].locked=true;auto fresh=edit::Pattern{};fresh.enabled=true;
 CHECK(edit::generateClassicDrive(fresh,48,17));edit::preserveLocks(p,fresh);
 CHECK(fresh.steps[0].locked&&fresh.steps[0].pitch==24);CHECK(fresh.steps[1].locked&&!fresh.steps[1].on);
 for(int bars:{1,2,4,8})for(int gate:{0,50,100}){
  edit::Pattern notes;notes.enabled=true;notes.bars=bars;
  CHECK(edit::modify(notes,0,48,100,4));CHECK(edit::modify(notes,bars*16-1,60,127,1));
  const auto bytes=toMidi(edit::toPhrase(notes),gate);std::size_t at=22;unsigned tick=0;int held=-1,on=0,off=0;bool ended=false;
  while(at<bytes.size()){
   tick+=vlq(bytes,at);auto status=bytes.at(at++);
   if(status==255){auto type=bytes.at(at++);auto size=vlq(bytes,at);at+=size;if(type==47){CHECK(at==bytes.size()&&size==0);ended=true;}}
   else {int pitch=bytes.at(at++),velocity=bytes.at(at++);CHECK(pitch<=127&&velocity<=127);
    if(status==144){CHECK(held==-1);held=pitch;++on;if(on==1)CHECK(tick==0);}
    else {CHECK(status==128&&held==pitch);held=-1;++off;if(off==1)CHECK(tick==static_cast<unsigned>(480*gate/100));}}
  }
  CHECK(ended&&held==-1&&tick==static_cast<unsigned>(bars*1920));CHECK(on==off&&on==(gate?2:0));
 }
 // Invalid individual input and a rejected NoteOff must leave no held trigger.
 for(int invalid=0;invalid<3;++invalid){
  live::Pattern prepared;CHECK(live::prepare(Phrase{1,{{0,48,100,4}}},prepared));
  live::Runtime rt;rt.setMode(live::Mode::MidiNote);
  live::Block block;block.samples=64;block.sampleRate=48000.;int on=0,off=0;bool reject=false;
  auto sink=[&](live::Output e)noexcept{if(!e.on&&reject){reject=false;return false;}if(e.on)++on;else ++off;return true;};
  live::Trigger go{0,48,true};CHECK(rt.process(prepared,block,&go,1,sink));reject=true;
  live::Trigger bad{invalid==0?-1:0,invalid==1?128:48,invalid!=2};
  CHECK(!rt.process(prepared,block,&bad,1,sink));CHECK(!rt.triggerHeld());
  CHECK(rt.process(prepared,block,nullptr,0,sink));CHECK(rt.activePitch()==-1&&on==off);
 }
 // Simultaneous old NoteOff, unrelated NoteOff and new root must preserve phase.
 {
  live::Pattern prepared;CHECK(live::prepare(Phrase{1,{{0,48,100,4},{4,50,100,1}}},prepared));
  live::Runtime rt;rt.setMode(live::Mode::MidiNote);live::Block block;block.samples=100;block.sampleRate=1000.;
  const live::Trigger go{0,48,true};bool nextOnTime=false;int balance=0;
  auto sink=[&](live::Output e)noexcept{balance+=e.on?1:-1;if(e.on&&e.pitch==54&&e.offset==400)nextOnTime=true;return true;};
  CHECK(rt.process(prepared,block,&go,1,sink));block.samples=500;
  const live::Trigger chord[]{{0,48,false},{0,47,false},{0,52,true}};
  CHECK(rt.process(prepared,block,chord,3,sink));CHECK(nextOnTime);CHECK(rt.stop(sink)&&balance==0);
  // Sixty-four roots at distinct sample offsets; finish on the final owner's Off.
  rt.reset();block.samples=64;block.sampleRate=48000.;live::Trigger roots[64]{};
  for(int i=0;i<64;++i)roots[i]={i,48+i%12,true};
  CHECK(rt.process(prepared,block,roots,64,sink));const live::Trigger off{0,51,false};
  CHECK(rt.process(prepared,block,&off,1,sink));CHECK(!rt.triggerHeld()&&balance==0);
 }
 host::AutomationPlan plan;CHECK(plan.add(31,true,0.,64));CHECK(plan.add(0,false,1.,64));CHECK(plan.add(31,true,1.,64));plan.sort();
 CHECK(plan.points[0].offset==0&&plan.points[1].value==0.&&plan.points[2].value==1.);
 CHECK(!plan.add(64,true,1.,64));CHECK(!plan.add(0,true,std::nan(""),64));
 host::Model model;auto d=model.snapshot();CHECK(d.editable.enabled&&d.editable.songStartBar==2&&model.songStartBar()==2);
 d.editable.bars=1;d.editable.steps={};CHECK(edit::modify(d.editable,0,48,100,4));d.mode=live::Mode::MidiNote;d.gatePercent=100;
 CHECK(model.restore(d));CHECK(model.setGate(50));CHECK(model.setMode(live::Mode::MidiNote));
 auto stale=d;stale.mode=live::Mode::Transport;stale.gatePercent=0;
 CHECK(model.restore(stale,false));CHECK(model.snapshot().gatePercent==50&&model.snapshot().mode==live::Mode::MidiNote);
 live::Block block;block.samples=1000;block.sampleRate=1000;block.tempo=120;
 live::Trigger trigger{0,48,true};int on=0,off=0,offAt=-1;
 auto sink=[&](live::Output e)noexcept{if(e.on)++on;else{++off;offAt=e.offset;}return true;};
 audio=true;bool ok=model.process(block,&trigger,1,sink);audio=false;
 CHECK(ok&&allocations==0&&on==1&&off==1&&offAt==250);
 CHECK(model.restore(stale));CHECK(model.snapshot().mode==live::Mode::Transport&&model.snapshot().gatePercent==0);
 CHECK(model.setGate(100));CHECK(model.setMode(live::Mode::MidiNote));CHECK(model.snapshot().gatePercent==100);
 // Project state replacement must stop an old trigger exactly on adoption;
 // output rejection keeps that same reset pending without reviving ownership.
 {
  host::Model recalled;auto state=d;state.mode=live::Mode::MidiNote;state.gatePercent=100;
  CHECK(recalled.restore(state));live::Block small;small.samples=64;small.sampleRate=48000.;
  bool reject=false;int starts=0,ends=0;
  auto hear=[&](live::Output e)noexcept{if(!e.on&&reject){reject=false;return false;}if(e.on)++starts;else ++ends;return true;};
  CHECK(recalled.process(small,&trigger,1,hear));CHECK(starts==1&&ends==0);
  CHECK(recalled.restore(state));reject=true;
  CHECK(!recalled.process(small,nullptr,0,hear));CHECK(recalled.process(small,nullptr,0,hear));
  CHECK(starts==ends&&starts==1);CHECK(recalled.process(small,nullptr,0,hear));CHECK(starts==1);
 }
 // Entire published tuples must remain coherent, not just note tables.
 host::PatternExchange exchange;host::PreparedState a,b;
 CHECK(live::prepare(Phrase{1,{{0,48,100,1}}},a.pattern));a.root=48;a.songStart=1;a.gate=100;a.mode=live::Mode::Transport;
 CHECK(live::prepare(Phrase{2,{{0,60,100,1}}},b.pattern));b.root=60;b.scale=edit::Scale::Major;b.songStart=9999;b.gate=50;b.mode=live::Mode::MidiNote;
 CHECK(exchange.publish(a));std::atomic<bool> done{false},broken{false};
 std::thread writer([&]{for(int i=0;i<10000;++i)(void)exchange.publish(i%2?a:b);done=true;});
 unsigned consumed=0;do{std::uint64_t rev=0;const auto* s=exchange.acquire(rev);if(s){++consumed;bool first=s->root==48;
  if(s->pattern.cells[0].onPitch!=(first?48:60)||s->songStart!=(first?1:9999)||s->gate!=(first?100:50)||s->mode!=(first?live::Mode::Transport:live::Mode::MidiNote))broken=true;
 }}while(!done.load());writer.join();CHECK(!broken&&consumed>0);
 // Exercise Model::restore and the actual RT consumer concurrently. A trigger
 // at 48 must always sound 48 when source pitch and song root move together.
 {
  host::Model shared;auto first=shared.snapshot();first.mode=live::Mode::MidiNote;
  first.editable.steps={};first.editable.bars=1;CHECK(edit::modify(first.editable,0,48,100,1));
  auto second=first;v2::Session transposed;transposed.restore(first.session);transposed.setRootMidi(60);
  second.session=transposed.snapshot();second.editable.steps[0].pitch=60;second.editable.songStartBar=9999;
  CHECK(shared.restore(second));CHECK(shared.restore(first));std::atomic<bool> finished{false},failed{false};std::atomic<int> publishesFailed{0},wrongPitch{0};int rtFailed=0;
  std::thread producer([&]{for(int i=0;i<2000;++i)if(!shared.restore(i%2?first:second,false))++publishesFailed;finished=true;});
  live::Block part;part.samples=64;part.sampleRate=48000.;const live::Trigger go{0,48,true};
  auto consume=[&](live::Output e)noexcept{if(e.pitch!=48){++wrongPitch;failed=true;}return true;};
  do{audio=true;const bool stopped=shared.stop(consume),played=shared.process(part,&go,1,consume);audio=false;
   if(!stopped||!played){++rtFailed;failed=true;}
  }while(!finished.load());producer.join();if(failed||allocations)std::fprintf(stderr,"tuple stress: publish reject %d, wrong pitch %d, RT fail %d, allocations %u\n",publishesFailed.load(),wrongPitch.load(),rtFailed,allocations);CHECK(!failed&&allocations==0);CHECK(shared.stop(consume));
 }
 // Frozen corpus from the unchanged b1d2879 composer, measured independently
 // before accepting this repair; never regenerate to hide a musical regression.
 {
  std::uint64_t hash=14695981039346656037ULL;
  for(int style=0;style<3;++style)for(int bars:{4,8})for(int variation:{0,15,30,55,85})for(int seed=1;seed<=100;++seed){
   Config config;config.style=static_cast<Style>(style);config.bars=bars;config.variation=variation;config.seed=seed;
   for(auto byte:toMidi(v2::render(v2::compose(config)))){hash^=byte;hash*=1099511628211ULL;}
  }
  CHECK(hash==0x68003fbe9b876b02ULL);
 }
 // Paced current-machine callback measurements; not a Windows/DAW performance claim.
 host::Model timed;live::Block paced;paced.samples=64;paced.sampleRate=48000;paced.tempo=120;paced.playing=paced.ppqValid=paced.tempoValid=true;
 std::vector<double> micros;micros.reserve(750);int balance=0;bool valid=true;unsigned overruns=0;
 auto hear=[&](live::Output e)noexcept{balance+=e.on?1:-1;valid=valid&&balance>=0&&balance<=1;return true;};
 double timerPair=0.;
 for(int i=0;i<1000;++i){const auto t=std::chrono::steady_clock::now();timerPair+=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();}
 timerPair/=1000.;
 const auto epoch=std::chrono::steady_clock::now();
 for(int i=0;i<750;++i){paced.projectPpq=4.+i*64.*120./(60.*48000.);auto start=std::chrono::steady_clock::now();
  audio=true;ok=timed.process(paced,nullptr,0,hear);audio=false;CHECK(ok);
  auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();micros.push_back(us);if(us>64.*1.e6/48000.)++overruns;
  std::this_thread::sleep_until(epoch+std::chrono::microseconds((i+1)*64000000LL/48000));
 }
 audio=true;ok=timed.stop(hear);audio=false;CHECK(ok&&valid&&balance==0&&allocations==0);
 std::sort(micros.begin(),micros.end());
 std::printf("PASS repair policies, 12 decoded MIDI exports, coherent tuples (%u acquisitions), control precedence, zero callback allocations\n",consumed);
 std::printf("Linux paced 48k/64: mean=%.3fus p95=%.3fus p99=%.3fus max=%.3fus timer-pair-mean=%.3fus deadline=1333.333us overruns=%u (750 callbacks)\n",std::accumulate(micros.begin(),micros.end(),0.)/static_cast<double>(micros.size()),micros[712],micros[742],micros.back(),timerPair,overruns);
}
