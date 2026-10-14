#include "Plugin.hpp"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/base/ibstream.h"
#include <algorithm>
#include <cmath>

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

tresult PLUGIN_API Controller::initialize(FUnknown* context){
    const auto result=EditController::initialize(context);
    if(result!=kResultOk)return result;
    auto* param=new RangeParameter(STR16("Playback Trigger"),TriggerModeId,
                                   STR16(""),0.,1.,0.,1,ParameterInfo::kCanAutomate);
    param->setPrecision(0);
    parameters.addParameter(param);
    return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream){
    host::Document doc;
    if(!readDocument(stream,doc))return kResultFalse;
    return setParamNormalized(TriggerModeId,
        doc.mode==live::Mode::MidiNote?1.:0.);
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
