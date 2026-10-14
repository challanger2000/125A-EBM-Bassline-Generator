#pragma once
#include "ebm/HostBridge.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include <atomic>

namespace ebm::vst3 {
using namespace Steinberg;
using namespace Steinberg::Vst;
// Stable IDs allocated solely to 125A EBM Bassline Generator; NOT Rifferator IDs.
static const FUID ProcessorUID(0x125AEB01,0x45424D42,0x4153534C,0x00020001);
static const FUID ControllerUID(0x125AEB02,0x45424D42,0x4153534C,0x00020001);
constexpr ParamID TriggerModeId=100;
class Processor final:public AudioEffect {
public:
    Processor();
    static FUnknown* createInstance(void*) {return static_cast<IAudioProcessor*>(new Processor);}
    tresult PLUGIN_API initialize(FUnknown*) override;
    tresult PLUGIN_API setupProcessing(ProcessSetup&) override;
    tresult PLUGIN_API setActive(TBool) override;
    tresult PLUGIN_API setProcessing(TBool) override;
    tresult PLUGIN_API canProcessSampleSize(int32) override;
    tresult PLUGIN_API getState(IBStream*) override;
    tresult PLUGIN_API setState(IBStream*) override;
    tresult PLUGIN_API process(ProcessData&) override;
private:
    host::Model model_{};
    std::atomic<bool> cleanupPending_{false};
    double sampleRate_=44100.;
};
class Controller final:public EditController {
public:
    static FUnknown* createInstance(void*) {return static_cast<IEditController*>(new Controller);}
    tresult PLUGIN_API initialize(FUnknown*) override;
    tresult PLUGIN_API setComponentState(IBStream*) override;
    // No GUI in this deliberately minimal adapter. Hosts may show generic controls.
};
}
