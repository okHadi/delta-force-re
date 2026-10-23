#include "dfre/mission_exit_resources.hpp"
#include <stdexcept>
namespace dfre {
namespace {
using U=std::uint32_t;
struct Memory {
 const MissionExitResourceBindings& b;
 explicit Memory(const MissionExitResourceBindings& value):b(value){if(!b.read||!b.write)throw std::runtime_error("exit resources memory provider required");}
 U r(U at)const{return b.read(at,4);}
 void w(U at,U value)const{b.write(at,4,value);}
 void call(U site,std::vector<U> args)const{if(!b.call)throw std::runtime_error("exit resources call provider required");b.call(site,args);}
};
}
void mission_input_record_close(const MissionExitResourceBindings& b){
 const Memory m(b);const U file=m.r(0xbd4828);if(!file)return;
 m.call(0x49a840,{file});m.w(0xbd4828,0);
}
void mission_input_replay_close(const MissionExitResourceBindings& b){
 const Memory m(b);const U file=m.r(0xbd482c);if(!file)return;
 m.call(0x49a840,{file});m.w(0xbd482c,0);m.w(0x67dbbc,2);
}
void mission_terrain_resources_release(const MissionExitResourceBindings& b){
 const Memory m(b);
 for(U offset=0;offset!=0x28;offset+=4){m.call(0x4980d0,{m.r(0xbd5284+offset)});m.call(0x4980d0,{m.r(0xbd52ac+offset)});m.call(0x4980d0,{m.r(0xbd52d4+offset)});}
 for(U offset=0;offset!=0x10;offset+=4){m.call(0x4980d0,{m.r(0xbd52fc+offset)});m.call(0x4980d0,{m.r(0xbd530c+offset)});}
 m.call(0x4980d0,{m.r(0xbd5330)});
 for(U offset=0;offset!=0x14;offset+=4)m.call(0x4980d0,{m.r(0xbd531c+offset)});
 for(U offset=0;offset!=0x24;offset+=4)m.call(0x4980d0,{m.r(0xbd5438+offset)});
 for(U offset=0;offset!=0x20;offset+=4)m.call(0x4980d0,{m.r(0xbd545c+offset)});
 m.call(0x4980d0,{m.r(0xbd5434)});m.call(0x4980d0,{m.r(0xbf67f8)});m.call(0x4980d0,{m.r(0xbd63c4)});
 m.call(0x49a620,{0xbd4f94,0,0x21c6c});
}
}
