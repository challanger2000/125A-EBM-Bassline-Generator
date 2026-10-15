#include "Plugin.hpp"
#include "EbmPanel.hpp"
#include "EbmBrand.hpp"
#include "UiGeometry.hpp"
#include "ebm/MidiWriter.hpp"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/base/ibstream.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ebm::vst3 {
namespace {
bool readDocument(IBStream* stream,host::Document& document) noexcept {
    if(!stream)return false;
    host::DocumentBytes bytes{};
    int32 count=0;
    if(stream->read(bytes.data(),8,&count)!=kResultOk || count!=8)return false;
    const bool old=bytes[3]=='1'&&bytes[4]==1;
    const auto size=old?host::kLegacyDocumentBytes:host::kDocumentBytes;
    if(stream->read(bytes.data()+8,static_cast<int32>(size-8),&count)!=kResultOk ||
       count!=static_cast<int32>(size-8))return false;
    return host::decode(bytes.data(),size,document);
}
bool writeDocument(IBStream* stream,const host::Document& document) noexcept {
    if(!stream)return false;
    host::DocumentBytes bytes{};
    if(!host::encode(document,bytes))return false;
    int32 count=0;
    return stream->write(bytes.data(),static_cast<int32>(bytes.size()),&count)==kResultOk &&
           count==static_cast<int32>(bytes.size());
}
}
Processor::Processor(){setControllerClass(ControllerUID);}
tresult PLUGIN_API Processor::initialize(FUnknown* context){
    const auto result=AudioEffect::initialize(context);
    if(result!=kResultOk)return result;
    addEventInput(STR16("Trigger In"),16,kMain,BusInfo::kDefaultActive);
    addEventOutput(STR16("Bass MIDI Out"),1,kMain,BusInfo::kDefaultActive);
    return kResultOk;
}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup){
    if(!std::isfinite(setup.sampleRate)||setup.sampleRate<=0.)return kInvalidArgument;
    sampleRate_=setup.sampleRate;
    return AudioEffect::setupProcessing(setup);
}
tresult PLUGIN_API Processor::setActive(TBool active){
    if(!active)cleanupPending_.store(true,std::memory_order_release);
    return AudioEffect::setActive(active);
}
tresult PLUGIN_API Processor::setProcessing(TBool active){
    if(!active)cleanupPending_.store(true,std::memory_order_release);
    return kResultOk;
}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 size){
    return (size==kSample32||size==kSample64)?kResultTrue:kResultFalse;
}
tresult PLUGIN_API Processor::getState(IBStream* stream){
    return writeDocument(stream,model_.snapshot())?kResultOk:kResultFalse;
}
tresult PLUGIN_API Processor::setState(IBStream* stream){
    host::Document doc;
    if(!readDocument(stream,doc)||!model_.restore(doc))return kResultFalse;
    cleanupPending_.store(true,std::memory_order_release);
    return kResultOk;
}
tresult PLUGIN_API Processor::process(ProcessData& data){
    if(data.numSamples<0)return kInvalidArgument;
    if(data.inputParameterChanges){
        auto* changes=data.inputParameterChanges;
        for(int32 i=0;i<changes->getParameterCount();++i){
            auto* q=changes->getParameterData(i);
            if(!q||q->getParameterId()!=TriggerModeId||q->getPointCount()==0)continue;
            int32 sample=0;ParamValue value=0.;
            if(q->getPoint(q->getPointCount()-1,sample,value)==kResultOk &&
               std::isfinite(value)&&value>=0.&&value<=1.)
                model_.setMode(value<0.5?live::Mode::Transport:live::Mode::MidiNote);
        }
    }
    live::Block block;
    block.samples=data.numSamples;
    block.sampleRate=sampleRate_;
    const auto* context=data.processContext;
    if(context){
        block.tempoValid=(context->state&ProcessContext::kTempoValid)!=0;
        block.ppqValid=(context->state&ProcessContext::kProjectTimeMusicValid)!=0;
        block.playing=(context->state&ProcessContext::kPlaying)!=0;
        if(block.tempoValid)block.tempo=context->tempo;
        if(block.ppqValid)block.projectPpq=context->projectTimeMusic-4.*(model_.songStartBar()-1);
    }
    live::Trigger triggers[live::Runtime::maxTriggers]{};
    int count=0;
    if(data.inputEvents){
        const int32 total=data.inputEvents->getEventCount();
        if(total>live::Runtime::maxTriggers)return kResultFalse;
        for(int32 i=0;i<total;++i){
            Steinberg::Vst::Event ev{};
            if(data.inputEvents->getEvent(i,ev)!=kResultOk)continue;
            if(ev.type!=Steinberg::Vst::Event::kNoteOnEvent &&
               ev.type!=Steinberg::Vst::Event::kNoteOffEvent)continue;
            const bool on=ev.type==Steinberg::Vst::Event::kNoteOnEvent && ev.noteOn.velocity>0.f;
            const int pitch=ev.type==Steinberg::Vst::Event::kNoteOnEvent?ev.noteOn.pitch:ev.noteOff.pitch;
            if(ev.sampleOffset<0||ev.sampleOffset>=data.numSamples||pitch<0||pitch>127)return kResultFalse;
            triggers[count++]={ev.sampleOffset,pitch,on};
        }
    }
    auto sink=[&](live::Output e) noexcept -> bool {
        if(!data.outputEvents || e.offset<0||e.offset>=data.numSamples)return false;
        Steinberg::Vst::Event event{};
        event.busIndex=0;
        event.sampleOffset=e.offset;
        if(e.on){
            event.type=Steinberg::Vst::Event::kNoteOnEvent;
            event.noteOn.channel=0;
            event.noteOn.pitch=static_cast<int16>(e.pitch);
            event.noteOn.velocity=static_cast<float>(e.velocity)/127.f;
            event.noteOn.tuning=0.f;
            event.noteOn.length=0;
            event.noteOn.noteId=-1;
        }else{
            event.type=Steinberg::Vst::Event::kNoteOffEvent;
            event.noteOff.channel=0;
            event.noteOff.pitch=static_cast<int16>(e.pitch);
            event.noteOff.velocity=0.f;
            event.noteOff.tuning=0.f;
            event.noteOff.noteId=-1;
        }
        return data.outputEvents->addEvent(event)==kResultOk;
    };
    if(cleanupPending_.exchange(false,std::memory_order_acq_rel)){
        const auto wanted=model_.mode();
        model_.setMode(live::Mode::Transport);
        auto stopped=block;
        stopped.playing=false;stopped.ppqValid=false;
        if(!model_.process(stopped,nullptr,0,sink)){
            model_.setMode(wanted);
            cleanupPending_.store(true,std::memory_order_release);
            return kResultFalse;
        }
        model_.setMode(wanted);
    }
    if(!model_.process(block,triggers,count,sink))return kResultFalse;
    int step=16; // 16 is "not playing" for this 0..16 read-only parameter.
    if(model_.mode()==live::Mode::Transport&&block.playing&&block.ppqValid&&block.tempoValid &&
       std::isfinite(block.projectPpq)&&block.projectPpq>=0.){
        const auto tick=static_cast<std::int64_t>(std::floor(block.projectPpq*4.));
        step=static_cast<int>(tick%16);
    }else if(model_.mode()==live::Mode::MidiNote){
        const auto current=model_.playheadStep();
        if(current>=0)step=current;
    }
    if(step!=reportedStep_&&data.outputParameterChanges){
        int32 queueIndex=0;
        auto* q=data.outputParameterChanges->addParameterData(PlayheadStepId,queueIndex);
        if(q){int32 point=0;
          if(q->addPoint(0,static_cast<double>(step)/16.,point)==kResultOk)
            reportedStep_=step;
        }
    }
    return kResultOk;
}

tresult PLUGIN_API Processor::notify(IMessage* msg){
 if(!msg || !msg->getMessageID() || std::strcmp(msg->getMessageID(),"125A_EBM_FULL_DOCUMENT_V1")!=0)
  return AudioEffect::notify(msg);
 const void* data=nullptr;
 uint32 size=0;
 if(!msg->getAttributes() ||
    msg->getAttributes()->getBinary("State",data,size)!=kResultOk)
  return kResultFalse;
 host::Document restored;
 if(!host::decode(static_cast<const std::uint8_t*>(data),size,restored) ||
    !model_.restore(restored)) return kResultFalse;
 return kResultOk;
}

tresult PLUGIN_API Controller::initialize(FUnknown* context){
 const auto result=EditController::initialize(context);
 if(result!=kResultOk)return result;
 auto* param=new RangeParameter(STR16("Playback Trigger"),TriggerModeId,
                                  STR16(""),0.,1.,0.,1,ParameterInfo::kCanAutomate);
 param->setPrecision(0);parameters.addParameter(param);
 (void)session_.generate();
 editable_=edit::fromPhrase(v2::render(session_.current()),8,edit::Scale::Chromatic,2);
 return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream){
 host::Document doc;
 if(!readDocument(stream,doc))return kResultFalse;
 try{
  v2::Session next;next.restore(doc.session);
  auto pat=doc.editable.enabled?doc.editable:
            edit::fromPhrase(next.hasCurrent()?v2::render(next.current()):Phrase{next.options().bars,{}},
                             next.options().bars,edit::Scale::Chromatic,2);
  if(!edit::valid(pat))return kResultFalse;
  session_=std::move(next);editable_=pat;mode_=doc.mode;
  bar_=std::clamp(bar_,0,editable_.bars-1);
  EditController::setParamNormalized(TriggerModeId,mode_==live::Mode::Transport?0.:1.);
  redraw();return kResultOk;
 }catch(...){return kResultFalse;}
}
tresult PLUGIN_API Controller::getState(IBStream* stream){
 if(!stream)return kInvalidArgument;
 const std::uint32_t bytes[3]{0x554D4245u,zoom_>1.25?150u:100u,static_cast<std::uint32_t>(bar_)};
 int32 written=0;
 return stream->write(const_cast<std::uint32_t*>(bytes),sizeof(bytes),&written)==kResultOk &&
        written==sizeof(bytes)?kResultOk:kResultFalse;
}
tresult PLUGIN_API Controller::setState(IBStream* stream){
 if(!stream)return kInvalidArgument;
 std::uint32_t bytes[3]{};int32 count=0;
 if(stream->read(bytes,sizeof(bytes),&count)!=kResultOk || count!=sizeof(bytes) ||
    bytes[0]!=0x554D4245u || (bytes[1]!=100&&bytes[1]!=150) || bytes[2]>=8)return kResultFalse;
 zoom_=bytes[1]/100.;bar_=std::clamp(static_cast<int>(bytes[2]),0,editable_.bars-1);
 if(editor_)editor_->setZoomFactor(zoom_);
 redraw();return kResultOk;
}
tresult PLUGIN_API Controller::setParamNormalized(ParamID id,ParamValue v){
 const auto result=EditController::setParamNormalized(id,v);
 if(result==kResultOk&&id==PlayheadStepId&&std::isfinite(v)){
  playhead_=std::clamp(static_cast<int>(std::floor(v*16.+0.5)),0,16);redraw();
 }
 if(result==kResultOk&&id==TriggerModeId&&std::isfinite(v)){
  mode_=v<0.5?live::Mode::Transport:live::Mode::MidiNote;
  redraw();
 }
 return result;
}
IPlugView* PLUGIN_API Controller::createView(FIDString type){
 if(!type||std::strcmp(type,ViewType::kEditor)!=0)return nullptr;
 auto* view=new VSTGUI::VST3Editor(this,"EBMView","ebm.uidesc");
 view->setAllowedZoomFactors(std::vector<double>{1.0,1.5});
 view->setEditorSizeConstrains(VSTGUI::CPoint(ui::width,ui::height),VSTGUI::CPoint(ui::width,ui::height));
 editor_=view;editor_->setZoomFactor(zoom_);return view;
}
VSTGUI::CView* Controller::createCustomView(VSTGUI::UTF8StringPtr name,
   const VSTGUI::UIAttributes& attr,const VSTGUI::IUIDescription*,VSTGUI::VST3Editor*){
 if(!name)return nullptr;
 VSTGUI::CPoint origin{0.,0.},size{1100.,590.};
 attr.getPointAttribute("origin",origin);attr.getPointAttribute("size",size);
 if(std::strcmp(name,"EbmPanel")==0){panel_=new EbmPanel(VSTGUI::CRect(origin,size),this);return panel_;}
 if(std::strcmp(name,"EbmBrand")==0)return new EbmBrand(VSTGUI::CRect(origin,size));
 return nullptr;
}
void Controller::willClose(VSTGUI::VST3Editor* closed){
 if(editor_==closed){editor_=nullptr;panel_=nullptr;textInput_=nullptr;editIndex_=-1;editRow_=-1;}
}
void Controller::redraw(){if(panel_)panel_->invalid();}
bool Controller::deliver(const v2::Session& next,live::Mode mode,const edit::Pattern& pattern){
 host::Document doc{next.snapshot(),mode,pattern};
 host::DocumentBytes bytes{};
 if(!host::encode(doc,bytes))return false;
 auto* message=allocateMessage();if(!message)return false;
 message->setMessageID("125A_EBM_FULL_DOCUMENT_V1");
 const auto attributes=message->getAttributes();
 if(!attributes || attributes->setBinary("State",bytes.data(),static_cast<uint32>(bytes.size()))!=kResultOk){
  message->release();return false;
 }
 const bool sent=sendMessage(message)==kResultOk;message->release();return sent;
}
bool Controller::transact(v2::Session next,live::Mode mode,edit::Pattern pattern){
 if(!edit::valid(pattern)||!deliver(next,mode,pattern)){
  status_="HOST UPDATE FAILED";redraw();return false;
 }
 session_=std::move(next);mode_=mode;editable_=std::move(pattern);
 bar_=std::clamp(bar_,0,editable_.bars-1);
 status_="PATTERN SENT TO HOST";redraw();return true;
}
bool Controller::commitEdit(edit::Pattern pattern){return transact(session_,mode_,std::move(pattern));}
void Controller::setVariationFromPixel(int x){
 const int value=ui::variationValue(x);
 if(value==session_.options().variation)return;
 try{
  auto next=session_;
  if(next.hasCurrent())(void)next.variate(value);
  else {auto st=next.snapshot();st.options.variation=value;next.restore(st);}
  // VARIATION sets the intensity for future VARIATE clicks. Changing the slider
  // must never silently replace user-entered notes or delete an unlocked edit.
  if(transact(std::move(next),mode_,editable_)){
   status_="VARIATION INTENSITY SET";redraw();
  }
 }catch(...){status_="INVALID VARIATION";redraw();}
}
void Controller::dragNumeric(int index,int row,int base,int upwardPixels){
 if(index<0||index>=editable_.bars*16||(row!=2&&row!=3))return;
 const auto& source=editable_.steps[static_cast<std::size_t>(index)];
 if(!source.on||source.locked)return;
 const ui::Target target=row==2?ui::Target::Velocity:ui::Target::Length;
 int maxValue=128;
 if(row==3){
  maxValue=editable_.bars*16-index;
  for(int next=index+1;next<editable_.bars*16;++next)
   if(editable_.steps[static_cast<std::size_t>(next)].on){maxValue=next-index;break;}
 }
 const int value=ui::dragValue(target,base,upwardPixels,maxValue);
 if(value==(row==2?source.velocity:source.length))return;
 auto p=editable_;
 const bool ok=edit::modify(p,index,source.pitch,row==2?value:source.velocity,
                                row==3?value:source.length);
 if(ok&&commitEdit(std::move(p))){
  status_=row==2?"VELOCITY DRAGGED":"LENGTH DRAGGED";redraw();
 }
}
void Controller::choice(int x,int y,int kind){
 if(!editor_||!editor_->getFrame()||!panel_)return;
 auto* menu=new VSTGUI::COptionMenu;
 const auto add=[&](const std::string& title,int value,bool selected){
  auto* item=new VSTGUI::CMenuItem(title.c_str(),value);
  if(selected)item->setChecked(true);
  menu->addEntry(item);
 };
 static constexpr const char* notes[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
 if(kind==0){
  for(int value=0;value<3;++value)add(styleName(static_cast<Style>(value)),value,static_cast<int>(session_.options().style)==value);
 }else if(kind==1){
  for(int b:{1,2,4,8})add(std::to_string(b)+" BARS",b,editable_.bars==b);
 }else if(kind==2){
  for(int midi=24;midi<=60;++midi)add(std::string(notes[midi%12])+std::to_string(edit::studioOneOctave(midi)),midi,midi==session_.options().rootMidi);
 }else if(kind==3){
  for(int v=0;v<edit::kScaleCount;++v){
   const auto scale=static_cast<edit::Scale>(v);
   add(edit::scaleName(scale),v,editable_.scale==scale);
  }
 }else if(kind==4){
  for(int b=1;b<=16;++b)add("DAW BAR "+std::to_string(b),b,b==editable_.songStartBar);
 }else if(kind==5){
  const int index=bar_*16+editIndex_;
  const auto root=session_.options().rootMidi;
  for(int midi=24;midi<=84;++midi){
   if(!edit::inScale(midi,root,editable_.scale))continue;
   add(std::string(notes[midi%12])+std::to_string(edit::studioOneOctave(midi)),midi,
       editable_.steps[static_cast<std::size_t>(index)].pitch==midi);
  }
 }
 VSTGUI::CPoint point{static_cast<VSTGUI::CCoord>(x),static_cast<VSTGUI::CCoord>(y)};
 panel_->localToFrame(point);
 if(!menu->popup(editor_->getFrame(),point,[this,kind](VSTGUI::COptionMenu* opened){
  int index=-1;auto* selected=opened->getLastItemMenu(index);
  if(selected&&index>=0){
   auto* item=selected->getEntry(index);
   if(item&&item->getTag()>=0){
    try{
     const int value=item->getTag();auto next=session_;auto pat=editable_;
     if(kind==0){
      next.setStyle(static_cast<Style>(value));(void)next.generate();
      auto fresh=edit::fromPhrase(v2::render(next.current()),pat.bars,pat.scale,pat.songStartBar);
      if(next.options().style==Style::ClassicEBM)
       (void)edit::generateClassicDrive(fresh,next.options().rootMidi,next.current().settings.seed);
      for(int i=0;i<edit::kMaxSteps;++i)if(pat.steps[static_cast<std::size_t>(i)].locked)
          fresh.steps[static_cast<std::size_t>(i)]=pat.steps[static_cast<std::size_t>(i)];
      edit::conformToScale(fresh,next.options().rootMidi);
      pat=std::move(fresh);
     }else if(kind==1){
      const int engineBars=value<=4?4:8;
      if(next.options().bars!=engineBars)next.setBars(engineBars);
      if(!next.hasCurrent())(void)next.generate();
      pat.bars=value;
     }else if(kind==2){
      const int transpose=value-next.options().rootMidi;
      next.setRootMidi(value);
      for(auto& st:pat.steps)if(st.on&&!st.locked)
        st.pitch=static_cast<std::uint8_t>(std::clamp(static_cast<int>(st.pitch)+transpose,0,127));
     }else if(kind==3){
      pat.scale=static_cast<edit::Scale>(value);
      edit::conformToScale(pat,next.options().rootMidi);
     }else if(kind==4)pat.songStartBar=value;
     else if(kind==5){
      const int ix=bar_*16+editIndex_;
      if(ix>=0&&ix<edit::kMaxSteps)
       (void)edit::modify(pat,ix,value,pat.steps[static_cast<std::size_t>(ix)].velocity,
                          pat.steps[static_cast<std::size_t>(ix)].length);
     }
     (void)transact(std::move(next),mode_,std::move(pat));
    }catch(...){status_="INVALID SELECTION";redraw();}
   }
  }
  opened->forget();redraw();
 }))menu->forget();
}
void Controller::numericEdit(int index,int row){
 if(!editor_||!editor_->getFrame()||index<0||index>=editable_.bars*16)return;
 auto* frame=editor_->getFrame();
 if(!textInput_){
  textInput_=new VSTGUI::CTextEdit(VSTGUI::CRect(0,0,49,48),this,9100,"100");
  textInput_->setBackColor(VSTGUI::CColor{30,47,60,255});
  textInput_->setFontColor(VSTGUI::CColor{255,255,255,255});
  textInput_->setHoriAlign(VSTGUI::kCenterText);
  frame->addView(textInput_);
 }
 editIndex_=index;editRow_=row;
 const auto& st=editable_.steps[static_cast<std::size_t>(index)];
 const int value=row==2?st.velocity:st.length;
 const int x=ui::gridX+(index%16)*ui::cellW,y=ui::gridY+row*ui::rowH;
 textInput_->setViewSize(VSTGUI::CRect(x,y,x+49,y+48));
 textInput_->setText(std::to_string(value).c_str());
 textInput_->setVisible(true);
 frame->setFocusView(textInput_);
}
void Controller::valueChanged(VSTGUI::CControl* control){
 if(!textInput_||control!=textInput_||editIndex_<0)return;
 try{
  const int value=std::stoi(textInput_->getText().data());
  auto pat=editable_;const auto& st=pat.steps[static_cast<std::size_t>(editIndex_)];
  if(editRow_==2 && value>=1 && value<=127)
   (void)edit::modify(pat,editIndex_,st.pitch,value,st.length);
  else if(editRow_==3 && value>=1 && value<=128)
   (void)edit::modify(pat,editIndex_,st.pitch,st.velocity,value);
  else {status_="ENTER A VALID NUMBER";redraw();return;}
  (void)commitEdit(std::move(pat));
 }catch(...){status_="INVALID NUMBER";redraw();}
 textInput_->setVisible(false);editIndex_=-1;editRow_=-1;
}
void Controller::hover(int x,int y){
 using ui::Target;
 const char* next="CLICK TO EDIT | CTRL+CLICK: RESET";
 switch(ui::hit(x,y).target){
 case Target::NoteOn:next="NOTE ON: click toggle | SHIFT+click LOCK | CTRL+click reset";break;
 case Target::Pitch:next="PITCH: double-click choose scale note | CTRL+click root";break;
 case Target::Velocity:next="VELOCITY: drag up/down or double-click 1-127 | CTRL+click 100";break;
 case Target::Length:next="LENGTH: drag up/down or double-click steps | CTRL+click 1";break;
 case Target::Bars:next="BARS: choose 1 / 2 / 4 / 8";break;
 case Target::Root:next="ROOT: note names follow Studio One (MIDI 60 = C3)";break;
 case Target::Scale:next="SCALE: generated unlocked notes follow the selected scale";break;
 case Target::Variation:next="VARIATION: intensity for next VARIATE; slider leaves notes unchanged";break;
 case Target::SongStart:next="SONG START: DAW bar at which pattern bar 1 starts";break;
 case Target::Copy:next="COPY BAR: store selected 16 steps";break;
 case Target::Paste:next="PASTE BAR: paste into current bar (LOCKED steps protected)";break;
 case Target::Variate:next="VARIATE: edits visible notes in this bar; intensity set above";break;
 case Target::Export:next="EXPORT MIDI: exports exactly the selected bar count";break;
 default:break;
 }
 if(help_!=next){help_=next;redraw();}
}
void Controller::exportMidi(){
 if(!editor_||!editor_->getFrame()){status_="NO EDITOR";redraw();return;}
 auto* chooser=VSTGUI::CNewFileSelector::create(editor_->getFrame(),VSTGUI::CNewFileSelector::kSelectSaveFile);
 if(!chooser){status_="SAVE DIALOG FAILED";redraw();return;}
 chooser->setTitle("Export EBM MIDI");
 chooser->setDefaultSaveName("125A EBM Bassline.mid");
 chooser->setDefaultExtension(VSTGUI::CFileExtension("Standard MIDI File","mid"));
 if(chooser->runModal()&&chooser->getNumSelectedFiles()>0){
  try{
   const auto* path=chooser->getSelectedFile(0);
   const auto bytes=toMidi(edit::toPhrase(editable_));
   if(!path||bytes.empty())status_="MIDI EXPORT FAILED";
   else{
    std::ofstream out(std::filesystem::u8path(path),std::ios::binary|std::ios::trunc);
    if(out){out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
       out.flush();status_=out.good()?"MIDI EXPORTED":"WRITE FAILED";}
    else status_="SAVE PATH FAILED";
   }
  }catch(...){status_="EXPORT FAILED";}
 }
 chooser->forget();redraw();
}
void Controller::click(int x,int y,bool ctrl,bool shift,bool doubleClick){
 using ui::Target;
 const auto target=ui::hit(x,y);const int index=bar_*16+target.column;
 switch(target.target){
 case Target::Style:if(ctrl){auto next=session_;next.setStyle(Style::ClassicEBM);
   (void)next.generate();auto pat=edit::fromPhrase(v2::render(next.current()),editable_.bars,editable_.scale,editable_.songStartBar);
   (void)edit::generateClassicDrive(pat,next.options().rootMidi,next.current().settings.seed);
   edit::conformToScale(pat,next.options().rootMidi);
   (void)transact(std::move(next),mode_,std::move(pat));}else choice(x,y,0);break;
 case Target::Bars:if(ctrl){auto next=editable_;next.bars=8;(void)commitEdit(next);}else choice(x,y,1);break;
 case Target::Root:if(ctrl){auto next=session_;const int offset=48-next.options().rootMidi;next.setRootMidi(48);
   auto pat=editable_;for(auto& s:pat.steps)if(s.on&&!s.locked)
     s.pitch=static_cast<std::uint8_t>(std::clamp(static_cast<int>(s.pitch)+offset,0,127));
   (void)transact(std::move(next),mode_,std::move(pat));}else choice(x,y,2);break;
 case Target::Scale:if(ctrl){auto p=editable_;p.scale=edit::Scale::Chromatic;(void)commitEdit(p);}else choice(x,y,3);break;
 case Target::Variation:if(ctrl)setVariationFromPixel(628+30*141/100);break;
 case Target::SongStart:if(ctrl){auto p=editable_;p.songStartBar=2;(void)commitEdit(p);}else choice(x,y,4);break;
 case Target::Trigger:{
  const auto desired=ctrl?live::Mode::Transport:(mode_==live::Mode::Transport?live::Mode::MidiNote:live::Mode::Transport);
  if(deliver(session_,desired,editable_)){
   beginEdit(TriggerModeId);
   EditController::setParamNormalized(TriggerModeId,desired==live::Mode::MidiNote?1.:0.);
   performEdit(TriggerModeId,desired==live::Mode::MidiNote?1.:0.);
   endEdit(TriggerModeId);mode_=desired;status_="TRIGGER CHANGED";
  }else status_="HOST UPDATE FAILED";
  redraw();break;
 }
 case Target::Zoom:zoom_=zoom_>1.25?1.0:1.5;if(editor_)editor_->setZoomFactor(zoom_);redraw();break;
 case Target::PrevBar:bar_=std::max(0,bar_-1);redraw();break;
 case Target::NextBar:bar_=std::min(editable_.bars-1,bar_+1);redraw();break;
 case Target::Bar:if(target.column<editable_.bars){bar_=target.column;redraw();}break;
 case Target::Generate:{
  auto next=session_;(void)next.generate();
  auto p=edit::fromPhrase(v2::render(next.current()),editable_.bars,editable_.scale,editable_.songStartBar);
  if(next.options().style==Style::ClassicEBM)
   (void)edit::generateClassicDrive(p,next.options().rootMidi,next.current().settings.seed);
  for(int i=0;i<edit::kMaxSteps;++i)if(editable_.steps[static_cast<std::size_t>(i)].locked)
   p.steps[static_cast<std::size_t>(i)]=editable_.steps[static_cast<std::size_t>(i)];
  edit::conformToScale(p,next.options().rootMidi);
  variationTurn_=0;
  (void)transact(std::move(next),mode_,std::move(p));break;
 }
 case Target::Variate:{
  if(session_.options().variation==0){status_="VARIATE: 0% IS NEUTRAL";redraw();break;}
  auto p=editable_;
  const int nextTurn=variationTurn_+1;
  const auto result=edit::variateVisible(p,session_.options().variation,nextTurn,
                                        session_.options().rootMidi,bar_);
  if(result==edit::VariationEdit::None){status_="VARIATE: NO UNLOCKED NOTES";redraw();break;}
  if(commitEdit(std::move(p))){
   variationTurn_=nextTurn;
   switch(result){
    case edit::VariationEdit::Accent:status_="VARIATE: ACCENT CHANGED";break;
    case edit::VariationEdit::Pitch:status_="VARIATE: PITCH CHANGED";break;
    case edit::VariationEdit::Gate:status_="VARIATE: LENGTH CHANGED";break;
    case edit::VariationEdit::Timing:status_="VARIATE: TIMING CHANGED";break;
    default:break;
   }
   redraw();
  }
  break;
 }
 case Target::Clear:{auto p=editable_;(void)edit::clear(p);(void)commitEdit(std::move(p));break;}
 case Target::Copy:{for(int j=0;j<16;++j)clipboard_[static_cast<std::size_t>(j)]=editable_.steps[static_cast<std::size_t>(bar_*16+j)];
  copied_=true;status_="BAR COPIED";redraw();break;}
 case Target::Paste:{
  if(!copied_){status_="COPY A BAR FIRST";redraw();break;}
  auto p=editable_;bool allowed=true;
  for(int j=0;j<16;++j)if(p.steps[static_cast<std::size_t>(bar_*16+j)].locked)allowed=false;
  if(!allowed){status_="DESTINATION LOCKED";redraw();break;}
  for(int j=0;j<16;++j)p.steps[static_cast<std::size_t>(bar_*16+j)]=clipboard_[static_cast<std::size_t>(j)];
  (void)commitEdit(std::move(p));break;
 }
 case Target::Export:exportMidi();break;
 case Target::NoteOn:{
  auto p=editable_;
  if(shift)p.steps[static_cast<std::size_t>(index)].locked=!p.steps[static_cast<std::size_t>(index)].locked;
  else if(ctrl){auto& st=p.steps[static_cast<std::size_t>(index)];
   if(!st.locked){st.on=false;st.pitch=static_cast<std::uint8_t>(session_.options().rootMidi);st.velocity=100;st.length=1;}}
  else {const bool old=p.steps[static_cast<std::size_t>(index)].on;
   if(edit::toggle(p,index)&&!old)
    p.steps[static_cast<std::size_t>(index)].pitch=static_cast<std::uint8_t>(
      edit::nearestPitch(session_.options().rootMidi,session_.options().rootMidi,p.scale));
  }
  (void)commitEdit(std::move(p));break;
 }
 case Target::Pitch:{
  if(editable_.steps[static_cast<std::size_t>(index)].locked)break;
  if(ctrl){auto p=editable_;auto& st=p.steps[static_cast<std::size_t>(index)];
   (void)edit::modify(p,index,session_.options().rootMidi,st.velocity,st.length);(void)commitEdit(std::move(p));}
  else if(doubleClick){editIndex_=target.column;choice(x,y,5);}break;
 }
 case Target::Velocity:case Target::Length:{
  if(editable_.steps[static_cast<std::size_t>(index)].locked)break;
  if(ctrl){auto p=editable_;const auto& st=p.steps[static_cast<std::size_t>(index)];
   (void)edit::modify(p,index,st.pitch,target.target==Target::Velocity?100:st.velocity,
                       target.target==Target::Length?1:st.length);
   (void)commitEdit(std::move(p));}
  else if(doubleClick)numericEdit(index,target.row);
  break;
 }
 default:break;
 }
}
} // namespace ebm::vst3

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace ebm::vst3;
BEGIN_FACTORY_DEF("125A Systems","https://github.com/challanger2000/125A-EBM-Bassline-Generator","")
DEF_CLASS2(INLINE_UID_FROM_FUID(ProcessorUID),PClassInfo::kManyInstances,kVstAudioEffectClass,
 "125A EBM Bassline Generator",Vst::kDistributable,Vst::PlugType::kInstrumentSynth,
 "0.2.0",kVstVersionString,Processor::createInstance)
DEF_CLASS2(INLINE_UID_FROM_FUID(ControllerUID),PClassInfo::kManyInstances,kVstComponentControllerClass,
 "125A EBM Bassline Generator Controller",0,"","0.2.0",kVstVersionString,Controller::createInstance)
END_FACTORY
