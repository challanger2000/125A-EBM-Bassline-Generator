#include "EbmPanel.hpp"
#include "Plugin.hpp"
#include "UiGeometry.hpp"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cbuttonstate.h"
#include <algorithm>
#include <string>
#include <array>
namespace ebm::vst3 {
namespace {
using namespace VSTGUI;
constexpr CColor backdrop{13,20,27,255}, card{23,34,44,255},well{17,27,35,255},
 edge{70,92,105,255}, foreground{230,236,240,255}, faint{150,169,182,255},
 accent{168,61,66,255},hot{224,133,134,255}, quiet{40,64,79,255};
CRect box(int x,int y,int w,int h){return {static_cast<CCoord>(x),static_cast<CCoord>(y),static_cast<CCoord>(x+w),static_cast<CCoord>(y+h)};}
void fill(CDrawContext* dc,int x,int y,int w,int h,CColor c,CColor border=edge){
 dc->setFillColor(c);dc->setFrameColor(border);dc->setLineWidth(1.);
 dc->drawRect(box(x,y,w,h),kDrawFilledAndStroked);
}
void label(CDrawContext* dc,std::string s,int x,int y,int w,int h,CColor c,bool centered=false,bool big=false){
 dc->setFont(big?kNormalFontVeryBig:kNormalFontSmall);dc->setFontColor(c);
 dc->drawString(s.c_str(),box(x,y,w,h),centered?kCenterText:kLeftText);
}
std::string pitch(int note){
 constexpr const char* name[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
 if(note<0||note>127)return "--";
 return std::string(name[note%12])+std::to_string(note/12-1);
}
}
EbmPanel::EbmPanel(const VSTGUI::CRect& bounds,Controller* owner):VSTGUI::CView(bounds),owner_(owner){setTransparency(false);}
VSTGUI::CMouseEventResult EbmPanel::onMouseDown(VSTGUI::CPoint& p,const VSTGUI::CButtonState& buttons){
 if(!owner_||!(buttons&VSTGUI::kLButton))return VSTGUI::kMouseEventNotHandled;
 const int x=static_cast<int>(p.x-getViewSize().left);
 const int y=static_cast<int>(p.y-getViewSize().top);
 if(ui::hit(x,y).target==ui::Target::Variation && !(buttons&VSTGUI::kControl)){
  dragging_=true;owner_->setVariationFromPixel(x);
  return VSTGUI::kMouseEventHandled;
 }
 owner_->click(x,y,(buttons&VSTGUI::kControl)!=0);
 return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}
VSTGUI::CMouseEventResult EbmPanel::onMouseMoved(VSTGUI::CPoint& p,const VSTGUI::CButtonState&){
 if(!dragging_||!owner_)return VSTGUI::kMouseEventNotHandled;
 owner_->setVariationFromPixel(static_cast<int>(p.x-getViewSize().left));
 return VSTGUI::kMouseEventHandled;
}
VSTGUI::CMouseEventResult EbmPanel::onMouseUp(VSTGUI::CPoint&,const VSTGUI::CButtonState&){
 if(!dragging_)return VSTGUI::kMouseEventNotHandled;
 dragging_=false;return VSTGUI::kMouseEventHandled;
}
VSTGUI::CMouseEventResult EbmPanel::onMouseCancel(){dragging_=false;return VSTGUI::kMouseEventHandled;}
void EbmPanel::draw(VSTGUI::CDrawContext* dc){
 using namespace VSTGUI;
 if(!owner_)return;
 const auto& st=owner_->session();
 const auto& cfg=st.options();
 const int current=owner_->bar();
 const int variation=cfg.variation;
 dc->setDrawMode(kAntiAliasing);
 fill(dc,0,0,ui::width,ui::height,backdrop,backdrop);
 fill(dc,10,10,1080,570,card,edge);
 fill(dc,20,18,1060,63,well);
 // The authoritative 125A vector is drawn in the separate noninteractive logo view.
 label(dc,"EBM BASSLINE GENERATOR",220,25,635,35,foreground,true,true);
 label(dc,"V0.2  /  MIDI COMPOSITION",221,58,635,17,faint,true);
 fill(dc,974,25,102,35,accent,hot);
 label(dc,owner_->zoom()>1.25?"ZOOM 150%":"ZOOM 100%",975,32,100,20,foreground,true);
 fill(dc,20,93,1060,82,well,edge);
 struct Field{int x,w;const char* title;std::string value;};
 const std::array<Field,5> fields{{
   {24,242,"STYLE  [v]",std::string(styleName(cfg.style))},
   {278,143,"PATTERN  [v]",std::to_string(cfg.bars)+" BARS"},
   {433,157,"ROOT NOTE  [v]",pitch(cfg.rootMidi)},
   {602,275,"VARIATION",std::to_string(variation)+" %"},
   {889,187,"TRIGGER",owner_->mode()==live::Mode::Transport?"TRANSPORT":"MIDI NOTE"}
 }};
 for(const auto& f:fields){
  fill(dc,f.x,102,f.w,65,backdrop);
  label(dc,f.title,f.x+12,108,f.w-24,17,faint);
  label(dc,f.value,f.x+12,125,f.w-24,27,foreground);
 }
 fill(dc,618,154,240,7,quiet);
 fill(dc,618,154,variation*240/100+1,7,accent,hot);
 fill(dc,std::clamp(618+variation*240/100-3,618,852),151,10,13,hot,hot);
 fill(dc,20,187,1060,287,well);
 label(dc,"BAR",25,193,75,20,faint);
 fill(dc,27,212,44,38,card);label(dc,"<",27,221,44,22,foreground,true);
 label(dc,std::to_string(current+1)+" / "+std::to_string(cfg.bars),76,221,82,22,foreground,true);
 fill(dc,160,212,44,38,card);label(dc,">",160,221,44,22,foreground,true);
 for(int i=0;i<16;++i){
  label(dc,std::to_string(i+1),205+i*51,212,49,32,faint,true);
  const int x=206+i*51;
  for(int r=0;r<3;++r)fill(dc,x,249+r*70,49,64,quiet);
 }
 label(dc,"ATTACK",31,266,164,20,foreground);
 label(dc,"PITCH",31,337,164,20,foreground);
 label(dc,"VELOCITY",31,406,164,20,foreground);
 if(st.hasCurrent()){
  const auto phrase=v2::render(st.current());
  for(const auto& n:phrase.notes){
   if(n.step/16!=current)continue;
   const int column=n.step%16,x=206+column*51;
   fill(dc,x,249,49,64,accent,hot);
   label(dc,"ON",x,269,49,22,foreground,true);
   fill(dc,x,319,49,64,card,edge);
   label(dc,pitch(n.pitch),x,340,49,20,foreground,true);
   fill(dc,x,389,49,64,card,edge);
   label(dc,std::to_string(n.velocity),x,407,49,21,foreground,true);
  }
 }else label(dc,"PRESS GENERATE TO CREATE A BASSLINE",210,314,800,50,faint,true);
 fill(dc,20,483,1060,69,well);
 const struct {int x,w;const char* name;} commands[]{
  {24,254,"GENERATE"},{292,175,"VARIATE"},
  {481,276,"EXPORT LOOP MIDI"},{771,305,"EXPORT ONE-SHOT MIDI"}
 };
 for(const auto& b:commands){fill(dc,b.x,492,b.w,53,b.x==24?accent:card,b.x==24?hot:edge);
  label(dc,b.name,b.x,510,b.w,22,foreground,true);}
 label(dc,std::string("STATUS: ")+owner_->status(),28,560,1038,22,faint);
 setDirty(false);
}
}
