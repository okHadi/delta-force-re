#include "dfre/mission_control_gate.hpp"
#include <cassert>
#include <map>
#include <vector>
using namespace dfre;
int main(){
    {bool failed=false;try{mission_control_gate({});}catch(const MissionControlGateError&){failed=true;}assert(failed);}
    std::map<std::uint32_t,std::uint32_t> cells{{0x67f520,0},{0x67f528,1},{0xbc5d20,1000}};
    std::vector<std::uint32_t> reads;MissionControlGateBindings b;
    b.read=[&](auto at,auto width){assert(width==4);reads.push_back(at);return cells.at(at);};
    assert(mission_control_gate(b)==0&&reads.size()==1);reads.clear();cells[0x67f520]=1;
    assert(mission_control_gate(b)==0&&reads.size()==2);reads.clear();cells[0x67f528]=0;
    assert(mission_control_gate(b)==16&&reads.size()==3);
    cells[0xbc5d20]=0x8000000;assert(mission_control_gate(b)==-2147483);
    cells[0xbc5d20]=0xffffffff;assert(mission_control_gate(b)==0);
    cells[0xbc5d20]=0x10000000;assert(mission_control_gate(b)==0);
    cells[0x67f528]=1;b.read=[&](auto at,auto width){assert(width==4);if(at==0x67f520)cells[0x67f528]=0;if(at==0x67f528)cells[0xbc5d20]=1000;return cells.at(at);};
    assert(mission_control_gate(b)==16);
    b.read=[](auto,auto)->std::uint32_t{throw MissionControlGateError("Missing shared owner");};
    bool failed=false;try{mission_control_gate(b);}catch(const MissionControlGateError&){failed=true;}assert(failed);
}
