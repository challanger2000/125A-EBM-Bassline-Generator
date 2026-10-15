#include "../src/vst3/UiGeometry.hpp"
#include "ebm/HostBridge.hpp"
#include "ebm/MidiWriter.hpp"
#include <iostream>
#include <array>
using namespace ebm;
#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(){
 using namespace ebm::vst3::ui;
 constexpr std::array<Target,17> targets{
  Target::Zoom,Target::Style,Target::Bars,Target::Root,Target::Scale,
  Target::Variation,Target::SongStart,Target::Trigger,Target::PrevBar,Target::NextBar,
  Target::Bar,Target::Generate,Target::Variate,Target::Clear,Target::Copy,Target::Paste,Target::Export};
 constexpr std::array<int,17> xs{1025,45,225,345,500,700,835,975,49,180,560,151,273,385,480,598,820};
 constexpr std::array<int,17> ys{40,131,131,131,131,131,131,131,228,228,223,520,520,520,520,520,520};
 for(std::size_t i=0;i<targets.size();++i)CHECK(hit(xs[i],ys[i]).target==targets[i]);
 for(int row=0;row<4;++row)for(int col=0;col<16;++col){
  const auto h=hit(gridX+col*cellW+25,gridY+row*rowH+20);
  CHECK(h.column==col&&h.row==row);
  CHECK(h.target==(row==0?Target::NoteOn:row==1?Target::Pitch:row==2?Target::Velocity:Target::Length));
 }
 CHECK(variationValue(-100)==0);
 CHECK(variationValue(628)==0);
 CHECK(variationValue(769)==100);
 CHECK(variationValue(10000)==100);
 for(int x=628;x<769;++x)CHECK(variationValue(x)<=variationValue(x+1));
 CHECK(defaultValue(Target::SongStart)==2);
 CHECK(defaultValue(Target::Variation)==30);
 CHECK(defaultValue(Target::Bars)==8);
 CHECK(defaultValue(Target::Root)==36);
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
