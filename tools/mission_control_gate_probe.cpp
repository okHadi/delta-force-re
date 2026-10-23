#include "dfre/mission_control_gate.hpp"
#include <iostream>
#include <sstream>
#include <map>
int main(){try{std::uint64_t multiplayer,host,delay,mutation;while(std::cin>>multiplayer>>host>>delay>>mutation){
    std::map<std::uint32_t,std::uint32_t> cells{{0x67f520,std::uint32_t(multiplayer)},{0x67f528,std::uint32_t(host)},{0xbc5d20,std::uint32_t(delay)}};
    std::ostringstream trace;dfre::MissionControlGateBindings bindings;
    bindings.read=[&](std::uint32_t at,std::uint32_t width){trace<<at<<','<<width<<';';
        if((mutation&1)&&at==0x67f520)cells[0x67f528]=0;
        if((mutation&2)&&at==0x67f528)cells[0xbc5d20]=0x8000000;
        return cells.at(at);};
    const auto result=dfre::mission_control_gate(bindings);std::cout<<result<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
