#pragma once
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "ebm/HostBridge.hpp"
#include <cstdio>
#include <limits>
namespace ebm::qa {
class LimitedEvents:public Steinberg::Vst::EventList {
public:
 int rejectOffs=0;
 LimitedEvents():EventList(256){}
 Steinberg::tresult PLUGIN_API addEvent(Steinberg::Vst::Event& e)override{
  if(e.type==Steinberg::Vst::Event::kNoteOffEvent&&rejectOffs>0){--rejectOffs;return Steinberg::kResultFalse;}
  return EventList::addEvent(e);
 }
};
inline int processChecks(Steinberg::Vst::IComponent* component,const host::Document& original){
 using namespace Steinberg;using namespace Steinberg::Vst;
 #define PROBE(x) do{if(!(x)){std::fprintf(stderr,"PROCESSOR FAIL %d: %s\n",__LINE__,#x);return 2;}}while(false)
 IAudioProcessor* audio=nullptr;
 PROBE(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&audio))==kResultOk&&audio);
 auto doc=original;doc.mode=live::Mode::MidiNote;doc.gatePercent=100;
 doc.editable={};doc.editable.enabled=true;doc.editable.bars=1;doc.editable.songStartBar=1;
 PROBE(edit::modify(doc.editable,0,48,100,4));
 auto restore=[&](){host::DocumentBytes bytes{};if(!host::encode(doc,bytes))return false;
  MemoryStream stream(bytes.data(),static_cast<TSize>(bytes.size()));return component->setState(&stream)==kResultOk;};
 for(int format:{kSample32,kSample64}){
  doc.mode=live::Mode::MidiNote;doc.gatePercent=100;
  ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=format;setup.maxSamplesPerBlock=256;setup.sampleRate=48000.;
  PROBE(audio->setupProcessing(setup)==kResultOk);
  PROBE(component->activateBus(kEvent,kInput,0,true)==kResultOk);
  PROBE(component->activateBus(kEvent,kOutput,0,true)==kResultOk);
  PROBE(component->setActive(true)==kResultOk);PROBE(audio->setProcessing(true)==kResultOk);
  LimitedEvents input,output;ParameterChanges changes;
  ProcessContext ctx{};ctx.sampleRate=48000.;ctx.tempo=120.;ctx.projectTimeMusic=0.;
  ctx.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid;
  ProcessData data{};data.numSamples=256;data.symbolicSampleSize=format;data.processContext=&ctx;
  data.inputEvents=&input;data.outputEvents=&output;
  auto note=[&](int pitch,int offset,bool on){Event e{};e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;e.sampleOffset=offset;
   if(on){e.noteOn.pitch=static_cast<int16>(pitch);e.noteOn.velocity=1.f;e.noteOn.noteId=-1;}
   else{e.noteOff.pitch=static_cast<int16>(pitch);e.noteOff.noteId=-1;}
   return input.addEvent(e)==kResultOk;};
  for(int attack=0;attack<8;++attack){
   PROBE(restore());input.clear();output.clear();PROBE(note(48,0,true));
   PROBE(audio->process(data)==kResultOk);PROBE(output.getEventCount()==1);
   input.clear();output.clear();output.rejectOffs=1;
   if(attack==0)PROBE(note(48,-1,true));
   if(attack==1)PROBE(note(128,0,true));
   if(attack==2)PROBE(note(-1,0,true));
   if(attack==3)PROBE(note(48,256,true));
   if(attack==4)for(int n=0;n<65;++n)PROBE(note(48,0,true));
   if(attack>=5){PROBE(note(48,0,true));input.getEventByIndex(0)->noteOn.velocity=
      attack==5?std::numeric_limits<float>::quiet_NaN():attack==6?std::numeric_limits<float>::infinity():-0.1f;}
   PROBE(audio->process(data)!=kResultOk);PROBE(output.getEventCount()==0);
   input.clear();PROBE(audio->process(data)==kResultOk);PROBE(output.getEventCount()==1);
   PROBE(output.getEventByIndex(0)->type==Event::kNoteOffEvent);output.clear();
   for(int block=0;block<32;++block)PROBE(audio->process(data)==kResultOk);
   PROBE(output.getEventCount()==0);
  }
  // Matching trigger NoteOff with limited output capacity must permanently stop.
  PROBE(restore());input.clear();output.clear();PROBE(note(48,0,true));PROBE(audio->process(data)==kResultOk);
  input.clear();output.clear();PROBE(note(48,100,false));output.rejectOffs=1;
  PROBE(audio->process(data)!=kResultOk);input.clear();PROBE(audio->process(data)==kResultOk);
  PROBE(output.getEventCount()==1&&output.getEventByIndex(0)->type==Event::kNoteOffEvent);
  output.clear();for(int n=0;n<32;++n)PROBE(audio->process(data)==kResultOk);PROBE(output.getEventCount()==0);
  // Studio One-compatible VST3 delivery permutations, not a Studio One capture:
  // 1-sample gaps, an Off at the last sample of one block, On in the next,
  // and a long silent interval. Every generated onset keeps the original grid.
  const auto savedDoc=doc;
  doc.editable={};doc.editable.enabled=true;doc.editable.bars=1;doc.editable.songStartBar=1;
  for(int step=0;step<16;++step)PROBE(edit::modify(doc.editable,step,48,100,1));
  PROBE(restore());ctx.state|=ProcessContext::kPlaying;
  const live::Trigger timeline[]={{0,48,true},{6100,48,false},{6101,52,true},
     {12287,52,false},{12288,57,true},{18100,57,false},{40001,55,true},{49000,55,false}};
  const int expectedTimes[]={0,6000,12000,18000,42000,48000};
  const int expectedRoots[]={48,48,52,57,55,55};int emitted=0,voice=-1;
  for(int origin=0;origin<50176;origin+=256){
   ctx.projectTimeMusic=origin/24000.;input.clear();output.clear();
   for(auto e:timeline)if(e.offset>=origin&&e.offset<origin+256)PROBE(note(e.pitch,e.offset-origin,e.on));
   PROBE(audio->process(data)==kResultOk);
   for(int32 n=0;n<output.getEventCount();++n){const auto* e=output.getEventByIndex(n);
    if(e->type==Event::kNoteOnEvent){
     PROBE(voice<0&&emitted<6);PROBE(origin+e->sampleOffset==expectedTimes[emitted]);
     PROBE(e->noteOn.pitch==expectedRoots[emitted]);voice=e->noteOn.pitch;++emitted;
    }else{PROBE(e->type==Event::kNoteOffEvent&&voice==e->noteOff.pitch);voice=-1;}
   }
  }
  PROBE(emitted==6&&voice==-1);
  // Overlap: new root before the old Off must retune at the On offset;
  // the late old Off must not close the new owner's gate or restart phase.
  PROBE(restore());input.clear();output.clear();PROBE(note(48,0,true));PROBE(audio->process(data)==kResultOk);
  input.clear();output.clear();PROBE(note(52,31,true));PROBE(note(48,32,false));PROBE(audio->process(data)==kResultOk);
  PROBE(output.getEventCount()==2&&output.getEventByIndex(0)->type==Event::kNoteOffEvent&&output.getEventByIndex(0)->sampleOffset==31);
  PROBE(output.getEventByIndex(1)->type==Event::kNoteOnEvent&&output.getEventByIndex(1)->noteOn.pitch==52&&output.getEventByIndex(1)->sampleOffset==31);
  input.clear();output.clear();PROBE(note(52,64,false));PROBE(audio->process(data)==kResultOk);
  PROBE(output.getEventCount()==1&&output.getEventByIndex(0)->type==Event::kNoteOffEvent&&output.getEventByIndex(0)->sampleOffset==64);
  // A real observed host stop closes the voice even with no input NoteOff,
  // ignores same-callback chase, and a later stopped-live On starts at step 1.
  PROBE(restore());input.clear();output.clear();PROBE(note(48,0,true));PROBE(audio->process(data)==kResultOk);
  PROBE(output.getEventCount()==1);ctx.state&=~ProcessContext::kPlaying;
  input.clear();output.clear();PROBE(note(52,0,true));PROBE(audio->process(data)==kResultOk);
  PROBE(output.getEventCount()==1&&output.getEventByIndex(0)->type==Event::kNoteOffEvent&&output.getEventByIndex(0)->sampleOffset==0);
  input.clear();output.clear();PROBE(audio->process(data)==kResultOk);PROBE(output.getEventCount()==0);
  PROBE(note(55,7,true));PROBE(audio->process(data)==kResultOk);
  PROBE(output.getEventCount()==1&&output.getEventByIndex(0)->type==Event::kNoteOnEvent&&output.getEventByIndex(0)->sampleOffset==7);
  std::printf("PASS actual VST3 MIDI phase format %d: C/E/A/G, 1-sample/cross-block/long gaps, exact original grid, balanced voices, host stop/chase/live restart\n",format);
  input.clear();output.clear();PROBE(note(55,0,false));PROBE(audio->process(data)==kResultOk);
  PROBE(output.getEventCount()==1&&output.getEventByIndex(0)->type==Event::kNoteOffEvent);
  doc=savedDoc;ctx.projectTimeMusic=0.;
  // Actual VST3 parameter queue: a late Gate=0 must not suppress the early NoteOn.
  doc.mode=live::Mode::Transport;PROBE(restore());ctx.state|=ProcessContext::kPlaying;
  changes.clearQueue();int32 qi=0,pi=0;auto* gate=changes.addParameterData(102,qi);PROBE(gate);
  PROBE(gate->addPoint(255,0.,pi)==kResultOk);data.inputParameterChanges=&changes;
  input.clear();output.clear();PROBE(audio->process(data)==kResultOk);PROBE(output.getEventCount()==2);
  PROBE(output.getEventByIndex(0)->type==Event::kNoteOnEvent&&output.getEventByIndex(0)->sampleOffset==0);
  PROBE(output.getEventByIndex(1)->type==Event::kNoteOffEvent&&output.getEventByIndex(1)->sampleOffset==255);
  // Mode changes take effect at 64, with a second point retained at 128.
  PROBE(restore());changes.clearQueue();auto* mode=changes.addParameterData(100,qi);PROBE(mode);
  PROBE(mode->addPoint(64,1.,pi)==kResultOk);PROBE(mode->addPoint(128,0.,pi)==kResultOk);
  output.clear();PROBE(audio->process(data)==kResultOk);PROBE(output.getEventCount()==2);
  PROBE(output.getEventByIndex(1)->type==Event::kNoteOffEvent&&output.getEventByIndex(1)->sampleOffset==64);
  data.inputParameterChanges=nullptr;ctx.state&=~ProcessContext::kPlaying;
  output.clear();PROBE(audio->process(data)==kResultOk);
  PROBE(audio->setProcessing(false)==kResultOk);PROBE(audio->setProcessing(true)==kResultOk);
  PROBE(audio->setProcessing(false)==kResultOk);PROBE(component->setActive(false)==kResultOk);
  std::printf("PASS actual VST3 ProcessData format %d: invalid offsets/pitches/overflow, NoteOff retry, Gate/Mode offsets, lifecycle\n",format);
 }
 audio->release();
 #undef PROBE
 return 0;
}
}
