#pragma once
#include "ebm/HostBridge.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uiattributes.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include <atomic>
#include <array>
namespace VSTGUI {class CView;class VST3Editor;class IUIDescription;class CControl;class CTextEdit;}
namespace ebm::vst3 {
using namespace Steinberg;
using namespace Steinberg::Vst;
// Stable EBM-specific IDs; never copy Rifferator plugin identity.
static const FUID ProcessorUID(0x125AEB01,0x45424D42,0x4153534C,0x00020001);
static const FUID ControllerUID(0x125AEB02,0x45424D42,0x4153534C,0x00020001);
constexpr ParamID TriggerModeId=100;
constexpr ParamID GateId=102;
constexpr ParamID PlayheadStepId=101; // read-only host-fed GUI position
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
 int reportedStep_=-1;
};
class EbmPanel;
class Controller final:public EditController,public VSTGUI::VST3EditorDelegate,public VSTGUI::IControlListener {
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
 void click(int x,int y,bool control,bool shift=false,bool doubleClick=false);
 void hover(int x,int y);
 void valueChanged(VSTGUI::CControl*) override;
 const edit::Pattern& pattern() const noexcept {return editable_;}
 const char* help() const noexcept {return help_;}
 int playhead() const noexcept {return playhead_;}
 void setVariationFromPixel(int x);
 void setGateFromPixel(int x);
 int gate() const noexcept {return gate_;}
 void dragNumeric(int index,int row,int startingValue,int upwardPixels);
private:
 bool transact(v2::Session next,live::Mode mode,edit::Pattern pattern);
 bool commitEdit(edit::Pattern pattern);
 void numericEdit(int index,int row);
 bool deliver(const v2::Session& next,live::Mode mode,const edit::Pattern& pattern,int gate);
 void choice(int x,int y,int kind);
 void exportMidi();
 void redraw();
 v2::Session session_{};
 edit::Pattern editable_{};
 std::array<edit::Step,16> clipboard_{};
 bool copied_=false;
 VSTGUI::CTextEdit* textInput_{};
 int editIndex_=-1,editRow_=-1;
 const char* help_{"NOTE ON: click | SHIFT+click: LOCK | PITCH/VELOCITY/LENGTH: double-click"};
 live::Mode mode_{live::Mode::Transport};
 VSTGUI::VST3Editor* editor_{};
 EbmPanel* panel_{};
 int bar_{0};
 int variationTurn_{0};
 int gate_{100};
 int playhead_{16};
 double zoom_{1.0};
 const char* status_{"READY"};
};
}
