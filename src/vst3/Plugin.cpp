#include "Plugin.hpp"
#include "EbmPanel.hpp"
#include "EbmBrand.hpp"
#include "UiGeometry.hpp"
#include "ebm/MidiWriter.hpp"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/cframe.h"
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
    if(stream->read(bytes.data(),static_cast<int32>(bytes.size()),&count)!=kResultOk ||
       count!=static_cast<int32>(bytes.size()))return false;
    return host::decode(bytes.data(),bytes.size(),document);
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
        if(block.ppqValid)block.projectPpq=context->projectTimeMusic;
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
    return model_.process(block,triggers,count,sink)?kResultOk:kResultFalse;
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
    param->setPrecision(0);
    parameters.addParameter(param);
    // Constructor defaults correspond to the Processor's default composed riff.
    (void)session_.generate();
    return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream){
 host::Document doc;
 if(!readDocument(stream,doc))return kResultFalse;
 try{
  v2::Session next;next.restore(doc.session);
  session_=std::move(next);
  mode_=doc.mode;
  bar_=std::clamp(bar_,0,session_.options().bars-1);
  EditController::setParamNormalized(TriggerModeId,mode_==live::Mode::Transport?0.:1.);
  redraw();return kResultOk;
 }catch(...){return kResultFalse;}
}
// Zoom and active BAR are UI-only and never overwrite the Processor musical state.
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
    bytes[0]!=0x554D4245u || (bytes[1]!=100&&bytes[1]!=150) ||
    bytes[2]>=8)return kResultFalse;
 zoom_=bytes[1]/100.;bar_=std::clamp(static_cast<int>(bytes[2]),0,session_.options().bars-1);
 if(editor_)editor_->setZoomFactor(zoom_);
 redraw();return kResultOk;
}
tresult PLUGIN_API Controller::setParamNormalized(ParamID id,ParamValue v){
 const auto result=EditController::setParamNormalized(id,v);
 if(result==kResultOk && id==TriggerModeId && std::isfinite(v)){
  mode_=v<0.5?live::Mode::Transport:live::Mode::MidiNote;
  redraw();
 }
 return result;
}
IPlugView* PLUGIN_API Controller::createView(FIDString type){
 if(!type || std::strcmp(type,ViewType::kEditor)!=0)return nullptr;
 auto* view=new VSTGUI::VST3Editor(this,"EBMView","ebm.uidesc");
 view->setAllowedZoomFactors(std::vector<double>{1.0,1.5});
 view->setEditorSizeConstrains(VSTGUI::CPoint(ui::width,ui::height),VSTGUI::CPoint(ui::width,ui::height));
 editor_=view;
 editor_->setZoomFactor(zoom_);
 return view;
}
VSTGUI::CView* Controller::createCustomView(VSTGUI::UTF8StringPtr name,
   const VSTGUI::UIAttributes& attributes,const VSTGUI::IUIDescription*,VSTGUI::VST3Editor*){
 if(!name)return nullptr;
 VSTGUI::CPoint origin{0.,0.},size{1100.,590.};
 attributes.getPointAttribute("origin",origin);
 attributes.getPointAttribute("size",size);
 if(std::strcmp(name,"EbmPanel")==0){panel_=new EbmPanel(VSTGUI::CRect(origin,size),this);return panel_;}
 if(std::strcmp(name,"EbmBrand")==0)return new EbmBrand(VSTGUI::CRect(origin,size));
 return nullptr;
}
void Controller::willClose(VSTGUI::VST3Editor* closed){
 if(editor_==closed){editor_=nullptr;panel_=nullptr;}
}
void Controller::redraw(){if(panel_)panel_->invalid();}
bool Controller::deliver(const v2::Session& next,live::Mode mode){
 host::Document doc{next.snapshot(),mode};
 host::DocumentBytes bytes{};
 if(!host::encode(doc,bytes))return false;
 auto* message=allocateMessage();
 if(!message)return false;
 message->setMessageID("125A_EBM_FULL_DOCUMENT_V1");
 const auto attributes=message->getAttributes();
 if(!attributes || attributes->setBinary("State",bytes.data(),static_cast<uint32>(bytes.size()))!=kResultOk){
  message->release();return false;
 }
 const bool ok=sendMessage(message)==kResultOk;
 message->release();return ok;
}
bool Controller::transact(v2::Session next,live::Mode mode){
 if(!deliver(next,mode)){status_="HOST UPDATE FAILED";redraw();return false;}
 session_=std::move(next);mode_=mode;
 bar_=std::clamp(bar_,0,session_.options().bars-1);
 status_="PATTERN SENT TO HOST";
 redraw();return true;
}
void Controller::setVariationFromPixel(int x){
 const int value=ui::variationValue(x);
 if(value==session_.options().variation)return;
 try{
  auto next=session_;
  if(next.hasCurrent())(void)next.variate(value);
  else{auto s=next.snapshot();s.options.variation=value;next.restore(s);}
  (void)transact(std::move(next),mode_);
 }catch(...){status_="INVALID VARIATION";redraw();}
}
void Controller::choice(int x,int y,int kind){
 if(!editor_||!editor_->getFrame()||!panel_)return;
 auto* menu=new VSTGUI::COptionMenu;
 const auto add=[&](const std::string& label,int value,bool selected){
  auto* item=new VSTGUI::CMenuItem(label.c_str(),value);
  if(selected)item->setChecked(true);
  menu->addEntry(item);
 };
 if(kind==0){
  for(int value=0;value<3;++value)
   add(styleName(static_cast<Style>(value)),value,static_cast<int>(session_.options().style)==value);
 }else if(kind==1){
  for(int b:{4,8})add(std::to_string(b)+" BARS",b,session_.options().bars==b);
 }else{
  static constexpr const char* notes[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
  for(int midi=24;midi<=60;++midi)
   add(std::string(notes[midi%12])+std::to_string(midi/12-1),midi,midi==session_.options().rootMidi);
 }
 VSTGUI::CPoint where{static_cast<VSTGUI::CCoord>(x),static_cast<VSTGUI::CCoord>(y)};
 panel_->localToFrame(where);
 if(!menu->popup(editor_->getFrame(),where,[this,kind](VSTGUI::COptionMenu* opened){
  int index=-1;
  auto* selected=opened->getLastItemMenu(index);
  if(selected&&index>=0){
   auto* item=selected->getEntry(index);
   if(item && item->getTag()>=0){
    try{
     auto next=session_;
     const int value=item->getTag();
     if(kind==0){next.setStyle(static_cast<Style>(value));(void)next.generate();}
     else if(kind==1){next.setBars(value);if(!next.hasCurrent())(void)next.generate();}
     else next.setRootMidi(value);
     (void)transact(std::move(next),mode_);
    }catch(...){status_="INVALID SELECTION";redraw();}
   }
  }
  opened->forget();redraw();
 }))menu->forget();
}
void Controller::exportMidi(v2::RenderEnding ending){
 if(!session_.hasCurrent()){status_="GENERATE A RIFF FIRST";redraw();return;}
 if(!editor_||!editor_->getFrame()){status_="NO EDITOR";redraw();return;}
 auto* chooser=VSTGUI::CNewFileSelector::create(editor_->getFrame(),VSTGUI::CNewFileSelector::kSelectSaveFile);
 if(!chooser){status_="SAVE DIALOG FAILED";redraw();return;}
 chooser->setTitle("Export EBM MIDI");
 chooser->setDefaultSaveName(ending==v2::RenderEnding::Loopable?"125A EBM Loop.mid":"125A EBM One-Shot.mid");
 chooser->setDefaultExtension(VSTGUI::CFileExtension("Standard MIDI File","mid"));
 if(chooser->runModal() && chooser->getNumSelectedFiles()>0){
  try{
   const auto* path=chooser->getSelectedFile(0);
   const auto bytes=toMidi(v2::render(session_.current(),ending));
   if(!path || bytes.empty())status_="MIDI EXPORT FAILED";
   else{
    std::ofstream output(std::filesystem::u8path(path),std::ios::binary|std::ios::trunc);
    if(output){output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
       output.flush();status_=output.good()?"MIDI EXPORTED":"WRITE FAILED";}
    else status_="SAVE PATH FAILED";
   }
  }catch(...){status_="EXPORT FAILED";}
 }
 chooser->forget();redraw();
}
void Controller::click(int x,int y,bool ctrl){
 using ui::Target;
 switch(ui::hit(x,y).target){
 case Target::Style:
  if(ctrl){auto next=session_;next.setStyle(Style::ClassicEBM);if(!next.hasCurrent())(void)next.generate();(void)transact(std::move(next),mode_);}
  else choice(x,y,0);break;
 case Target::Bars:
  if(ctrl){auto next=session_;next.setBars(ui::defaultValue(Target::Bars));if(!next.hasCurrent())(void)next.generate();(void)transact(std::move(next),mode_);}
  else choice(x,y,1);break;
 case Target::Root:
  if(ctrl){auto next=session_;next.setRootMidi(ui::defaultValue(Target::Root));(void)transact(std::move(next),mode_);}
  else choice(x,y,2);break;
 case Target::Variation:
  if(ctrl)setVariationFromPixel(618+ui::defaultValue(Target::Variation)*240/100);
  break;
 case Target::Trigger:{
  const auto desired=ctrl?live::Mode::Transport:(mode_==live::Mode::Transport?live::Mode::MidiNote:live::Mode::Transport);
  if(deliver(session_,desired)){
   beginEdit(TriggerModeId);
   EditController::setParamNormalized(TriggerModeId,desired==live::Mode::MidiNote?1.:0.);
   performEdit(TriggerModeId,desired==live::Mode::MidiNote?1.:0.);
   endEdit(TriggerModeId);mode_=desired;status_="TRIGGER CHANGED";
  }else status_="HOST UPDATE FAILED";
  redraw();break;
 }
 case Target::Zoom:
  zoom_=zoom_>1.25?1.0:1.5;
  if(editor_)editor_->setZoomFactor(zoom_);
  redraw();break;
 case Target::PrevBar:bar_=std::max(0,bar_-1);redraw();break;
 case Target::NextBar:bar_=std::min(session_.options().bars-1,bar_+1);redraw();break;
 case Target::Generate:{auto next=session_;(void)next.generate();(void)transact(std::move(next),mode_);break;}
 case Target::Variate:{
  auto next=session_;if(!next.nextVariant()){
   status_="NO CHANGE (VARIATION 0%?)";redraw();
  }else (void)transact(std::move(next),mode_);
  break;
 }
 case Target::ExportLoop:exportMidi(v2::RenderEnding::Loopable);break;
 case Target::ExportOneShot:exportMidi(v2::RenderEnding::OneShot);break;
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
