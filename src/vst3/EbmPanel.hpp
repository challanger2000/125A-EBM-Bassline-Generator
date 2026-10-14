#pragma once
#include "vstgui/lib/cview.h"
namespace ebm::vst3 {
class Controller;
class EbmPanel final:public VSTGUI::CView {
public:
 EbmPanel(const VSTGUI::CRect& r,Controller* owner);
 void draw(VSTGUI::CDrawContext*) override;
 VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
 VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
 VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
 VSTGUI::CMouseEventResult onMouseCancel() override;
private:
 Controller* owner_{};
 bool dragging_{};
};
}
