#pragma once
#include "vstgui/lib/cview.h"
#include "UiGeometry.hpp"
namespace ebm::vst3 {
class Controller;
class EbmPanel final:public VSTGUI::CView {
public:
 EbmPanel(const VSTGUI::CRect& r,Controller* owner);
 void draw(VSTGUI::CDrawContext*) override;
 void onMouseWheelEvent(VSTGUI::MouseWheelEvent&) override;
 VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
 VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
 VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
 VSTGUI::CMouseEventResult onMouseCancel() override;
private:
 Controller* owner_{};
 bool draggingGate_=false;
 bool dragging_{};
 bool draggingNumeric_{};
 int dragIndex_=-1,dragRow_=-1,dragY_=0,dragBase_=0;
 int pressedColumn_{-1};
 ui::Target pressed_{ui::Target::None}, lastAction_{ui::Target::None};
};
}
