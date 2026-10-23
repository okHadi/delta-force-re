#include "dfre/mission_client_finalize.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
U read(const MissionClientAddedBindings& b,U at,U width=4){if(!b.read)throw MissionClientAddedError("Client finalization needs current memory reads");return b.read(at,width);}
void write(const MissionClientAddedBindings& b,U at,U value,U width=4){if(!b.write)throw MissionClientAddedError("Client finalization needs current memory writes");b.write(at,width,value);}
U call(const MissionClientAddedBindings& b,U site,std::vector<U> args){if(!b.call)throw MissionClientAddedError("Client finalization needs its nested body");return b.call(site,args);}
void copy(const MissionClientAddedBindings& b,U from,U to){for(;;){const auto first=read(b,from,1);write(b,to,first,1);if(!first)return;const auto second=read(b,from+1,1);from+=2;write(b,to+1,second,1);to+=2;if(!second)return;}}
}
void mission_client_finalize(const MissionClientAddedBindings& b){
    if(!read(b,0x67f528))return;
    for(U offset=0;offset!=0xa53c4;offset+=0x9b84){
        const U client=0x6e9a90+offset;if(!read(b,client+0x18c,1))continue;
        const auto side=read(b,client+0xa4),face=read(b,client+0xa0);
        const auto actor=call(b,0x45d520,{face,side}),peer=read(b,client);
        write(b,client+8,actor);write(b,actor+8,peer);
        const auto current=read(b,client+8),team=read(b,client+0x9c,2);write(b,current+0x70,team,2);
        call(b,0x43f610,{client});const auto person=read(b,read(b,client+8)+0x0c);
        copy(b,client+0x0c,person+4);
    }
}
} // namespace dfre
