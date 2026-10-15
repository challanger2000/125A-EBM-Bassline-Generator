#pragma once
#include <windows.h>
#include <cwchar>
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/common/memorystream.h"
#include "ebm/HostBridge.hpp"
namespace ebm::qa {
class Handler final:public Steinberg::Vst::IComponentHandler,public Steinberg::Vst::IComponentHandler2 {
 std::atomic<Steinberg::uint32> refs_{1};
public:
 int dirty=0,begins=0,ends=0;
 Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,void** out)override{
  using namespace Steinberg;using namespace Vst;if(!out)return kInvalidArgument;*out=nullptr;
  if(std::memcmp(iid,IComponentHandler::iid,16)==0||std::memcmp(iid,FUnknown::iid,16)==0)*out=static_cast<IComponentHandler*>(this);
  else if(std::memcmp(iid,IComponentHandler2::iid,16)==0)*out=static_cast<IComponentHandler2*>(this);
  if(!*out)return kNoInterface;addRef();return kResultOk;
 }
 Steinberg::uint32 PLUGIN_API addRef()override{return ++refs_;}
 Steinberg::uint32 PLUGIN_API release()override{auto n=--refs_;if(!n)delete this;return n;}
 Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID)override{++begins;return Steinberg::kResultOk;}
 Steinberg::tresult PLUGIN_API performEdit(Steinberg::Vst::ParamID,Steinberg::Vst::ParamValue)override{return Steinberg::kResultOk;}
 Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID)override{++ends;return Steinberg::kResultOk;}
 Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32)override{return Steinberg::kResultOk;}
 Steinberg::tresult PLUGIN_API setDirty(Steinberg::TBool value)override{if(value)++dirty;return Steinberg::kResultOk;}
 Steinberg::tresult PLUGIN_API requestOpenEditor(Steinberg::FIDString)override{return Steinberg::kResultOk;}
 Steinberg::tresult PLUGIN_API startGroupEdit()override{return Steinberg::kResultOk;}
 Steinberg::tresult PLUGIN_API finishGroupEdit()override{return Steinberg::kResultOk;}
};
inline BOOL CALLBACK findFrame(HWND h,LPARAM out){wchar_t name[100]{};GetClassNameW(h,name,100);
 if(std::wcsncmp(name,L"VSTGUI",6)==0){*reinterpret_cast<HWND*>(out)=h;return FALSE;}return TRUE;}
inline BOOL CALLBACK findEdit(HWND h,LPARAM out){wchar_t name[100]{};GetClassNameW(h,name,100);
 if(_wcsicmp(name,L"EDIT")==0){*reinterpret_cast<HWND*>(out)=h;return FALSE;}return TRUE;}
inline int guiChecks(HWND window,int zoom,Steinberg::Vst::IComponent* component,Steinberg::Vst::IEditController* controller,const host::Document& original){
 using namespace Steinberg;using namespace Vst;
 #define GUI_CHECK(x) do{if(!(x)){std::fprintf(stderr,"GUI FAIL %d: %s\n",__LINE__,#x);return 2;}}while(false)
 auto* handler=new Handler;GUI_CHECK(controller->setComponentHandler(handler)==kResultOk);
 IConnectionPoint *processorPoint=nullptr,*controllerPoint=nullptr;
 GUI_CHECK(component->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&processorPoint))==kResultOk);
 GUI_CHECK(controller->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&controllerPoint))==kResultOk);
 GUI_CHECK(processorPoint->connect(controllerPoint)==kResultOk);GUI_CHECK(controllerPoint->connect(processorPoint)==kResultOk);
 bool havePlayhead=false,haveGate=false;for(int32 i=0;i<controller->getParameterCount();++i){ParameterInfo info{};
  GUI_CHECK(controller->getParameterInfo(i,info)==kResultOk);if(info.id==102){haveGate=true;GUI_CHECK(info.stepCount==100&&info.defaultNormalizedValue==1.);}
  if(info.id==101){havePlayhead=true;GUI_CHECK(info.flags&ParameterInfo::kIsReadOnly);}}
 GUI_CHECK(haveGate&&havePlayhead&&controller->setParamNormalized(101,0.5)==kResultOk);
 ShowWindow(window,SW_SHOW);UpdateWindow(window);HWND gui=nullptr;
 EnumChildWindows(window,findFrame,reinterpret_cast<LPARAM>(&gui));GUI_CHECK(gui);
 auto click=[&](int x,int y,WPARAM modifiers=0){auto where=MAKELPARAM(x*zoom/100,y*zoom/100);
  SendMessageW(gui,WM_LBUTTONDOWN,MK_LBUTTON|modifiers,where);SendMessageW(gui,WM_LBUTTONUP,modifiers,where);};
 auto fixture=original;fixture.editable={};fixture.editable.enabled=true;fixture.editable.bars=8;
 GUI_CHECK(edit::modify(fixture.editable,0,48,100,16));GUI_CHECK(edit::modify(fixture.editable,2,50,90,1));
 host::DocumentBytes encoded{};GUI_CHECK(host::encode(fixture,encoded));
 auto reset=[&](){MemoryStream a(encoded.data(),encoded.size()),b(encoded.data(),encoded.size());
  return component->setState(&a)==kResultOk&&controller->setComponentState(&b)==kResultOk;};
 auto read=[&](host::Document& d){MemoryStream state;if(component->getState(&state)!=kResultOk)return false;
  return host::decode(reinterpret_cast<const std::uint8_t*>(state.getData()),state.getSize(),d);};
 GUI_CHECK(reset());host::Document observed;const int dirtyBefore=handler->dirty;
 click(231,270);GUI_CHECK(read(observed)&&!observed.editable.steps[0].on&&handler->dirty>dirtyBefore);
 click(231,270);GUI_CHECK(read(observed)&&observed.editable.steps[0].on);
 click(231,270,MK_SHIFT);click(45,131,MK_CONTROL);GUI_CHECK(read(observed));
 GUI_CHECK(observed.editable.steps[0].locked&&observed.editable.steps[0].length==16);GUI_CHECK(reset());
 // Real mouse drag on PITCH: default chromatic ROOT C2 (MIDI48),
 // two scale steps up -> D2 (MIDI50), without altering other steps.
 SendMessageW(gui,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(231*zoom/100,322*zoom/100));
 SendMessageW(gui,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(231*zoom/100,306*zoom/100));
 SendMessageW(gui,WM_LBUTTONUP,0,MAKELPARAM(231*zoom/100,306*zoom/100));
 GUI_CHECK(read(observed)&&observed.editable.steps[0].pitch==50);
 GUI_CHECK(reset());

 const int begins=handler->begins,ends=handler->ends;
 SendMessageW(gui,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(691*zoom/100,530*zoom/100));
 for(int x:{730,780,836})SendMessageW(gui,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x*zoom/100,530*zoom/100));
 SendMessageW(gui,WM_LBUTTONUP,0,MAKELPARAM(836*zoom/100,530*zoom/100));
 GUI_CHECK(handler->begins==begins+1&&handler->ends==ends+1&&controller->getParamNormalized(102)==1.);
 POINT wheel{231*zoom/100,372*zoom/100};GUI_CHECK(ClientToScreen(gui,&wheel));
 SendMessageW(gui,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),MAKELPARAM(wheel.x,wheel.y));
 GUI_CHECK(read(observed)&&observed.editable.steps[0].velocity==101);
 auto type=[&](int x,int y,const wchar_t* value){
  auto where=MAKELPARAM(x*zoom/100,y*zoom/100);
  SendMessageW(gui,WM_LBUTTONDBLCLK,MK_LBUTTON,where);SendMessageW(gui,WM_LBUTTONUP,0,where);
  HWND input=nullptr;EnumChildWindows(gui,findEdit,reinterpret_cast<LPARAM>(&input));if(!input)return false;
  if(!SetWindowTextW(input,value))return false;
  SendMessageW(input,WM_KEYDOWN,VK_RETURN,1|(MapVirtualKeyW(VK_RETURN,MAPVK_VK_TO_VSC)<<16));return true;
 };
 GUI_CHECK(type(750,518,L"50"));GUI_CHECK(read(observed)&&observed.gatePercent==50);
 GUI_CHECK(type(700,140,L"45"));GUI_CHECK(read(observed)&&observed.session.options.variation==45);
 GUI_CHECK(type(231,423,L"1"));GUI_CHECK(read(observed)&&observed.editable.steps[0].length==1);
 // A freshly opened LENGTH editor is an overlay; BAR navigation must hide
 // it rather than displaying the old value over another bar's actual LENGTH.
 SendMessageW(gui,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(231*zoom/100,423*zoom/100));
 HWND numericOverlay=nullptr;EnumChildWindows(gui,findEdit,reinterpret_cast<LPARAM>(&numericOverlay));
 GUI_CHECK(numericOverlay&&IsWindowVisible(numericOverlay));
 click(612,200);GUI_CHECK(!IsWindowVisible(numericOverlay));
 GUI_CHECK(read(observed)&&observed.editable.steps[0].length==1);

 GUI_CHECK(type(231,372,L"12x"));GUI_CHECK(read(observed)&&observed.editable.steps[0].velocity==101);
 GUI_CHECK(processorPoint->disconnect(controllerPoint)==kResultOk);GUI_CHECK(controllerPoint->disconnect(processorPoint)==kResultOk);
 processorPoint->release();controllerPoint->release();controller->setComponentHandler(nullptr);handler->release();
 std::printf("PASS native GUI %d%%: actual NOTE toggles, Dirty, STYLE reset LOCK, Gate drag gesture, wheel, actual Win32 EDIT typing and malformed rejection\n",zoom);
 #undef GUI_CHECK
 return 0;
}
}
