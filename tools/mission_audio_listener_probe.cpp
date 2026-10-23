#include "dfre/mission_audio_listener.hpp"
#include <iostream>
#include <map>
#include <sstream>
int main(){try{std::uint64_t x,y,z,yaw,pitch;while(std::cin>>x>>y>>z>>yaw>>pitch){
    std::map<std::uint32_t,std::uint32_t> cells;std::ostringstream trace;
    dfre::MissionAudioListenerBindings b;b.write=[&](auto at,auto width,auto value){cells[at]=value;trace<<at<<','<<width<<','<<value<<';';};
    dfre::mission_audio_listener(std::uint32_t(x),std::uint32_t(y),std::uint32_t(z),std::uint32_t(yaw),std::uint32_t(pitch),b);
    for(auto at:{0x6e06a8u,0x6e06acu,0x6e06b0u,0x6e4708u,0x6e4704u,0x6e06b4u,0x6e4700u,0x6e06b8u})std::cout<<cells.at(at)<<' ';
    std::cout<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
