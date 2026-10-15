#include "EbmPanel.hpp"
#include "Plugin.hpp"
#include "UiGeometry.hpp"
#include "ebm/ScaleNoteDisplay.hpp"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cbuttonstate.h"
#include <algorithm>
#include <array>
#include <string>
namespace ebm::vst3 {
namespace {
using namespace VSTGUI;
constexpr CColor backdrop{13,20,27,255},card{23,34,44,255},well{17,27,35,255},
 edge{70,92,105,255},foreground{230,236,240,255},faint{160,178,190,255},
 accent{168,61,66,255},hot{224,133,134,255},quiet{40,64,79,255};
CRect box(int x,int y,int w,int h){return {static_cast<CCoord>(x),static_cast<CCoord>(y),static_cast<CCoord>(x+w),static_cast<CCoord>(y+h)};}
void fill(CDrawContext* dc,int x,int y,int w,int h,CColor c,CColor border=edge){
 dc->setFillColor(c);dc->setFrameColor(border);dc->setLineWidth(1.);
 dc->drawRect(box(x,y,w,h),kDrawFilledAndStroked);
}
void label(CDrawContext* dc,const std::string& s,int x,int y,int w,int h,CColor c,bool centered=false,bool big=false){
 dc->setFont(big?kNormalFontVeryBig:kNormalFontSmall);dc->setFontColor(c);
 dc->drawString(s.c_str(),box(x,y,w,h),centered?kCenterText:kLeftText);
}
std::string pitch(int note){
 constexpr const char* name[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
 if(note<0||note>127)return "--";
 return std::string(name[note%12])+std::to_string(edit::studioOneOctave(note));
}

}
EbmPanel::EbmPanel(const VSTGUI::CRect& bounds,Controller* owner):VSTGUI::CView(bounds),owner_(owner){setTransparency(false);}
VSTGUI::CMouseEventResult EbmPanel::onMouseDown(VSTGUI::CPoint& p,const VSTGUI::CButtonState& buttons){
 if(!owner_||!(buttons&VSTGUI::kLButton))return VSTGUI::kMouseEventNotHandled;
 const int x=static_cast<int>(p.x-getViewSize().left),y=static_cast<int>(p.y-getViewSize().top);
 const auto hit=ui::hit(x,y);
 pressed_=hit.target;pressedColumn_=hit.column;
 if(ui::isCommand(hit.target))lastAction_=hit.target;
 invalid();
 if(hit.target==ui::Target::Gate&&!(buttons&(VSTGUI::kControl|VSTGUI::kDoubleClick))){
  draggingGate_=true;owner_->beginGateGesture();owner_->setGateFromPixel(x);return VSTGUI::kMouseEventHandled;
 }
 if(hit.target==ui::Target::Variation&&!(buttons&(VSTGUI::kControl|VSTGUI::kDoubleClick))){
  dragging_=true;owner_->setVariationFromPixel(x);return VSTGUI::kMouseEventHandled;
 }
 if((hit.target==ui::Target::Pitch||hit.target==ui::Target::Velocity||hit.target==ui::Target::Length)&&
    !(buttons&VSTGUI::kControl)&&!(buttons&VSTGUI::kDoubleClick)){
  const int ix=owner_->bar()*16+hit.column;
  const auto& note=owner_->pattern().steps[static_cast<std::size_t>(ix)];
  if(note.on&&!note.locked){
   draggingNumeric_=true;dragIndex_=ix;dragRow_=hit.row;dragY_=y;
   dragBase_=hit.target==ui::Target::Pitch?note.pitch:(hit.target==ui::Target::Velocity?note.velocity:edit::effectiveLength(owner_->pattern(),ix));
   return VSTGUI::kMouseEventHandled;
  }
 }
 owner_->click(x,y,(buttons&VSTGUI::kControl)!=0,(buttons&VSTGUI::kShift)!=0,
               (buttons&VSTGUI::kDoubleClick)!=0);
 // Capture the matching mouse-up, so every button has an actual pressed state.
 return VSTGUI::kMouseEventHandled;
}
VSTGUI::CMouseEventResult EbmPanel::onMouseMoved(VSTGUI::CPoint& p,const VSTGUI::CButtonState& buttons){
 if(!owner_)return VSTGUI::kMouseEventNotHandled;
 const int x=static_cast<int>(p.x-getViewSize().left),y=static_cast<int>(p.y-getViewSize().top);
 if(draggingGate_){owner_->setGateFromPixel(x);return VSTGUI::kMouseEventHandled;}
 if(dragging_){owner_->setVariationFromPixel(x);return VSTGUI::kMouseEventHandled;}
 if(draggingNumeric_){
  if(!(buttons&VSTGUI::kLButton)){
   draggingNumeric_=false;pressed_=ui::Target::None;pressedColumn_=-1;invalid();return VSTGUI::kMouseEventHandled;
  }
  owner_->dragNumeric(dragIndex_,dragRow_,dragBase_,dragY_-y);
  return VSTGUI::kMouseEventHandled;
 }
 if(pressed_!=ui::Target::None)return VSTGUI::kMouseEventHandled;
 owner_->hover(x,y);return VSTGUI::kMouseEventNotHandled;
}
VSTGUI::CMouseEventResult EbmPanel::onMouseUp(VSTGUI::CPoint&,const VSTGUI::CButtonState&){
 const bool handled=pressed_!=ui::Target::None||dragging_||draggingNumeric_||draggingGate_;
 if(draggingGate_&&owner_)owner_->endGateGesture();
 draggingGate_=false;dragging_=false;draggingNumeric_=false;pressed_=ui::Target::None;pressedColumn_=-1;invalid();
 return handled?VSTGUI::kMouseEventHandled:VSTGUI::kMouseEventNotHandled;
}
VSTGUI::CMouseEventResult EbmPanel::onMouseCancel(){
 if(draggingGate_&&owner_)owner_->endGateGesture();
 draggingGate_=false;dragging_=false;draggingNumeric_=false;pressed_=ui::Target::None;pressedColumn_=-1;invalid();
 return VSTGUI::kMouseEventHandled;
}
void EbmPanel::onMouseWheelEvent(VSTGUI::MouseWheelEvent& event){
 if(!owner_)return;
 const auto delta=event.deltaY!=0.?event.deltaY:event.deltaX;
 if(delta==0.)return;
 const int x=static_cast<int>(event.mousePosition.x-getViewSize().left);
 const int y=static_cast<int>(event.mousePosition.y-getViewSize().top);
 if(owner_->wheel(x,y,delta>0.?1:-1))event.consumed=true;
}
void EbmPanel::draw(VSTGUI::CDrawContext* dc){
 using namespace VSTGUI;
 if(!owner_)return;
 const auto& s=owner_->session();const auto& cfg=s.options();
 const auto& p=owner_->pattern();const int current=owner_->bar();
 dc->setDrawMode(kAntiAliasing);
 fill(dc,0,0,ui::width,ui::height,backdrop,backdrop);
 fill(dc,10,10,1080,570,card);
 fill(dc,20,18,1060,63,well);
 label(dc,"EBM BASSLINE GENERATOR",230,25,620,35,foreground,true,true);
 label(dc,"V0.2  /  EDITABLE MIDI BASSLINE",230,58,620,17,faint,true);
 fill(dc,974,25,102,35,pressed_==ui::Target::Zoom?hot:accent,hot);
 label(dc,owner_->zoom()>1.25?"ZOOM 150%":"ZOOM 100%",975,32,100,20,foreground,true);
 fill(dc,20,93,1060,82,well);
 struct Field{int x,w;const char* title;std::string value;};
 const std::array<Field,7> fields{{
  {24,172,"STYLE  v",styleName(cfg.style)},
  {206,108,"BARS  v",std::to_string(p.bars)},
  {324,135,"KEY / ROOT  v",pitch(cfg.rootMidi)},
  {469,136,"SCALE  v",edit::scaleName(p.scale)},
  {615,166,"VARIATION",std::to_string(cfg.variation)+" %"},
  {791,123,"SONG START  v",std::to_string(p.songStartBar)},
  {924,152,"TRIGGER",owner_->mode()==live::Mode::Transport?"TRANSPORT":"MIDI NOTE"}
 }};
 for(const auto& field:fields){
  const auto target=ui::hit(field.x+field.w/2,134).target;
  fill(dc,field.x,102,field.w,65,pressed_==target?accent:backdrop,
       pressed_==target?hot:edge);
  label(dc,field.title,field.x+9,109,field.w-16,17,faint);
  label(dc,field.value,field.x+9,131,field.w-16,25,foreground);
 }
 fill(dc,628,155,141,7,quiet);
 fill(dc,628,155,1+cfg.variation*141/100,7,accent,hot);
 fill(dc,std::clamp(628+cfg.variation*141/100-4,628,760),152,9,13,hot,hot);
 fill(dc,20,187,1060,288,well);
 label(dc,"BAR",25,191,75,17,faint);
 fill(dc,27,207,44,37,pressed_==ui::Target::PrevBar?accent:card);label(dc,"<",27,217,44,20,foreground,true);
 label(dc,std::to_string(current+1)+" / "+std::to_string(p.bars),76,217,82,20,foreground,true);
 fill(dc,160,207,44,37,pressed_==ui::Target::NextBar?accent:card);label(dc,">",160,217,44,20,foreground,true);
 // Show exactly the pitch classes accepted by the current ROOT/SCALE.
 // It updates with redraw() after either selector is changed, without
 // introducing a new control or altering MIDI output or project state.
 label(dc,"TRIGGER NOTES",ui::scaleGuideX,ui::scaleGuideY,
       ui::scaleGuideLabelW,ui::scaleGuideH,faint);
 const auto allowed=edit::triggerScaleNotes(cfg.rootMidi,p.scale);
 if(allowed.count>0){
  const int cell=ui::scaleGuideNotesW/allowed.count;
  for(int n=0;n<allowed.count;++n){
   const int x=ui::scaleGuideNotesX+n*cell;
   label(dc,allowed.names[static_cast<std::size_t>(n)],x,ui::scaleGuideY,
         cell,ui::scaleGuideH,n==0?hot:foreground,true);
  }
 }
 for(int b=0;b<p.bars;++b){
  fill(dc,550+b*62,190,58,24,
       (pressed_==ui::Target::Bar&&pressedColumn_==b)?hot:(b==current?accent:card),b==current?hot:edge);
  label(dc,std::to_string(b+1),550+b*62,193,58,18,foreground,true);
 }
 const char* rowNames[]{"NOTE ON","PITCH","VELOCITY","LENGTH"};
 for(int r=0;r<4;++r){
  label(dc,rowNames[r],31,261+r*ui::rowH,160,20,foreground);
  for(int c=0;c<16;++c){
   const int x=ui::gridX+c*ui::cellW,y=ui::gridY+r*ui::rowH;
   const auto& n=p.steps[static_cast<std::size_t>(current*16+c)];
   fill(dc,x,y,49,48,n.on?(r==0?accent:card):quiet,n.locked?hot:edge);
   if(n.on){
    std::string value;
    if(r==0)value=n.locked?"LOCK":"ON";
    if(r==1)value=pitch(n.pitch);
    if(r==2)value=std::to_string(n.velocity);
    if(r==3){
      value=edit::lengthText(edit::effectiveLength(p,current*16+c));
     }
    label(dc,value,x,y+12,49,24,foreground,true);
   }else if(r==0&&n.locked)label(dc,"LOCK",x,y+12,49,24,hot,true);
  }
 }
 for(int c=0;c<16;++c){
  label(dc,std::to_string(c+1),ui::gridX+c*ui::cellW,222,49,20,faint,true);
  if(owner_->playhead()==c)
   fill(dc,ui::gridX+c*ui::cellW,244,49,5,hot,hot);
 }
 fill(dc,20,483,1060,69,well);
 struct Command{int x,w;const char* title;};
 const Command actions[]={{24,180,"GENERATE"},{210,135,"VARIATE"},
  {351,90,"CLEAR"},{447,110,"COPY BAR"},{563,110,"PASTE BAR"},{856,220,"EXPORT MIDI"}};
 for(const auto& action:actions){
  const auto hit=ui::hit(action.x+action.w/2,517).target;
  const bool down=pressed_==hit, recentlyClicked=lastAction_==hit;
  fill(dc,action.x,492,action.w,53,down?hot:(recentlyClicked?accent:(action.x==24?accent:card)),
       down?foreground:(recentlyClicked?hot:(action.x==24?hot:edge)));
  label(dc,action.title,action.x,509,action.w,21,foreground,true);
  // Persistent confirmation marker makes fast clicks visible even when the
  // host coalesces mouse-down/up repaints in one frame.
  if(recentlyClicked&&!down)fill(dc,action.x+action.w-15,498,8,8,hot,hot);
 }
 fill(dc,679,492,171,53,pressed_==ui::Target::Gate?accent:card);
 label(dc,"GLOBAL GATE  "+std::to_string(owner_->gate())+"%",688,497,155,20,foreground);
 fill(dc,691,528,145,5,quiet);
 fill(dc,691,528,1+145*owner_->gate()/100,5,accent,hot);
 label(dc,std::string("HELP: ")+owner_->help(),27,555,710,17,faint);
 label(dc,std::string("STATUS: ")+owner_->status(),745,555,330,17,faint,true);
 setDirty(false);
}
} // namespace ebm::vst3
