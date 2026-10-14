#include "../src/vst3/UiGeometry.hpp"
#include "ebm/HostBridge.hpp"
#include "ebm/MidiWriter.hpp"
#include <cstdlib>
#include <iostream>
#include <array>
using namespace ebm;
#define CHECK(x) do { if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;} }while(false)
int main() {
 using namespace ebm::vst3::ui;
 constexpr std::array<Target,12> targets{
  Target::Zoom,Target::Style,Target::Bars,Target::Root,Target::Variation,Target::Trigger,
  Target::PrevBar,Target::NextBar,Target::Generate,Target::Variate,Target::ExportLoop,Target::ExportOneShot};
 constexpr std::array<int,12> xs{1025,145,348,510,730,980,49,180,151,373,620,926};
 constexpr std::array<int,12> ys{40,131,131,131,131,131,228,228,520,520,520,520};
 for(std::size_t i=0;i<targets.size();++i)CHECK(hit(xs[i],ys[i]).target==targets[i]);
 for(int x: {-3,0,1099,1100})for(int y:{-5,0,589,590})
  if(x<0||x>=width||y<0||y>=height)CHECK(hit(x,y).target==Target::None);
 CHECK(variationValue(-999)==0);
 CHECK(variationValue(618)==0);
 CHECK(variationValue(738)==50);
 CHECK(variationValue(858)==100);
 CHECK(variationValue(9999)==100);
 for(int x=618;x<858;++x)CHECK(variationValue(x)<=variationValue(x+1));
 CHECK(defaultValue(Target::Variation)==30);
 CHECK(defaultValue(Target::Bars)==8);
 CHECK(defaultValue(Target::Root)==36);
 v2::Session session;
 auto info=session.generate();(void)info;
 const auto first=session.snapshot();
 CHECK(first.hasCurrent);
 Config baseline=first.current;baseline.variation=0;
 const auto original=v2::render(v2::compose(baseline)).notes;
 for(int v:{0,15,30,55,85,100}){
  (void)session.variate(v);
  CHECK(session.snapshot().current.seed==first.current.seed);
  if(v==0)CHECK(v2::render(session.current()).notes==original);
  const auto bytes=toMidi(v2::render(session.current()));
  CHECK(bytes.size()>32);
 }
 session.setRootMidi(43);CHECK(session.snapshot().current.seed==first.current.seed);
 auto saved=session.snapshot();
 v2::Session replay;replay.restore(saved);
 CHECK(v2::render(replay.current()).notes==v2::render(session.current()).notes);
 // Style and length changes must not silently leave the old source motif active.
 session.setStyle(Style::MinimalBody);CHECK(!session.hasCurrent());
 (void)session.generate();CHECK(session.hasCurrent());
 session.setBars(4);CHECK(!session.hasCurrent());
 (void)session.generate();CHECK(session.current().settings.bars==4);
 std::cout<<"PASS: 12 unique GUI hit targets, absolute variation endpoints, control semantics, seed/state and MIDI coherence\n";
}
