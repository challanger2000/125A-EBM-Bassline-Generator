#pragma once
#include "ebm/LiveMidi.hpp"
#include <array>
#include <cstdio>
#include <vector>
namespace ebm::qa {
inline int midiPhaseTests(){
 #define PHASE_CHECK(x) do{if(!(x)){std::fprintf(stderr,"PHASE FAIL %d: %s\n",__LINE__,#x);return 1;}}while(false)
 using namespace live;
 Pattern pattern;pattern.ready=true;pattern.bars=1;pattern.lengthSteps=16;
 for(auto& cell:pattern.cells){cell.onPitch=48;cell.velocity=100;cell.duration=1;}
 struct Heard{int sample,pitch;bool on;};
 int cases=0;
 // Exact clock oracle: every sixteenth is sr/8 samples at 120 BPM.
 // Positive gaps must emit no off-grid restart; zero-gap retunes are allowed.
 for(int sr:{800,48000,96000})for(int size:{1,31,127,256})for(int gap:{0,1,17,300,5000}){
  const int step=sr/8,anchor=3;
  std::vector<Trigger> timeline{{anchor,48,true}};
  int cursor=anchor+step+step/3,previous=48;
  for(int root:{52,57,55}){
   timeline.push_back({cursor,previous,false});timeline.push_back({cursor+gap,root,true});
   cursor+=gap+step+step/3;previous=root;
  }
  timeline.push_back({cursor,previous,false});const int end=cursor+2*step;
  Runtime runtime;runtime.setMode(Mode::MidiNote);std::vector<Heard> heard;
  for(int origin=0;origin<end;origin+=size){
   Block b;b.samples=std::min(size,end-origin);b.sampleRate=sr;b.tempo=120.;b.songRoot=48;
   Trigger local[8]{};int count=0;
   for(auto e:timeline)if(e.offset>=origin&&e.offset<origin+b.samples){e.offset-=origin;local[count++]=e;}
   auto sink=[&](Output e){heard.push_back({origin+e.offset,e.pitch,e.on});return true;};
   PHASE_CHECK(runtime.process(pattern,b,local,count,sink));
  }
  int held=-1;std::size_t next=0;int expectedGrid=0,actualGrid=0,voice=-1;
  for(int t=anchor;t<end;t+=step){
   while(next<timeline.size()&&timeline[next].offset<=t){auto e=timeline[next++];if(e.on)held=e.pitch;else if(held==e.pitch)held=-1;}
   if(held>=0){++expectedGrid;bool found=false;for(auto e:heard)if(e.on&&e.sample==t&&e.pitch==held)found=true;PHASE_CHECK(found);}
  }
  for(auto e:heard){
   if(e.on){
    PHASE_CHECK(voice<0);voice=e.pitch;
    if((e.sample-anchor)%step==0)++actualGrid;
    else{bool retune=false;if(gap==0)for(auto t:timeline)retune=retune||(t.on&&t.offset==e.sample);PHASE_CHECK(retune);}
   }else{PHASE_CHECK(voice==e.pitch);voice=-1;}
  }
  if(expectedGrid!=actualGrid||voice!=-1||runtime.triggerHeld()||runtime.activePitch()!=-1)std::fprintf(stderr,"matrix sr=%d size=%d gap=%d expected=%d actual=%d voice=%d held=%d active=%d\n",sr,size,gap,expectedGrid,actualGrid,voice,runtime.triggerHeld(),runtime.activePitch());
  PHASE_CHECK(expectedGrid==actualGrid&&voice==-1&&!runtime.triggerHeld()&&runtime.activePitch()==-1);
  // Every non-simultaneous Off closes a currently sounding bass at that sample.
  if(gap>0)for(auto t:timeline)if(!t.on){bool found=false;for(auto e:heard)found=found||(!e.on&&e.sample==t.offset);PHASE_CHECK(found);}
  ++cases;
 }
 // Long silent gap wraps the clock without reviving a released note; same-key
 // retrigger continues phase. Explicit stop alone makes the next On step one.
 Runtime runtime;runtime.setMode(Mode::MidiNote);std::vector<Output> out;
 auto sink=[&](Output e){out.push_back(e);return true;};
 Block b;b.samples=40;b.sampleRate=800.;b.tempo=120.;
 Trigger on{0,48,true};PHASE_CHECK(runtime.process(pattern,b,&on,1,sink));
 Trigger off{0,48,false};out.clear();PHASE_CHECK(runtime.process(pattern,b,&off,1,sink));
 PHASE_CHECK(out.size()==1&&!out[0].on&&out[0].offset==0);
 b.samples=100;out.clear();for(int n=0;n<1000;++n)PHASE_CHECK(runtime.process(pattern,b,nullptr,0,sink));PHASE_CHECK(out.empty());
 b.samples=40;PHASE_CHECK(runtime.process(pattern,b,&on,1,sink));PHASE_CHECK(out.size()==1&&out[0].on&&out[0].offset==20);
 PHASE_CHECK(runtime.stop(sink));out.clear();PHASE_CHECK(runtime.process(pattern,b,&on,1,sink));PHASE_CHECK(out.size()==1&&out[0].on&&out[0].offset==0);
 // Observed host stop edge is immediate, dominates simultaneous/chased input,
 // retries output rejection once and permits a later stopped-live fresh start.
 runtime.reset();out.clear();b.transportValid=true;b.playing=true;
 PHASE_CHECK(runtime.process(pattern,b,&on,1,sink));out.clear();b.playing=false;
 bool reject=true;auto limited=[&](Output e){if(!e.on&&reject){reject=false;return false;}out.push_back(e);return true;};
 PHASE_CHECK(!runtime.process(pattern,b,&on,1,limited));PHASE_CHECK(!runtime.triggerHeld());
 PHASE_CHECK(runtime.process(pattern,b,nullptr,0,limited));PHASE_CHECK(out.size()==1&&!out[0].on&&out[0].offset==0);
 out.clear();PHASE_CHECK(runtime.process(pattern,b,nullptr,0,limited));PHASE_CHECK(out.empty());
 PHASE_CHECK(runtime.process(pattern,b,&on,1,limited));PHASE_CHECK(out.size()==1&&out[0].on&&out[0].offset==0);
 PHASE_CHECK(runtime.stop(limited));
 // Absent transport context is not an invented stop; mode changes are explicit.
 runtime.reset();out.clear();b.transportValid=true;b.playing=true;PHASE_CHECK(runtime.process(pattern,b,&on,1,sink));
 b.transportValid=false;b.playing=false;out.clear();PHASE_CHECK(runtime.process(pattern,b,nullptr,0,sink));PHASE_CHECK(runtime.triggerHeld());
 runtime.setMode(Mode::Transport);PHASE_CHECK(runtime.process(pattern,b,nullptr,0,sink));PHASE_CHECK(!runtime.triggerHeld()&&runtime.activePitch()==-1);
 runtime.setMode(Mode::MidiNote);out.clear();PHASE_CHECK(runtime.process(pattern,b,&on,1,sink));PHASE_CHECK(out.size()==1&&out[0].on&&out[0].offset==0);PHASE_CHECK(runtime.stop(sink));
 std::printf("PASS MIDI phase: %d C/E/A/G gap/block/rate cases, exact grid/pitches, balanced voices, long silent wrap, same-key retrigger, explicit stop/retry/mode/live restart\n",cases);
 #undef PHASE_CHECK
 return 0;
}
}
