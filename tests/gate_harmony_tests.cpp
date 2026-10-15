#include "ebm/Harmony.hpp"
#include "ebm/HostBridge.hpp"
#include "ebm/MidiWriter.hpp"
#include "../src/vst3/UiGeometry.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace ebm;
#define CHECK(x) do {if(!(x)){std::cerr<<"GATE/HARMONY FAIL line "<<__LINE__<<" : "#x"\n";return 1;}}while(false)
struct Seen {int sample,pitch,velocity;bool on;};
int main(){
 using ebm::edit::Scale;
 for(int root:{48,60}){
   CHECK(harmony::chordPitch(root,root,root,Scale::Major)==root);
   CHECK(harmony::chordPitch(root+4,root,root+4,Scale::Major)==root+7);
   CHECK(harmony::chordPitch(root+7,root,root+4,Scale::Major)==root+11);
   CHECK(harmony::chordPitch(root,root,root+9,Scale::Major)==root+9);
   CHECK(harmony::chordPitch(root+4,root,root+9,Scale::Major)==root+12);
   CHECK(harmony::chordPitch(root+7,root,root+9,Scale::Major)==root+16);
   CHECK(harmony::chordPitch(root+7,root,root+7,Scale::Major)==root+14);
 }
 CHECK(harmony::chordPitch(48,48,52,Scale::NaturalMinor)==51); // E trigger snaps to Eb in C minor
 CHECK(harmony::chordPitch(55,48,52,Scale::Chromatic)==59);
 CHECK(edit::lengthText(1)=="1/16"&&edit::lengthText(2)=="1/8");
 CHECK(edit::lengthText(4)=="1/4"&&edit::lengthText(16)=="1 BAR");
 CHECK(edit::lengthText(3)=="3/16"&&edit::lengthText(32)=="2 BAR");
 CHECK(vst3::ui::hit(750,518).target==vst3::ui::Target::Gate);
 CHECK(vst3::ui::hit(970,518).target==vst3::ui::Target::Export);
 CHECK(vst3::ui::gateValue(vst3::ui::gateXStart)==0);
 CHECK(vst3::ui::gateValue(vst3::ui::gateXEnd)==100);
 for(int x=vst3::ui::gateXStart;x<vst3::ui::gateXEnd;++x)
  CHECK(vst3::ui::gateValue(x)<=vst3::ui::gateValue(x+1));
 host::Model model;
 auto doc=model.snapshot();doc.gatePercent=50;doc.editable.enabled=true;
 doc.editable.bars=1;doc.editable.scale=Scale::Major;doc.mode=live::Mode::MidiNote;
 doc.editable.steps={};
 CHECK(edit::modify(doc.editable,0,48,100,4));
 CHECK(edit::modify(doc.editable,4,52,110,2));
 CHECK(edit::modify(doc.editable,6,55,90,2));
 CHECK(model.restore(doc));
 CHECK(model.snapshot().gatePercent==50);
 host::DocumentBytes bytes{};CHECK(host::encode(model.snapshot(),bytes));
 host::Document decoded{};CHECK(host::decode(bytes.data(),bytes.size(),decoded));
 CHECK(decoded.gatePercent==50);
 auto damaged=bytes;damaged[damaged.size()-5]^=1;
 CHECK(!host::decode(damaged.data(),damaged.size(),decoded));
 CHECK(decoded.gatePercent==50);
 // Simulated EBM2 document: same serialized data less the one v3 gate byte.
 std::array<std::uint8_t,host::kV2DocumentBytes> v2{};
 std::copy_n(bytes.begin(),host::kV2DocumentBytes-4,v2.begin());
 v2[3]='2';v2[4]=2;v2[5]=0;v2[6]=0;v2[7]=0;
 std::uint32_t h=2166136261u;
 for(std::size_t i=0;i<v2.size()-4;++i){h^=v2[i];h*=16777619u;}
 for(int i=0;i<4;++i)v2[v2.size()-4+i]=static_cast<std::uint8_t>(h>>(8*i));
 CHECK(host::decode(v2.data(),v2.size(),decoded));
 CHECK(decoded.gatePercent==100);CHECK(decoded.editable.scale==Scale::Major);
 // MIDI file must obey same Gate proportion at tick precision.
 const auto phrase=edit::toPhrase(doc.editable);
 const auto full=toMidi(phrase,100),half=toMidi(phrase,50),muted=toMidi(phrase,0);
 CHECK(full!=half && half!=muted);
 CHECK(full.size()>muted.size()&&half.size()>muted.size());
 CHECK(std::string(full.begin(),full.begin()+4)=="MThd");
 // Detailed sample-accurate runtime: 120 BPM at 1000 Hz => 125 samples/sixteenth.
 live::Runtime rt;rt.setMode(live::Mode::MidiNote);
 live::Pattern pat{};CHECK(live::prepare(phrase,pat));
 live::Block block;block.samples=1000;block.sampleRate=1000;block.tempo=120;
 block.scale=Scale::Major;block.songRoot=48;block.gatePercent=50;
 const live::Trigger change[]={{0,48,true},{500,48,false},{500,52,true},{750,52,false},{750,57,true}};
 std::vector<Seen> events;
 auto sink=[&](live::Output e) noexcept {events.push_back({e.offset,e.pitch,e.velocity,e.on});return true;};
 CHECK(rt.process(pat,block,change,5,sink));
 CHECK(rt.triggerHeld());
 CHECK(std::any_of(events.begin(),events.end(),[](const Seen&e){return e.on&&e.sample==0&&e.pitch==48;}));
 CHECK(std::any_of(events.begin(),events.end(),[](const Seen&e){return !e.on&&e.sample==250&&e.pitch==48;}));
 CHECK(std::any_of(events.begin(),events.end(),[](const Seen&e){return e.on&&e.sample==500&&e.pitch==55;}));
 CHECK(std::any_of(events.begin(),events.end(),[](const Seen&e){return e.on&&e.sample==750&&e.pitch==64;}));
 // Exit NOTE trigger, clean NoteOff, no stuck voice.
 const live::Trigger stop{0,57,false};
 CHECK(rt.process(pat,block,&stop,1,sink));CHECK(!rt.triggerHeld()&&rt.activePitch()==-1);
 int nOn=0,nOff=0;for(auto e:events){if(e.on)++nOn;else ++nOff;}CHECK(nOn==nOff);
 live::Runtime zero;zero.setMode(live::Mode::MidiNote);
 block.gatePercent=0;const live::Trigger trig{0,48,true};
 events.clear();CHECK(zero.process(pat,block,&trig,1,sink));CHECK(events.empty());
 // Ten virtual minutes of continuous MIDI-note triggering: no imposed timeout.
 // 128 samples at 1000 Hz per callback x 4688 callbacks ~600 seconds.
 {
  live::Runtime longRun;longRun.setMode(live::Mode::MidiNote);
  live::Block period;period.samples=128;period.sampleRate=1000.;period.tempo=120.;
  period.songRoot=48;period.scale=Scale::Major;period.gatePercent=75;
  int on=0,off=0;
  auto count=[&](live::Output e)noexcept{if(e.on)++on;else ++off;return true;};
  const live::Trigger go{0,48,true};
  CHECK(longRun.process(pat,period,&go,1,count));
  for(int i=0;i<4687;++i)CHECK(longRun.process(pat,period,nullptr,0,count));
  CHECK(longRun.triggerHeld());
  CHECK(on>100&&off>100&&on-off>=0&&on-off<=1);
  const live::Trigger end{0,48,false};
  CHECK(longRun.process(pat,period,&end,1,count));
  CHECK(on==off);
 }
 std::cout<<"PASS: diatonic C/Em/Am/G, fractional gate/MIDI export, 0/100%, EBM2 migration, 10-minute MIDI continuity\n";
}
