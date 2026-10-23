#include "dfre/mission_netsock_queue.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
U read(const MissionNetsockQueueBindings& b,U at,U width=4){if(!b.read)throw std::runtime_error("Netsock queue current read is not bound");return b.read(at,width);}
void write(const MissionNetsockQueueBindings& b,U at,U value,U width=4){if(!b.write)throw std::runtime_error("Netsock queue current store is not bound");b.write(at,width,value);}
U call(const MissionNetsockQueueBindings& b,U target,const std::vector<U>& args){if(b.before_call)b.before_call(target,args);if(!b.call)throw std::runtime_error("Netsock queue required import or diagnostic is not bound");return b.call(target,args);}
bool signed_at_least(U value,U limit){return !(value&0x80000000u)&&value>=limit;}
}
U mission_netsock_queue_initialize(const MissionNetsockQueueBindings& b,U frame){
    const auto target=read(b,0x1000605c);const auto base=call(b,target,{0x10000});const auto queue=read(b,frame+4);write(b,queue,base);
    for(U offset=0;offset<0x10000;offset+=4)write(b,base+offset,0);
    const auto current=read(b,queue);write(b,queue+4,current);write(b,queue+8,current);write(b,queue+0xc,0);write(b,queue+0x10,0);return 0;
}
void mission_netsock_queue_enqueue(const MissionNetsockQueueBindings& b,U frame){
    const auto queue=read(b,frame+0xc);auto lock=read(b,queue+0x10);
    if(lock){const auto sleep=read(b,0x10006010);do{call(b,sleep,{0});lock=read(b,queue+0x10);}while(lock);}
    const auto length=read(b,frame+8);write(b,queue+0x10,1);if(!length){write(b,queue+0x10,0);return;}
    if(read(b,frame+0x18)==1&&signed_at_least(read(b,queue+0xc),0x800)){write(b,queue+0x10,0);return;}
    const auto initial_count=read(b,queue+0xc);if(signed_at_least(initial_count,0xff6)){call(b,0x10003aa0,{0x1000711c,initial_count});write(b,queue+0x10,0);return;}
    const auto tail=read(b,queue+8),address=read(b,frame+0x10),port=read(b,frame+0x14);write(b,tail+8,address);
    const auto old=read(b,tail);write(b,tail+4,length);write(b,tail+0xc,port);
    if(old){const auto free_target=read(b,0x10006068);call(b,free_target,{old});}
    const auto malloc_target=read(b,0x1000605c);const auto payload=call(b,malloc_target,{length});write(b,tail,payload);
    const auto source=read(b,frame+4);U next=tail+0x10,offset=0;
    for(U i=0;i<(length>>2);++i,offset+=4){const auto value=read(b,source+offset);write(b,payload+offset,value);}
    for(U i=0;i<(length&3);++i,++offset){const auto value=read(b,source+offset,1);write(b,payload+offset,value,1);}
    const auto base=read(b,queue);if(next>=base+0x10000u)next=base;
    const U count=read(b,queue+0xc)+1u;write(b,queue+8,next);write(b,queue+0xc,count);write(b,queue+0x10,0);
}
void mission_netsock_queue_destroy(const MissionNetsockQueueBindings& b,U frame){
    const auto queue=read(b,frame+4),base=read(b,queue);if(!base)return;
    const auto free_target=read(b,0x10006068);
    for(U offset=0;offset<0x10000;offset+=0x10){const auto payload=read(b,base+offset);if(payload)call(b,free_target,{payload});}
    const auto current=read(b,queue);call(b,free_target,{current});write(b,queue,0);write(b,queue+4,0);write(b,queue+8,0);write(b,queue+0x10,0);
}
} // namespace dfre
