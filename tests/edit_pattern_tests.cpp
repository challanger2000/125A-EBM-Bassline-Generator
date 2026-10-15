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
  const ebm::live::Trigger trigger{0,60,true};
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
  ebm::live::Trigger release{0,60,false};
  CHECK(live.process(block,&release,1,sink));
  CHECK(ons==offs);
 }
 std::cout<<"editable MIDI pattern contract PASS\n";
}
