#include "Plugin.hpp"
#include "EbmPanel.hpp"
#include "EbmBrand.hpp"
#include "UiGeometry.hpp"
#include "ebm/MidiWriter.hpp"
#include "ebm/Automation.hpp"
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
    const auto size=bytes[3]=='1'?host::kLegacyDocumentBytes:
                    bytes[3]=='2'?host::kV2DocumentBytes:host::kDocumentBytes;
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
    return kResultOk;
}
tresult PLUGIN_API Processor::process(ProcessData& data){
    if(data.numSamples<0)return kInvalidArgument;
    host::AutomationPlan automation;
    bool inputOverflow=false;
    if(data.inputParameterChanges){
        auto* changes=data.inputParameterChanges;
        const int32 queues=changes->getParameterCount();
        if(queues<0||queues>64)inputOverflow=true;
        for(int32 i=0;!inputOverflow&&i<queues;++i){
            auto* q=changes->getParameterData(i);
            if(!q){inputOverflow=true;break;}
            const auto id=q->getParameterId();
            if(id!=TriggerModeId&&id!=GateId)continue;
            const int32 points=q->getPointCount();
            if(points<0||points>host::AutomationPlan::capacity-automation.count){inputOverflow=true;break;}
            for(int32 n=0;n<points;++n){
                int32 offset=0;ParamValue value=0.;
                if(q->getPoint(n,offset,value)!=kResultOk||
                   !automation.add(offset,id==GateId,value,data.numSamples)){inputOverflow=true;break;}
            }
        }
    }
    automation.sort();
    live::Block block;
    block.samples=data.numSamples;
    block.sampleRate=sampleRate_;
    const auto* context=data.processContext;
    if(context){
        block.transportValid=true;
        block.tempoValid=(context->state&ProcessContext::kTempoValid)!=0;
        block.ppqValid=(context->state&ProcessContext::kProjectTimeMusicValid)!=0;
        block.playing=(context->state&ProcessContext::kPlaying)!=0;
        if(block.tempoValid && std::isfinite(context->tempo) && context->tempo>0. && context->tempo<=1000.){
            block.tempo=context->tempo;
            lastValidTempo_=context->tempo;
        }else{
            block.tempoValid=false;
        }
        if(block.ppqValid)block.projectPpq=context->projectTimeMusic;
    }
    // MIDI-note triggering may continue while the DAW has no tempo flag.
    // Transport triggering still requires an explicitly valid host tempo.
    if(!block.tempoValid)block.tempo=lastValidTempo_;
    live::Trigger triggers[live::Runtime::maxTriggers]{};
    int count=0;
    if(data.inputEvents){
        const int32 total=data.inputEvents->getEventCount();
        if(total<0)inputOverflow=true;
        for(int32 i=0;!inputOverflow&&i<total;++i){
            Steinberg::Vst::Event ev{};
            if(data.inputEvents->getEvent(i,ev)!=kResultOk){inputOverflow=true;break;}
            if(ev.type!=Steinberg::Vst::Event::kNoteOnEvent &&
               ev.type!=Steinberg::Vst::Event::kNoteOffEvent)continue;
            const bool on=ev.type==Steinberg::Vst::Event::kNoteOnEvent && ev.noteOn.velocity>0.f;
            const int pitch=ev.type==Steinberg::Vst::Event::kNoteOnEvent?ev.noteOn.pitch:ev.noteOff.pitch;
            const float velocity=ev.type==Steinberg::Vst::Event::kNoteOnEvent?ev.noteOn.velocity:ev.noteOff.velocity;
            if(ev.sampleOffset<0||ev.sampleOffset>=data.numSamples||pitch<0||pitch>127||
               !std::isfinite(velocity)||velocity<0.f||velocity>1.f){
                inputOverflow=true; // Malformed input also requires safe voice cleanup.
                break;
            }
            if(count>=live::Runtime::maxTriggers){inputOverflow=true;break;}
            triggers[count++]={ev.sampleOffset,pitch,on};
        }
    }
    int sliceOffset=0;
    auto sink=[&](live::Output e) noexcept -> bool {
        e.offset+=sliceOffset;
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
    if(!model_.beginBlock())return kResultFalse;
    if(inputOverflow)cleanupPending_.store(true,std::memory_order_release);
    if(cleanupPending_.exchange(false,std::memory_order_acq_rel)){
        if(!model_.stop(sink)){
            cleanupPending_.store(true,std::memory_order_release);return kResultFalse;
        }
    }
    if(inputOverflow)return kResultFalse;
    for(int i=1;i<count;++i){auto v=triggers[i];int j=i;
        while(j>0&&(triggers[j-1].offset>v.offset||
              (triggers[j-1].offset==v.offset&&triggers[j-1].on&&!v.on))){triggers[j]=triggers[j-1];--j;}
        triggers[j]=v;
    }
    int point=0,pos=0;
    do {
        while(point<automation.count&&automation.points[static_cast<unsigned>(point)].offset==pos){
            const auto p=automation.points[static_cast<unsigned>(point++)];
            const bool applied=p.gate?model_.setGate(static_cast<int>(std::lround(p.value*100.))):
                model_.setMode(p.value<0.5?live::Mode::Transport:live::Mode::MidiNote);
            if(!applied){cleanupPending_.store(true,std::memory_order_release);return kResultFalse;}
        }
        const int end=point<automation.count?automation.points[static_cast<unsigned>(point)].offset:data.numSamples;
        live::Trigger sliced[live::Runtime::maxTriggers]{};int selected=0;
        for(int i=0;i<count;++i)if(triggers[i].offset>=pos&&triggers[i].offset<end){
            sliced[selected]=triggers[i];sliced[selected++].offset-=pos;
        }
        auto part=block;part.samples=end-pos;
        if(part.ppqValid)part.projectPpq+=pos*part.tempo/(60.*part.sampleRate);
        sliceOffset=pos;
        if(!model_.processPrepared(part,sliced,selected,sink)){
            cleanupPending_.store(true,std::memory_order_release);return kResultFalse;
        }
        pos=end;
    }while(pos<data.numSamples);
    const int current=model_.playheadStep();
    const int step=current<0?16:current;
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
 if(msg && msg->getMessageID() && std::strcmp(msg->getMessageID(),"125A_EBM_GATE_V1")==0){
  int64 gate=-1;
  if(!msg->getAttributes() || msg->getAttributes()->getInt("Gate",gate)!=kResultOk ||
     gate<0 || gate>100)return kResultFalse;
  return model_.setGate(static_cast<int>(gate))?kResultOk:kResultFalse;
 }
 if(msg&&msg->getMessageID()&&std::strcmp(msg->getMessageID(),"125A_EBM_MODE_V1")==0){
  int64 mode=-1;
  if(!msg->getAttributes()||msg->getAttributes()->getInt("Mode",mode)!=kResultOk||mode<0||mode>1)return kResultFalse;
  return model_.setMode(static_cast<live::Mode>(mode))?kResultOk:kResultFalse;
 }
 if(!msg || !msg->getMessageID() || std::strcmp(msg->getMessageID(),"125A_EBM_FULL_DOCUMENT_V1")!=0)
  return AudioEffect::notify(msg);
 const void* data=nullptr;
 uint32 size=0;
 if(!msg->getAttributes() ||
    msg->getAttributes()->getBinary("State",data,size)!=kResultOk)
  return kResultFalse;
 host::Document restored;
 if(!host::decode(static_cast<const std::uint8_t*>(data),size,restored) ||
    !model_.restore(restored,false)) return kResultFalse;
 return kResultOk;
}

tresult PLUGIN_API Controller::initialize(FUnknown* context){
 const auto result=EditController::initialize(context);
 if(result!=kResultOk)return result;
 auto* param=new RangeParameter(STR16("Playback Trigger"),TriggerModeId,
                                  STR16(""),0.,1.,0.,1,ParameterInfo::kCanAutomate);
 param->setPrecision(0);parameters.addParameter(param);
 auto* gateParam=new RangeParameter(STR16("Global Gate"),GateId,STR16("%"),0.,100.,100.,100,ParameterInfo::kCanAutomate);
 gateParam->setPrecision(0);parameters.addParameter(gateParam);
 auto* playheadParam=new RangeParameter(STR16("Pattern Playhead"),PlayheadStepId,STR16(""),0.,16.,16.,16,ParameterInfo::kIsReadOnly);
 playheadParam->setPrecision(0);parameters.addParameter(playheadParam);
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
                             next.options().bars,edit::Scale::Chromatic,1);
  if(!edit::valid(pat))return kResultFalse;
  session_=std::move(next);editable_=pat;mode_=doc.mode;gate_=doc.gatePercent;
  bar_=std::clamp(bar_,0,editable_.bars-1);
  EditController::setParamNormalized(TriggerModeId,mode_==live::Mode::Transport?0.:1.);
  EditController::setParamNormalized(GateId,gate_/100.);
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
 if(result==kResultOk&&id==GateId&&std::isfinite(v)){
  gate_=std::clamp(static_cast<int>(std::lround(v*100.)),0,100);redraw();
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
 if(editor_==closed){
  // A host may close the editor mid-drag without sending mouse-up.
  endGateGesture();
  editor_=nullptr;panel_=nullptr;textInput_=nullptr;editIndex_=-1;editRow_=-1;
 }
}
void Controller::redraw(){if(panel_)panel_->invalid();}
bool Controller::deliver(const v2::Session& next,live::Mode mode,const edit::Pattern& pattern,int gate){
 host::Document doc{next.snapshot(),mode,pattern,gate};
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
 if(!edit::valid(pattern)||!deliver(next,mode,pattern,gate_)){
  status_="HOST UPDATE FAILED";redraw();return false;
 }
 session_=std::move(next);mode_=mode;editable_=std::move(pattern);
 bar_=std::clamp(bar_,0,editable_.bars-1);
 // Non-parameterized musical edits must mark the host project modified.
 (void)EditController::setDirty(true);
 status_="PATTERN SENT TO HOST";redraw();return true;
}
bool Controller::commitEdit(edit::Pattern pattern){return transact(session_,mode_,std::move(pattern));}
void Controller::beginGateGesture(){
 if(!gateGestureOpen_){beginEdit(GateId);gateGestureOpen_=true;}
}
void Controller::endGateGesture(){
 if(gateGestureOpen_){endEdit(GateId);gateGestureOpen_=false;}
}
void Controller::setGateFromPixel(int x){
 setGateValue(ui::gateValue(x));
}
void Controller::setGateValue(int proposed){
 proposed=std::clamp(proposed,0,100);
 if(proposed==gate_)return;
 // Gate gestures send a tiny atomic-control message, not a full musical
 // document/pattern revision per mouse move. The host parameter is automated.
 auto* msg=allocateMessage();
 if(!msg){status_="GATE UPDATE FAILED";redraw();return;}
 msg->setMessageID("125A_EBM_GATE_V1");
 const bool ready=msg->getAttributes() &&
   msg->getAttributes()->setInt("Gate",proposed)==kResultOk;
 const bool sent=ready && sendMessage(msg)==kResultOk;
 msg->release();
 if(!sent){status_="GATE UPDATE FAILED";redraw();return;}
 gate_=proposed;
 if(!gateGestureOpen_)beginEdit(GateId);
 EditController::setParamNormalized(GateId,gate_/100.);
 performEdit(GateId,gate_/100.);
 if(!gateGestureOpen_)endEdit(GateId);
 status_="GLOBAL GATE UPDATED";redraw();
}
void Controller::setVariationFromPixel(int x){
 setVariationValue(ui::variationValue(x));
}
void Controller::setVariationValue(int value){
 value=std::clamp(value,0,100);
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
 if(index<0||index>=editable_.bars*16||(row!=1&&row!=2&&row!=3))return;
 const auto& source=editable_.steps[static_cast<std::size_t>(index)];
 if(!source.on||source.locked)return;
 if(row==1){
  // Move through SCALE degrees, not chromatic semitones. The drag origin
  // remains fixed so upward/downward movement is reversible.
  const int root=session_.options().rootMidi;
  const int targetPitch=ui::pitchDrag(base,root,editable_.scale,upwardPixels);
  if(targetPitch==source.pitch)return;
  auto p=editable_;
  if(edit::modify(p,index,targetPitch,source.velocity,source.length)&&commitEdit(std::move(p))){
   status_="PITCH DRAGGED";redraw();
  }
  return;
 }
 const ui::Target target=row==2?ui::Target::Velocity:ui::Target::Length;
 const int maxValue=row==3?edit::maximumLength(editable_,index):127;
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
  const int selected=editable_.steps[static_cast<std::size_t>(index)].pitch;
  const int low=ui::pitchMin(root),high=ui::pitchMax(root);
  for(int midi=low;midi<=high;++midi){
   if(!edit::inScale(midi,root,editable_.scale))continue;
   add(std::string(notes[midi%12])+std::to_string(edit::studioOneOctave(midi)),midi,
       editable_.steps[static_cast<std::size_t>(index)].pitch==midi);
  }
  // Never make an existing out-of-range note impossible to inspect/select.
  if((selected<low||selected>high)&&edit::inScale(selected,root,editable_.scale))
   add(std::string(notes[selected%12])+std::to_string(edit::studioOneOctave(selected))+" (CURRENT)",selected,true);
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
      edit::preserveLocks(pat,fresh);
      edit::conformToScale(fresh,next.options().rootMidi);
      pat=std::move(fresh);
     }else if(kind==1){
      const int engineBars=value<=4?4:8;
      if(next.options().bars!=engineBars)next.setBars(engineBars);
      if(!next.hasCurrent())(void)next.generate();
      pat.bars=value;
     }else if(kind==2){
      if(!edit::transpose(pat,value-next.options().rootMidi))throw std::out_of_range("ROOT OUT OF MIDI RANGE");
      else next.setRootMidi(value);
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
bool Controller::wheel(int x,int y,int direction){
 const auto hit=ui::hit(x,y);
 if(hit.target==ui::Target::Gate){setGateValue(gate_+direction);return true;}
 if(hit.target==ui::Target::Variation){setVariationValue(session_.options().variation+direction);return true;}
 if(hit.target!=ui::Target::Velocity&&hit.target!=ui::Target::Length)return false;
 const int ix=bar_*16+hit.column;const auto& st=editable_.steps[static_cast<std::size_t>(ix)];
 if(!st.on||st.locked)return true;
 const int before=hit.row==2?st.velocity:edit::effectiveLength(editable_,ix);
 auto pat=editable_;
 const int value=std::clamp(before+direction,1,hit.row==2?127:edit::maximumLength(pat,ix));
 if(value!=before&&edit::modify(pat,ix,st.pitch,hit.row==2?value:st.velocity,hit.row==3?value:st.length))
  (void)commitEdit(std::move(pat));
 return true;
}
void Controller::numericEdit(int index,int row){
 if(!editor_||!editor_->getFrame()||row<2||row>5)return;
 if(row<4&&(index<0||index>=editable_.bars*16||editable_.steps[static_cast<std::size_t>(index)].locked))return;
 auto* frame=editor_->getFrame();
 if(!textInput_){
  textInput_=new VSTGUI::CTextEdit(VSTGUI::CRect(0,0,49,48),this,9100,"100");
  textInput_->setBackColor(VSTGUI::CColor{30,47,60,255});
  textInput_->setFontColor(VSTGUI::CColor{255,255,255,255});
  textInput_->setHoriAlign(VSTGUI::kCenterText);frame->addView(textInput_);
 }
 editIndex_=index;editRow_=row;
 int value=0,x=0,y=0,width=49,height=48;
 if(row==4){value=gate_;x=691;y=493;width=100;height=30;}
 else if(row==5){value=session_.options().variation;x=628;y=128;width=100;height=24;}
 else {
  const auto& st=editable_.steps[static_cast<std::size_t>(index)];
  value=row==2?st.velocity:edit::effectiveLength(editable_,index);
  x=ui::gridX+(index%16)*ui::cellW;y=ui::gridY+row*ui::rowH;
 }
 textInput_->setViewSize(VSTGUI::CRect(x,y,x+width,y+height));
 textInput_->setMouseableArea(textInput_->getViewSize());
 textInput_->setText(std::to_string(value).c_str());textInput_->setVisible(true);
 frame->setFocusView(textInput_);
}
void Controller::valueChanged(VSTGUI::CControl* control){
 if(!textInput_||control!=textInput_||editIndex_<0)return;
 int value=0;
 const int minimum=editRow_>=4?0:1;
 const int maximum=editRow_>=4?100:editRow_==2?127:edit::maximumLength(editable_,editIndex_);
 if(!ui::integer(textInput_->getText().data(),minimum,maximum,value)){
  // Never leave an uncommitted number over the real LENGTH readout.
  // The pattern remains unchanged; redraw its effective notation.
  textInput_->setVisible(false);editIndex_=-1;editRow_=-1;
  status_="INVALID VALUE - NO CHANGE";redraw();return;
 }
 if(editRow_==4)setGateValue(value);
 else if(editRow_==5)setVariationValue(value);
 else {
  auto pat=editable_;const auto& st=pat.steps[static_cast<std::size_t>(editIndex_)];
  if(!edit::modify(pat,editIndex_,st.pitch,editRow_==2?value:st.velocity,editRow_==3?value:st.length)||
     !commitEdit(std::move(pat)))return;
 }
 textInput_->setVisible(false);editIndex_=-1;editRow_=-1;
}
void Controller::hover(int x,int y){
 using ui::Target;
 const char* next="CLICK TO EDIT | CTRL+CLICK: RESET";
 switch(ui::hit(x,y).target){
 case Target::NoteOn:next="NOTE ON: click toggle | SHIFT+click LOCK | CTRL+click reset";break;
 case Target::Pitch:next="PITCH: drag scale notes | double-click menu | CTRL+click root";break;
 case Target::Velocity:next="VELOCITY: drag / wheel +/-1 / double-click 1-127 | CTRL=100";break;
 case Target::Length:next="LENGTH: 1=1/16 | drag / wheel +/-1 / double-click | CTRL=1";break;
 case Target::Gate:next="GATE: drag / wheel +/-1 / double-click 0-100 | CTRL = 100%";break;
 case Target::Bars:next="BARS: choose 1 / 2 / 4 / 8";break;
 case Target::Root:next="ROOT: note names follow Studio One (MIDI 60 = C3)";break;
 case Target::Scale:next="SCALE: generated unlocked notes follow the selected scale";break;
 case Target::Variation:next="VARIATION: drag / wheel +/-1 / double-click 0-100 | CTRL = 30%";break;
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
   const auto bytes=toMidi(edit::toPhrase(editable_),gate_);
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
 // Numeric edits are transient overlays, not a persistent part of the step
 // grid. Any distinct command/cell click closes the previous editor before
 // the new target is handled; the stored value remains the source of truth.
 if(textInput_&&editIndex_>=0){
  const bool sameCell=(target.target==ui::Target::Velocity||target.target==ui::Target::Length)&&
      target.row==editRow_&&index==editIndex_;
  const bool sameGate=target.target==ui::Target::Gate&&editRow_==4;
  const bool sameVariation=target.target==ui::Target::Variation&&editRow_==5;
  if(!sameCell&&!sameGate&&!sameVariation){
   textInput_->setVisible(false);editIndex_=-1;editRow_=-1;
  }
 }
 switch(target.target){
 case Target::Style:if(ctrl){auto next=session_;next.setStyle(Style::ClassicEBM);
   (void)next.generate();auto pat=edit::fromPhrase(v2::render(next.current()),editable_.bars,editable_.scale,editable_.songStartBar);
   (void)edit::generateClassicDrive(pat,next.options().rootMidi,next.current().settings.seed);
    edit::preserveLocks(editable_,pat);
   edit::conformToScale(pat,next.options().rootMidi);
   (void)transact(std::move(next),mode_,std::move(pat));}else choice(x,y,0);break;
 case Target::Bars:if(ctrl){auto next=session_;auto pat=editable_;
    if(next.options().bars!=8)next.setBars(8);
    if(!next.hasCurrent())(void)next.generate();
    pat.bars=8;(void)transact(std::move(next),mode_,std::move(pat));}else choice(x,y,1);break;
 case Target::Root:if(ctrl){auto next=session_;auto pat=editable_;
   if(!edit::transpose(pat,48-next.options().rootMidi)){status_="ROOT RESET OUT OF MIDI RANGE";redraw();break;}
   next.setRootMidi(48);(void)transact(std::move(next),mode_,std::move(pat));}else choice(x,y,2);break;
 case Target::Scale:if(ctrl){auto p=editable_;p.scale=edit::Scale::Chromatic;(void)commitEdit(p);}else choice(x,y,3);break;
 case Target::Variation:if(ctrl)setVariationValue(30);else if(doubleClick)numericEdit(0,5);break;
 case Target::SongStart:if(ctrl){auto p=editable_;p.songStartBar=2;(void)commitEdit(p);}else choice(x,y,4);break;
 case Target::Trigger:{
  const auto desired=ctrl?live::Mode::Transport:(mode_==live::Mode::Transport?live::Mode::MidiNote:live::Mode::Transport);
  if(deliver(session_,desired,editable_,gate_)){
   beginEdit(TriggerModeId);
   EditController::setParamNormalized(TriggerModeId,desired==live::Mode::MidiNote?1.:0.);
   performEdit(TriggerModeId,desired==live::Mode::MidiNote?1.:0.);
   endEdit(TriggerModeId);mode_=desired;status_="TRIGGER CHANGED";
  }else status_="HOST UPDATE FAILED";
  redraw();break;
 }
 case Target::Zoom:zoom_=zoom_>1.25?1.0:1.5;if(editor_)editor_->setZoomFactor(zoom_);redraw();break;
 case Target::PrevBar:
  if(textInput_){textInput_->setVisible(false);editIndex_=-1;editRow_=-1;}
  bar_=std::max(0,bar_-1);redraw();break;
 case Target::NextBar:
  if(textInput_){textInput_->setVisible(false);editIndex_=-1;editRow_=-1;}
  bar_=std::min(editable_.bars-1,bar_+1);redraw();break;
 case Target::Bar:
  if(target.column<editable_.bars){
   if(textInput_){textInput_->setVisible(false);editIndex_=-1;editRow_=-1;}
   bar_=target.column;redraw();
  }break;
 case Target::Generate:{
  auto next=session_;(void)next.generate();
  auto p=edit::fromPhrase(v2::render(next.current()),editable_.bars,editable_.scale,editable_.songStartBar);
  if(next.options().style==Style::ClassicEBM)
   (void)edit::generateClassicDrive(p,next.options().rootMidi,next.current().settings.seed);
  edit::preserveLocks(editable_,p);
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
 case Target::Gate:if(ctrl)setGateValue(100);else if(doubleClick)numericEdit(0,4);else setGateFromPixel(x);break;
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
