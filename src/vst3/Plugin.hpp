#pragma once
#include "ebm/HostBridge.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uiattributes.h"
#include <atomic>
namespace VSTGUI {class CView;class VST3Editor;class IUIDescription;}
namespace ebm::vst3 {
using namespace Steinberg;
using namespace Steinberg::Vst;
// Stable EBM-specific IDs; never copy Rifferator plugin identity.
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
 tresult PLUGIN_API notify(IMessage*) override;
private:
 host::Model model_{};
 std::atomic<bool> cleanupPending_{false};
 double sampleRate_=44100.;
};
class EbmPanel;
class Controller final:public EditController,public VSTGUI::VST3EditorDelegate {
public:
 static FUnknown* createInstance(void*) {return static_cast<IEditController*>(new Controller);}
 tresult PLUGIN_API initialize(FUnknown*) override;
 tresult PLUGIN_API setComponentState(IBStream*) override;
 tresult PLUGIN_API getState(IBStream*) override;
 tresult PLUGIN_API setState(IBStream*) override;
 tresult PLUGIN_API setParamNormalized(ParamID,ParamValue) override;
 IPlugView* PLUGIN_API createView(FIDString) override;
 VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr,
       const VSTGUI::UIAttributes&,const VSTGUI::IUIDescription*,VSTGUI::VST3Editor*) override;
 void willClose(VSTGUI::VST3Editor*) override;
 const v2::Session& session() const noexcept {return session_;}
 int bar() const noexcept {return bar_;}
 double zoom() const noexcept {return zoom_;}
 const char* status() const noexcept {return status_;}
 live::Mode mode() const noexcept {return mode_;}
 void click(int x,int y,bool control);
 void setVariationFromPixel(int x);
private:
 bool transact(v2::Session next,live::Mode mode);
 bool deliver(const v2::Session& next,live::Mode mode);
 void choice(int x,int y,int kind);
 void exportMidi(v2::RenderEnding ending);
 void redraw();
 v2::Session session_{};
 live::Mode mode_{live::Mode::Transport};
 VSTGUI::VST3Editor* editor_{};
 EbmPanel* panel_{};
 int bar_{0};
 double zoom_{1.0};
 const char* status_{"READY"};
};
}
