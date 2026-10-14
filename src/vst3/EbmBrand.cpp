#include "EbmBrand.hpp"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cresourcedescription.h"
#include "vstgui/uidescription/cstream.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
namespace ebm::vst3 {
namespace {
void skip(const std::string& str,std::size_t& position){
 while(position<str.size() && (std::isspace(static_cast<unsigned char>(str[position]))||str[position]==','))++position;
}
bool number(const std::string& data,std::size_t& p,double& v){
 skip(data,p);if(p>=data.size())return false;
 char* end=nullptr;v=std::strtod(data.c_str()+p,&end);
 if(end==data.c_str()+p)return false;
 p=static_cast<std::size_t>(end-data.c_str());return true;
}
bool drawPath(VSTGUI::CGraphicsPath* path,const std::string& input,double scale,double x,double y){
 std::size_t p=0;char operation=0;bool has=false;
 while(p<input.size()){
  skip(input,p);if(p>=input.size())break;
  const char c=input[p];
  if(std::isalpha(static_cast<unsigned char>(c))){
   operation=c;++p;
   if(c=='Z'||c=='z'){path->closeSubpath();has=true;operation=0;}
   continue;
  }
  if(operation!='M'&&operation!='L')return false;
  double a=0.,b=0.;
  if(!number(input,p,a)||!number(input,p,b))return false;
  VSTGUI::CPoint point{x+a*scale,y+b*scale};
  if(operation=='M'){path->beginSubpath(point);operation='L';}
  else path->addLine(point);
  has=true;
 }
 return has;
}
}
EbmBrand::EbmBrand(const VSTGUI::CRect& area):VSTGUI::CView(area){
 setTransparency(true);setMouseEnabled(false);
 VSTGUI::CResourceInputStream stream;
 if(!stream.open(VSTGUI::CResourceDescription("125A_Logo_Master_FINAL.svg")))return;
 std::string xml;std::array<char,4096> buffer{};
 for(;;){
  const auto read=stream.readRaw(buffer.data(),static_cast<uint32_t>(buffer.size()));
  if(!read||read==VSTGUI::kStreamIOError)break;
  xml.append(buffer.data(),static_cast<std::size_t>(read));
  if(read<buffer.size())break;
 }
 std::size_t position=0;
 while(true){
  const auto start=xml.find("<path",position);
  if(start==std::string::npos)break;
  const auto end=xml.find("/>",start);
  if(end==std::string::npos)break;
  const auto begin=xml.find("d=\"",start);
  if(begin!=std::string::npos && begin<end){
   const auto a=begin+3,b=xml.find('"',a);
   if(b!=std::string::npos && b<end){
    const auto red=xml.find("#D71920",b);
    shapes_.push_back({xml.substr(a,b-a),red!=std::string::npos&&red<end});
   }
  }
  position=end+2;
 }
}
void EbmBrand::draw(VSTGUI::CDrawContext* dc){
 if(shapes_.empty()){setDirty(false);return;}
 const auto r=getViewSize();
 const double scale=std::min(r.getWidth()/1774.0,r.getHeight()/887.0);
 const double x=r.left+(r.getWidth()-1774.0*scale)/2.;
 const double y=r.top+(r.getHeight()-887.0*scale)/2.;
 dc->setDrawMode(VSTGUI::kAntiAliasing);
 for(const auto& s:shapes_){
  auto* path=dc->createGraphicsPath();
  if(!path)continue;
  if(drawPath(path,s.geometry,scale,x,y)){
   dc->setFillColor(s.red?VSTGUI::CColor{215,25,32,255}:VSTGUI::CColor{217,217,217,255});
   dc->drawGraphicsPath(path,VSTGUI::CDrawContext::kPathFilledEvenOdd);
  }
  path->forget();
 }
 setDirty(false);
}
}
