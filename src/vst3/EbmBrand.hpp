#pragma once
#include "vstgui/lib/cview.h"
#include <string>
#include <vector>
namespace ebm::vst3 {
class EbmBrand final:public VSTGUI::CView {
public:
 explicit EbmBrand(const VSTGUI::CRect& r);
 void draw(VSTGUI::CDrawContext*) override;
private:
 struct Shape {std::string geometry;bool red=false;};
 std::vector<Shape> shapes_;
};
}
