#include "ebm/EditPattern.hpp"
#include "ebm/MidiWriter.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
 if(argc!=2)return 2;const std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
 for(int bars:{1,2,4,8})for(int gate:{0,50,100}){
  ebm::edit::Pattern pattern;pattern.enabled=true;pattern.bars=bars;
  if(!ebm::edit::modify(pattern,0,48,100,4)||!ebm::edit::modify(pattern,bars*16-1,60,127,1))return 3;
  const auto bytes=ebm::toMidi(ebm::edit::toPhrase(pattern),gate);
  std::ofstream file(out/("bars-"+std::to_string(bars)+"-gate-"+std::to_string(gate)+".mid"),std::ios::binary);
  file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();if(!file)return 4;
 }
 std::cout<<"PASS wrote twelve reproducible bar/gate exports\n";
}
