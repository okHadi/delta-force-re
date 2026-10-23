#include "dfre/mission_audio_listener.hpp"
#include <cassert>
#include <map>
#include <vector>
using namespace dfre;
int main(){
    bool failed=false;try{mission_audio_listener(0,0,0,0,0,{});}catch(const MissionAudioListenerError&){failed=true;}assert(failed);
    std::map<std::uint32_t,std::uint32_t> cells;std::vector<std::uint32_t> writes;
    MissionAudioListenerBindings b;b.write=[&](auto at,auto width,auto n){assert(width==4);writes.push_back(at);cells[at]=n;};
    mission_audio_listener(0x80000000,0xffffffff,7,8,9,b);
    assert(cells.at(0x6e06a8)==0x80000000&&cells.at(0x6e4708)==0x80000000);
    assert(cells.at(0x6e06ac)==0xffffffff&&cells.at(0x6e4704)==0xffffffff);
    assert(cells.at(0x6e06b0)==7&&cells.at(0x6e4700)==7&&cells.at(0x6e06b4)==8&&cells.at(0x6e06b8)==9);
    assert((writes==std::vector<std::uint32_t>{0x6e06a8,0x6e06ac,0x6e06b0,0x6e4708,0x6e4704,0x6e06b4,0x6e4700,0x6e06b8}));
    writes.clear();b.write=[&](auto at,auto,auto n){if(at==0x6e4708)throw MissionAudioListenerError("Store owner failed");writes.push_back(at);cells[at]=n;};
    failed=false;try{mission_audio_listener(10,11,12,13,14,b);}catch(const MissionAudioListenerError&){failed=true;}
    assert(failed&&writes.size()==3&&cells.at(0x6e06a8)==10&&cells.at(0x6e06b0)==12&&cells.at(0x6e06b4)==8);
}
