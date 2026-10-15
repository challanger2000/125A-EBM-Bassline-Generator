// Native Windows host proof of the *actual* compiled EBM VST3 module.
// The official Steinberg SDK hosting helpers are linked; private 125A code is not.
#ifdef _WIN32
#include <windows.h>
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/gui/iplugview.h"
#include "ebm/HostBridge.hpp"
#include "vst3_process_probe.hpp"
#include "vst3_gui_probe.hpp"
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace Hosting=VST3::Hosting;
#define CHECK(x) do {if(!(x)){std::cerr<<"HOST FAIL line "<<__LINE__<<" "<<#x<<"\n";return 2;}}while(false)
LRESULT CALLBACK probeWindow(HWND h,UINT message,WPARAM w,LPARAM l){
 if(message==WM_GETMINMAXINFO){auto* info=reinterpret_cast<MINMAXINFO*>(l);info->ptMaxTrackSize={4096,4096};return 0;}
 return DefWindowProcW(h,message,w,l);
}
class Frame final:public IPlugFrame {
 HWND window_{};
 std::atomic<uint32> refs_{1};
public:
 explicit Frame(HWND window):window_(window){}
 tresult PLUGIN_API queryInterface(const TUID iid,void** result) override {
  if(!result)return kInvalidArgument;
  *result=nullptr;
  if(std::memcmp(iid,IPlugFrame::iid,16)!=0 && std::memcmp(iid,FUnknown::iid,16)!=0)return kNoInterface;
  *result=static_cast<IPlugFrame*>(this);addRef();return kResultOk;
 }
 uint32 PLUGIN_API addRef() override {return ++refs_;}
 uint32 PLUGIN_API release() override {const auto n=--refs_;if(!n)delete this;return n;}
 tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* r) override {
  if(!r||r->right<=r->left||r->bottom<=r->top)return kInvalidArgument;
  RECT outer{0,0,r->right-r->left,r->bottom-r->top};
  if(!AdjustWindowRect(&outer,WS_OVERLAPPEDWINDOW,FALSE))return kResultFalse;
  if(!SetWindowPos(window_,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top,
                   SWP_NOMOVE|SWP_NOZORDER))return kResultFalse;
  return view->onSize(r);
 }
};
int main(int argc,char** argv){
 CHECK(argc==2);
 std::string error;
 auto module=Hosting::Module::create(argv[1],error);
 CHECK(module!=nullptr);
 const auto& factory=module->getFactory();
 auto* host=new HostApplication;
 factory.setHostContext(host);
 Hosting::ClassInfo audio,edit;
 bool foundAudio=false,foundEdit=false;
 for(const auto& cls:factory.classInfos()){
  if(cls.name()=="125A EBM Bassline Generator"){audio=cls;foundAudio=true;}
  if(cls.name()=="125A EBM Bassline Generator Controller"){edit=cls;foundEdit=true;}
 }
 CHECK(foundAudio&&foundEdit);
 auto component=factory.createInstance<IComponent>(audio.ID());
 auto controller=factory.createInstance<IEditController>(edit.ID());
 CHECK(component&&controller);
 CHECK(component->initialize(host)==kResultOk);
 CHECK(controller->initialize(host)==kResultOk);
 CHECK(component->getBusCount(kAudio,kInput)==0&&component->getBusCount(kAudio,kOutput)==0);
 CHECK(component->getBusCount(kEvent,kInput)==1&&component->getBusCount(kEvent,kOutput)==1);
 MemoryStream original;
 CHECK(component->getState(&original)==kResultOk);
 ebm::host::Document old{};
 CHECK(ebm::host::decode(reinterpret_cast<const std::uint8_t*>(original.getData()),
                            static_cast<std::size_t>(original.getSize()),old));
 CHECK(old.session.hasCurrent && old.session.options.bars==8);
 original.seek(0,IBStream::kIBSeekSet,nullptr);
 CHECK(controller->setComponentState(&original)==kResultOk);
 // Message mirrors a Controller-side GENERATE plus ROOT edit. The same message
 // and byte-codec are used by the real clickable GUI, without changing audio-thread code.
 ebm::v2::Session changed;changed.restore(old.session);
 const auto before=changed.snapshot();
 (void)changed.generate();changed.setRootMidi(43);
 auto next=old;next.session=changed.snapshot();
 CHECK(next.session.current.seed!=before.current.seed);
 CHECK(next.session.current.rootMidi==43);
 ebm::host::DocumentBytes encoded{};
 CHECK(ebm::host::encode(next,encoded));
 IConnectionPoint* endpoint=nullptr;
 CHECK(component->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&endpoint))==kResultOk&&endpoint);
 auto* msg=new HostMessage;
 msg->setMessageID("125A_EBM_FULL_DOCUMENT_V1");
 CHECK(msg->getAttributes()->setBinary("State",encoded.data(),static_cast<uint32>(encoded.size()))==kResultOk);
 CHECK(endpoint->notify(msg)==kResultOk);
 msg->release();
 MemoryStream persisted;
 CHECK(component->getState(&persisted)==kResultOk);
 ebm::host::Document observed{};
 CHECK(ebm::host::decode(reinterpret_cast<const std::uint8_t*>(persisted.getData()),
                            static_cast<std::size_t>(persisted.getSize()),observed));
 CHECK(observed.session.current.seed==next.session.current.seed);
 CHECK(observed.session.current.rootMidi==43);
 // An invalid message must leave the complete prior musical document untouched.
 encoded[25]^=0x40u;
 auto* bad=new HostMessage;
 bad->setMessageID("125A_EBM_FULL_DOCUMENT_V1");
 CHECK(bad->getAttributes()->setBinary("State",encoded.data(),static_cast<uint32>(encoded.size()))==kResultOk);
 CHECK(endpoint->notify(bad)!=kResultOk);
 bad->release();endpoint->release();
 MemoryStream retained;
 CHECK(component->getState(&retained)==kResultOk);
 CHECK(retained.getSize()==persisted.getSize());
 CHECK(std::memcmp(retained.getData(),persisted.getData(),static_cast<std::size_t>(retained.getSize()))==0);
 persisted.seek(0,IBStream::kIBSeekSet,nullptr);
 CHECK(controller->setComponentState(&persisted)==kResultOk);
 // User-only zoom and bar state survives save/reopen. Host parser rejects bad data.
 std::uint32_t displayState[3]{0x554D4245u,150u,7u};
 MemoryStream uiState(displayState,sizeof(displayState));
 CHECK(controller->setState(&uiState)==kResultOk);
 MemoryStream uiSaved;
 CHECK(controller->getState(&uiSaved)==kResultOk&&uiSaved.getSize()==sizeof(displayState));
 CHECK(std::memcmp(uiSaved.getData(),displayState,sizeof(displayState))==0);
 std::uint32_t badState[3]{0x554D4245u,125u,7u};
 MemoryStream wrongZoom(badState,sizeof(badState));
 CHECK(controller->setState(&wrongZoom)!=kResultOk);
 MemoryStream stillValid;
 CHECK(controller->getState(&stillValid)==kResultOk);
 CHECK(std::memcmp(stillValid.getData(),displayState,sizeof(displayState))==0);
 CHECK(ebm::qa::processChecks(component.get(),old)==0);
 const wchar_t cls[]=L"EBMHostProbeWindow";
 WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=probeWindow;
 wc.lpszClassName=cls;
 CHECK(RegisterClassW(&wc)!=0);
 for(std::uint32_t zoom:{100u,150u})for(int n=0;n<20;++n){
   std::uint32_t display[3]{0x554D4245u,zoom,0u};MemoryStream selectedZoom(display,sizeof(display));
   CHECK(controller->setState(&selectedZoom)==kResultOk);
  HWND window=CreateWindowExW(0,cls,L"125A EBM QA",WS_OVERLAPPEDWINDOW,
                 50,50,1710,940,nullptr,nullptr,wc.hInstance,nullptr);
  CHECK(window!=nullptr);
  auto* view=controller->createView(ViewType::kEditor);
  CHECK(view!=nullptr);
  auto* frame=new Frame(window);
  CHECK(view->setFrame(frame)==kResultTrue);
  CHECK(view->isPlatformTypeSupported(kPlatformTypeHWND)==kResultTrue);
  CHECK(view->attached(reinterpret_cast<void*>(window),kPlatformTypeHWND)==kResultTrue);
  ViewRect area{};
  CHECK(view->getSize(&area)==kResultTrue);
  CHECK(area.right-area.left==1100*static_cast<int>(zoom)/100);
  CHECK(area.bottom-area.top==590*static_cast<int>(zoom)/100);
  CHECK(frame->resizeView(view,&area)==kResultTrue);
  RECT client{};CHECK(GetClientRect(window,&client));
  CHECK(client.right==area.right-area.left&&client.bottom==area.bottom-area.top);
  if(n==0)CHECK(ebm::qa::guiChecks(window,static_cast<int>(zoom),component.get(),controller.get(),old)==0);
  MSG wmsg{};
  while(PeekMessageW(&wmsg,nullptr,0,0,PM_REMOVE)){
   TranslateMessage(&wmsg);DispatchMessageW(&wmsg);
  }
  CHECK(view->removed()==kResultTrue);
  view->setFrame(nullptr);view->release();frame->release();DestroyWindow(window);
 }
 std::cout<<"PASS native VST3: event buses, document message to Processor, corrupted rejection, persistent GUI state, forty Win32 editor attach/detach cycles (20 each at 100/150%) with exact HWND size\n";
 return 0;
}
#else
int main(){return 0;}
#endif
