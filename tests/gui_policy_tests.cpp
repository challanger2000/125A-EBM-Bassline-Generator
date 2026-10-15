#include "../src/vst3/UiGeometry.hpp"
#include "ebm/HostBridge.hpp"
#include "ebm/MidiWriter.hpp"
#include "ebm/ScaleNoteDisplay.hpp"
#include <iostream>
#include <array>
#include <string>
using namespace ebm;
#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(){
 using namespace ebm::vst3::ui;
 constexpr std::array<Target,18> targets{
  Target::Zoom,Target::Style,Target::Bars,Target::Root,Target::Scale,
  Target::Variation,Target::SongStart,Target::Trigger,Target::PrevBar,Target::NextBar,
  Target::Bar,Target::Generate,Target::Variate,Target::Clear,Target::Copy,Target::Paste,Target::Gate,Target::Export};
 constexpr std::array<int,18> xs{1025,45,225,345,500,700,835,975,49,180,560,151,273,385,480,598,750,970};
 constexpr std::array<int,18> ys{40,131,131,131,131,131,131,131,228,228,200,520,520,520,520,520,520,520};
 for(std::size_t i=0;i<targets.size();++i)CHECK(hit(xs[i],ys[i]).target==targets[i]);
 for(const Target t:{Target::Generate,Target::Variate,Target::Clear,
                     Target::Copy,Target::Paste,Target::Export})CHECK(isCommand(t));
 CHECK(!isCommand(Target::Root));
 for(int row=0;row<4;++row)for(int col=0;col<16;++col){
  const auto h=hit(gridX+col*cellW+25,gridY+row*rowH+20);
  CHECK(h.column==col&&h.row==row);
  CHECK(h.target==(row==0?Target::NoteOn:row==1?Target::Pitch:row==2?Target::Velocity:Target::Length));
 }
 // The read-only guide stays strictly between the BAR arrows and BAR tabs,
 // above the 16 STEP numbers; it must not intercept any mouse action.
 static_assert(scaleGuideX>=204 && scaleGuideNotesX>=scaleGuideX+scaleGuideLabelW);
 static_assert(scaleGuideNotesX+scaleGuideNotesW<550);
 static_assert(scaleGuideY+scaleGuideH<=222);
 static_assert(hit(scaleGuideNotesX+50,scaleGuideY+10).target==Target::None);
 // Exact pitch-class set comes from inScale(), not a separate mode table.
 const auto names=[](int key,edit::Scale scale){
  const auto notes=edit::triggerScaleNotes(key,scale);
  std::string rendered;
  for(int j=0;j<notes.count;++j){if(j)rendered+=' ';rendered+=notes.names[static_cast<std::size_t>(j)];}
  return rendered;
 };
 CHECK(names(48,edit::Scale::Major)=="C D E F G A B");
 CHECK(names(48,edit::Scale::NaturalMinor)=="C D Eb F G Ab Bb");
 CHECK(names(57,edit::Scale::NaturalMinor)=="A B C D E F G");
 CHECK(names(52,edit::Scale::NaturalMinor)=="E F# G A B C D");
 CHECK(names(48,edit::Scale::MinorPentatonic)=="C Eb F G Bb");
 CHECK(names(48,edit::Scale::Chromatic)=="C C# D D# E F F# G G# A A# B");
 for(int root=24;root<=60;++root)for(int mode=0;mode<edit::kScaleCount;++mode){
  const auto scale=static_cast<edit::Scale>(mode);
  const auto notes=edit::triggerScaleNotes(root,scale);
  int expected=0;
  for(int semitone=0;semitone<12;++semitone){
   const int pitchClass=(root%12+semitone)%12;
   if(edit::inScale(60+pitchClass,root,scale))++expected;
  }
  CHECK(notes.count==expected);
  CHECK(notes.count==(mode==0?12:mode==9?5:7));
  for(int n=0;n<notes.count;++n)CHECK(notes.names[static_cast<std::size_t>(n)]!=nullptr);
 }
 // Separate BAR navigation from the 16 STEP-number header at y>=222.
 for(int bar=0;bar<8;++bar){
  CHECK(hit(550+bar*62+25,200).target==Target::Bar);
  CHECK(hit(550+bar*62+25,200).column==bar);
 }
 for(int col=0;col<16;++col)CHECK(hit(gridX+col*cellW+20,235).target!=Target::Bar);
 CHECK(dragValue(Target::Velocity,80,40)==100);
 CHECK(dragValue(Target::Velocity,80,-500)==1);
 CHECK(dragValue(Target::Velocity,80,500)==127);
 CHECK(dragValue(Target::Length,1,32,16)==9);
 CHECK(dragValue(Target::Length,9,-500,16)==1);
 CHECK(dragValue(Target::Length,9,500,16)==16);
 CHECK(variationValue(-100)==0);
 CHECK(variationValue(628)==0);
 CHECK(variationValue(769)==100);
 CHECK(variationValue(10000)==100);
 for(int x=628;x<769;++x)CHECK(variationValue(x)<=variationValue(x+1));
 CHECK(defaultValue(Target::SongStart)==2);
 CHECK(defaultValue(Target::Variation)==30);
 CHECK(defaultValue(Target::Bars)==8);
 CHECK(defaultValue(Target::Root)==48);
 for(int x:{-1,1100})CHECK(hit(x,200).target==Target::None);
 v2::Session s;(void)s.generate();const auto snap=s.snapshot();
 CHECK(snap.hasCurrent);
 auto pat=edit::fromPhrase(v2::render(s.current()),1);
 auto exported=toMidi(edit::toPhrase(pat));CHECK(exported.size()>32);
 CHECK(exported[0]=='M'&&exported[1]=='T');
 CHECK(edit::toPhrase(pat).bars==1);
 auto state=s.snapshot();v2::Session replay;replay.restore(state);
 CHECK(v2::render(s.current()).notes==v2::render(replay.current()).notes);
 std::cout<<"GUI controls + full 16x4 hit mapping PASS\n";
}
