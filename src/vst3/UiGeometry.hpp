#pragma once
#include <algorithm>
namespace ebm::vst3::ui {
constexpr int width=1100,height=590,gridX=206,gridY=250,cellW=51,rowH=51;
enum class Target {None,Style,Bars,Root,Scale,Variation,SongStart,Trigger,Zoom,
                    PrevBar,NextBar,Bar,Generate,Variate,Clear,Copy,Paste,Export,
                    NoteOn,Pitch,Velocity,Length};
struct Hit{Target target=Target::None;int column=-1;int row=-1;};
constexpr bool inside(int x,int y,int l,int t,int w,int h) noexcept {
 return x>=l&&x<l+w&&y>=t&&y<t+h;
}
constexpr Hit hit(int x,int y) noexcept {
 if(x<0||y<0||x>=width||y>=height)return {};
 if(inside(x,y,974,25,102,35))return {Target::Zoom};
 if(inside(x,y,24,102,172,65))return {Target::Style};
 if(inside(x,y,206,102,108,65))return {Target::Bars};
 if(inside(x,y,324,102,135,65))return {Target::Root};
 if(inside(x,y,469,102,136,65))return {Target::Scale};
 if(inside(x,y,615,102,166,65))return {Target::Variation};
 if(inside(x,y,791,102,123,65))return {Target::SongStart};
 if(inside(x,y,924,102,152,65))return {Target::Trigger};
 if(inside(x,y,27,207,44,37))return {Target::PrevBar};
 if(inside(x,y,160,207,44,37))return {Target::NextBar};
 for(int b=0;b<8;++b)if(inside(x,y,550+b*62,190,58,24))return {Target::Bar,b};
 if(inside(x,y,gridX,gridY,16*cellW,4*rowH)){
  const int col=(x-gridX)/cellW,row=(y-gridY)/rowH;
  return {row==0?Target::NoteOn:row==1?Target::Pitch:row==2?Target::Velocity:Target::Length,col,row};
 }
 if(inside(x,y,24,492,180,53))return {Target::Generate};
 if(inside(x,y,210,492,135,53))return {Target::Variate};
 if(inside(x,y,351,492,90,53))return {Target::Clear};
 if(inside(x,y,447,492,110,53))return {Target::Copy};
 if(inside(x,y,563,492,110,53))return {Target::Paste};
 if(inside(x,y,679,492,397,53))return {Target::Export};
 return {};
}
constexpr bool isCommand(Target t) noexcept {
 return t==Target::Generate||t==Target::Variate||t==Target::Clear||
        t==Target::Copy||t==Target::Paste||t==Target::Export;
}
constexpr int dragValue(Target target,int base,int upwardPixels,int maxLength=128) noexcept {
 if(target==Target::Velocity)return std::clamp(base+upwardPixels/2,1,127);
 if(target==Target::Length)return std::clamp(base+upwardPixels/4,1,maxLength);
 return base;
}
constexpr int variationValue(int x) noexcept {return ((std::clamp(x,628,769)-628)*100+70)/141;}
constexpr int defaultValue(Target t) noexcept {
 if(t==Target::Bars)return 8;
 if(t==Target::SongStart)return 2;
 if(t==Target::Variation)return 30;
 if(t==Target::Velocity)return 100;
 if(t==Target::Length)return 1;
 if(t==Target::Root)return 36;
 return 0;
}
}
