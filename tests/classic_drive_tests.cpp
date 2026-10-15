#include "ebm/EditPattern.hpp"
#include "ebm/HostBridge.hpp"
#include "ebm/Composer.hpp"
#include "ebm/MidiWriter.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <set>
#include <vector>

#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<" " #x "\n";return 1;}}while(false)

int main(){
 using namespace ebm;
 CHECK(Config{}.rootMidi==48);
 CHECK(edit::studioOneOctave(Config{}.rootMidi)==2);
 // New default is C2, but existing projects written with MIDI 36 (C1)
 // must retain their exact notes and be byte-identical after decode/reencode.
 {
  Config old{};old.rootMidi=36;
  v2::Session oldSession(old);(void)oldSession.generate();
  auto state=host::Model{}.snapshot();
  state.session=oldSession.snapshot();
  state.editable=edit::fromPhrase(v2::render(oldSession.current()),4);
  // Source composer uses eight bars; preserve the displayed four-bar snapshot.
  host::DocumentBytes bytes{},again{};
  CHECK(host::encode(state,bytes));
  host::Document restored{};
  CHECK(host::decode(bytes.data(),bytes.size(),restored));
  CHECK(host::encode(restored,again));
  CHECK(bytes==again);
  CHECK(restored.session.options.rootMidi==36);
  CHECK(edit::toPhrase(restored.editable).notes==edit::toPhrase(state.editable).notes);
 }
 // A historic EBM2 project without a generated current motif serialized its
 // canonical fallback Config with C1. Both historic C1 and new C2 must decode.
 {
  auto doc=host::Model{}.snapshot();
  v2::Session session{};session.restore(doc.session);session.clearCurrent();
  doc.session=session.snapshot();
  host::DocumentBytes payload{};
  CHECK(host::encode(doc,payload));
  // EBM2 header 8 bytes; options 12 bytes; hasCurrent byte at 20;
  // storedCurrent Config root field: byte 23.
  CHECK(payload[20]==0u && payload[23]==48u);
  payload[23]=36u;
  std::uint32_t checksum=2166136261u;
  for(std::size_t i=0;i<payload.size()-4;++i){checksum^=payload[i];checksum*=16777619u;}
  for(int i=0;i<4;++i)payload[payload.size()-4+static_cast<std::size_t>(i)]=
     static_cast<std::uint8_t>(checksum>>(8*i));
  host::Document replay{};CHECK(host::decode(payload.data(),payload.size(),replay));
  CHECK(replay.session.options.rootMidi==48);
  CHECK(!replay.session.hasCurrent);
 }
 for(int bars:{1,2,4,8}){
  std::set<std::vector<int>> all;
  std::set<std::vector<int>> onsets;
  for(std::uint64_t seed=1;seed<=96;++seed){
   edit::Pattern pattern{};pattern.enabled=true;pattern.bars=bars;
   pattern.songStartBar=2;pattern.scale=edit::Scale::NaturalMinor;
   CHECK(edit::generateClassicDrive(pattern,48,seed));
   edit::conformToScale(pattern,48);
   CHECK(edit::valid(pattern));
   CHECK(pattern.bars==bars && pattern.songStartBar==2);
   const auto phrase=edit::toPhrase(pattern);
   CHECK(phrase.bars==bars);
   CHECK(static_cast<int>(phrase.notes.size())>=8*bars);
   CHECK(static_cast<int>(phrase.notes.size())<=12*bars);
   std::vector<int> signature;
   for(int bar=0;bar<bars;++bar){
     int weak=0,root=0,notes=0;
     for(int step=0;step<16;++step){
      const auto& n=pattern.steps[static_cast<std::size_t>(bar*16+step)];
      if(!n.on)continue;
      if(step%2==1)++weak;
      if(n.pitch==48)++root;
      ++notes;
      CHECK(edit::inScale(n.pitch,48,pattern.scale));
      CHECK(n.pitch>=48&&n.pitch<=60);
      CHECK(n.velocity>=1&&n.velocity<=127);
      signature.push_back(bar*16+step);
     }
     CHECK(notes>=8 && notes<=12);
     CHECK(weak>=1);
     CHECK(root*4>=notes*3); // >=75% tonic hits, not gratuitous melody
   }
   auto first=pattern;
   CHECK(edit::generateClassicDrive(first,48,seed));
   CHECK(edit::toPhrase(first).notes==phrase.notes);
   ebm::live::Pattern scheduled{};
   CHECK(ebm::live::prepare(phrase,scheduled));
   CHECK(scheduled.lengthSteps==bars*16);
   const auto midi=toMidi(phrase);
   CHECK(midi.size()>26&&midi[0]=='M'&&midi[1]=='T');
   std::vector<int> eventSignature;
   for(const auto& n:phrase.notes){eventSignature.push_back(n.step);eventSignature.push_back(n.pitch);eventSignature.push_back(n.velocity);eventSignature.push_back(n.duration);}
   all.insert(eventSignature);
   onsets.insert(signature);
  }
  CHECK(onsets.size()>=8);
  CHECK(all.size()>=20);
 }
 // LOCK is an explicit user override; generating a new pattern only replaces
 // unlocked cells, and the exact edited output is what gets saved and exported.
 {
  edit::Pattern existing{};existing.enabled=true;existing.bars=2;
  CHECK(edit::modify(existing,5,51,88,1));
  existing.steps[5].locked=true;
  const auto frozen=existing.steps[5];
  edit::Pattern generated{};generated.enabled=true;generated.bars=2;
  CHECK(edit::generateClassicDrive(generated,48,17));
  for(int i=0;i<edit::kMaxSteps;++i)
   if(existing.steps[static_cast<std::size_t>(i)].locked)
    generated.steps[static_cast<std::size_t>(i)]=existing.steps[static_cast<std::size_t>(i)];
  CHECK(generated.steps[5].locked && generated.steps[5].on);
  CHECK(generated.steps[5].pitch==frozen.pitch && generated.steps[5].velocity==frozen.velocity);
  host::Document doc=host::Model{}.snapshot();
  doc.editable=generated;
  host::DocumentBytes encoded{};CHECK(host::encode(doc,encoded));
  host::Document decoded{};CHECK(host::decode(encoded.data(),encoded.size(),decoded));
  CHECK(edit::toPhrase(decoded.editable).notes==edit::toPhrase(generated).notes);
  host::Model consumer;CHECK(consumer.restore(decoded));
  CHECK(edit::toPhrase(consumer.snapshot().editable).notes==edit::toPhrase(generated).notes);
 }
 std::cout<<"PASS: C2 new default / C1 recall, motoric CLASSIC rhythms at 1/2/4/8 bars,"
             " tonic focus, variation, scale, MIDI export and LOCK recall\n";
}
