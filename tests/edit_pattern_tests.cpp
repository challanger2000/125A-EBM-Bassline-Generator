#include "ebm/EditPattern.hpp"
#include "ebm/HostBridge.hpp"
#include <iostream>
#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(){
 namespace e=ebm::edit;
 e::Pattern p{};p.enabled=true;
 for(int bars:{1,2,4,8}){
  p.bars=bars;
  CHECK(e::clear(p));
  CHECK(e::modify(p,0,36,12,16));
  CHECK(e::modify(p,8,39,127,8));
  auto phrase=e::toPhrase(p);
  CHECK(phrase.bars==bars);
  CHECK(phrase.notes.size()==2);
  CHECK(phrase.notes[0].duration==8);
  CHECK(phrase.notes[1].velocity==127);
  ebm::live::Pattern rt{};
  CHECK(ebm::live::prepare(phrase,rt));
  CHECK(rt.lengthSteps==bars*16);
  CHECK(rt.cells[0].onPitch==36);
  CHECK(rt.cells[8].offPitch==36);
 }
 // All ten scales have distinct, accepted interval tables; the first
 // three stable IDs are unchanged for existing EBM2 project states.
 static_assert(static_cast<int>(e::Scale::Chromatic)==0);
 static_assert(static_cast<int>(e::Scale::Major)==1);
 static_assert(static_cast<int>(e::Scale::NaturalMinor)==2);
 static_assert(e::kScaleCount==10);
 static_assert(e::studioOneOctave(60)==3);
 static_assert(e::studioOneOctave(48)==2);
 static_assert(e::studioOneOctave(36)==1);
 CHECK(e::inScale(47,36,e::Scale::HarmonicMinor)); // B is minor leading tone
 CHECK(!e::inScale(46,36,e::Scale::HarmonicMinor));
 CHECK(e::inScale(45,36,e::Scale::MelodicMinor));
 CHECK(e::inScale(46,36,e::Scale::Dorian));
 CHECK(e::inScale(37,36,e::Scale::Phrygian));
 CHECK(e::inScale(46,36,e::Scale::Mixolydian));
 CHECK(e::inScale(42,36,e::Scale::Lydian));
 CHECK(!e::inScale(38,36,e::Scale::MinorPentatonic));
 CHECK(e::inScale(39,36,e::Scale::MinorPentatonic));
 for(int bars:{1,2,4,8}){
  e::Pattern original{};original.enabled=true;original.bars=bars;
  original.scale=e::Scale::NaturalMinor;
  original.songStartBar=2;
  CHECK(e::modify(original,0,36,88,1));
  CHECK(e::modify(original,4,39,110,2));
  original.steps[4].locked=true;
  const auto before=e::toPhrase(original).notes;
  CHECK(e::variateVisible(original,35,1,36,0)==e::VariationEdit::Accent);
  CHECK(original.steps[4].velocity==110 && original.steps[4].pitch==39);
  CHECK(e::toPhrase(original).notes!=before);
  CHECK(e::variateVisible(original,35,2,36,0)==e::VariationEdit::Pitch);
  CHECK(e::inScale(original.steps[0].pitch,36,e::Scale::NaturalMinor));
  CHECK(e::variateVisible(original,35,3,36,0)==e::VariationEdit::Gate);
  CHECK(e::variateVisible(original,35,4,36,0)==e::VariationEdit::Timing);
  CHECK(e::toPhrase(original).bars==bars);
  ebm::live::Pattern live{};
  CHECK(ebm::live::prepare(e::toPhrase(original),live));
  auto held=original;
  CHECK(e::variateVisible(held,0,1,36,0)==e::VariationEdit::None);
  CHECK(held.steps[4].locked);
 }
 {
  e::Pattern onlyLocked{};onlyLocked.enabled=true;onlyLocked.bars=1;
  CHECK(e::modify(onlyLocked,0,36,90,1));onlyLocked.steps[0].locked=true;
  CHECK(e::variateVisible(onlyLocked,75,1,36,0)==e::VariationEdit::None);
  e::Pattern scalePat{};scalePat.enabled=true;scalePat.bars=1;
  scalePat.scale=e::Scale::NaturalMinor;
  CHECK(e::modify(scalePat,0,42,110,1)); // F# not in C natural minor
  CHECK(e::modify(scalePat,1,42,90,1));scalePat.steps[1].locked=true;
  e::conformToScale(scalePat,36);
  CHECK(e::inScale(scalePat.steps[0].pitch,36,scalePat.scale));
  CHECK(scalePat.steps[1].pitch==42); // locked notes preserve user intent
 }
 CHECK(e::inScale(39,36,e::Scale::NaturalMinor));
 CHECK(!e::inScale(40,36,e::Scale::NaturalMinor));
 CHECK(e::nearestPitch(40,36,e::Scale::NaturalMinor)==41);
 CHECK(e::copyBar(p,0,1));
 p.steps[16].locked=true;
 CHECK(!e::copyBar(p,0,1));
 CHECK(!e::toggle(p,16));
 CHECK(e::clear(p));
 CHECK(p.steps[16].on);
 ebm::host::Model model;
 auto state=model.snapshot();
 state.editable=p;state.editable.bars=1;state.editable.songStartBar=2;
 ebm::host::DocumentBytes bytes{};
 CHECK(ebm::host::encode(state,bytes));
 ebm::host::Document recovered{};
 CHECK(ebm::host::decode(bytes.data(),bytes.size(),recovered));
 CHECK(recovered.editable.bars==1 && recovered.editable.songStartBar==2);
 CHECK(recovered.editable.steps[16].locked);
 CHECK(model.restore(recovered));
 CHECK(model.songStartBar()==2);
 CHECK(model.snapshot().editable.steps[16].locked);
 // Extended scale values use the same EBM2 1-byte field and round-trip.
 state.editable.scale=e::Scale::Phrygian;
 CHECK(ebm::host::encode(state,bytes));
 CHECK(ebm::host::decode(bytes.data(),bytes.size(),recovered));
 CHECK(recovered.editable.scale==e::Scale::Phrygian);
 CHECK(model.restore(recovered));
 CHECK(model.snapshot().editable.scale==e::Scale::Phrygian);
 auto corrupt=bytes;corrupt[corrupt.size()-20]^=0x1;
 CHECK(!ebm::host::decode(corrupt.data(),corrupt.size(),recovered));
 CHECK(recovered.editable.bars==1);
 // A backwards-compatible EBM1 snapshot must still be accepted.
 auto legacy=bytes;
 legacy[3]='1';legacy[4]=1;
 std::uint32_t crc=2166136261u;
 for(std::size_t i=0;i<ebm::host::kLegacyDocumentBytes-4;++i){crc^=legacy[i];crc*=16777619u;}
 for(int i=0;i<4;++i)legacy[ebm::host::kLegacyDocumentBytes-4+i]=static_cast<std::uint8_t>(crc>>(8*i));
 CHECK(ebm::host::decode(legacy.data(),ebm::host::kLegacyDocumentBytes,recovered));
 CHECK(!recovered.editable.enabled);
 CHECK(ebm::edit::toPhrase(recovered.editable).bars==8);
 // An active MIDI trigger must survive a GUI-authored pattern swap, including
 // the next step's new pitch/velocity and its proper NoteOff.
 {
  ebm::host::Model live;
  auto doc=live.snapshot();
  doc.mode=ebm::live::Mode::MidiNote;
  doc.editable=ebm::edit::Pattern{};
  doc.editable.enabled=true;doc.editable.bars=1;doc.editable.songStartBar=2;
  CHECK(ebm::edit::modify(doc.editable,0,36,77,1));
  CHECK(live.restore(doc));
  ebm::live::Block block{};block.samples=6000;block.sampleRate=48000;block.tempo=120;
  const ebm::live::Trigger trigger{0,48,true};
  int ons=0,offs=0;int lastPitch=-1;int lastVelocity=0;
  auto sink=[&](ebm::live::Output out) noexcept {
    if(out.on){++ons;lastPitch=out.pitch;lastVelocity=out.velocity;}
    else ++offs;
    return true;
  };
  CHECK(live.process(block,&trigger,1,sink));
  CHECK(ons==1 && lastPitch==36 && lastVelocity==77);
  CHECK(ebm::edit::modify(doc.editable,1,39,119,1));
  CHECK(live.restore(doc));
  CHECK(live.process(block,nullptr,0,sink));
  CHECK(ons>=2 && lastPitch==39 && lastVelocity==119);
  ebm::live::Trigger release{0,48,false};
  CHECK(live.process(block,&release,1,sink));
  CHECK(ons==offs);
 }
 // A live edit to a future step must not cut a note that was already
 // sounding; it ends at its original gate even if the MIDI plan was swapped.
 {
  ebm::host::Model live;
  auto doc=live.snapshot();
  doc.mode=ebm::live::Mode::MidiNote;
  doc.editable=ebm::edit::Pattern{};
  doc.editable.enabled=true;doc.editable.bars=1;
  CHECK(ebm::edit::modify(doc.editable,0,36,100,2));
  CHECK(live.restore(doc));
  ebm::live::Block block{};block.samples=6000;block.sampleRate=48000;block.tempo=120;
  const ebm::live::Trigger start{0,48,true};
  int on=0,off=0;
  auto sink=[&](ebm::live::Output e)noexcept -> bool {
   if(e.on)++on;else ++off;return true;
  };
  CHECK(live.process(block,&start,1,sink));
  CHECK(on==1&&off==0);
  CHECK(ebm::edit::modify(doc.editable,3,40,80,1));
  CHECK(live.restore(doc));
  CHECK(live.process(block,nullptr,0,sink)); // step 1: old note still held
  CHECK(on==1&&off==0);
  CHECK(live.process(block,nullptr,0,sink)); // step 2: original NoteOff
  CHECK(on==1&&off==1);
  CHECK(live.process(block,nullptr,0,sink)); // step 3: edited note
  CHECK(on==2&&off==1);
  const ebm::live::Trigger stop{0,48,false};
  CHECK(live.process(block,&stop,1,sink));
  CHECK(on==off);
 }
 std::cout<<"editable MIDI pattern contract PASS\n";
}
