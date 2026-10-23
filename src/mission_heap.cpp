#include "dfre/mission_heap.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
template<class F> const F& need(const F& f){if(!f)throw std::runtime_error("Heap reached a missing provider");return f;}
U read(const MissionPeriodicBindings& b,U at){return need(b.read)(at,4);}
void write(const MissionPeriodicBindings& b,U at,U n){need(b.write)(at,4,n);}
U call(const MissionPeriodicBindings& b,U site,const std::vector<U>& args){return need(b.call)(site,args);}
U range_start(const MissionPeriodicBindings& b,U node,U candidate){
    while(node){const auto next=read(b,node+0xc);if(!next)return node;if(candidate>=node&&candidate<next)return node;node=read(b,node+0xc);}return 0;
}
U range_end(const MissionPeriodicBindings& b,U node,U candidate,U total){
    if(!node)return 0;const U end=candidate+total;
    do{const auto next=read(b,node+0xc);if(!next)return node;if(end>=node&&end<next)return node;node=read(b,node+0xc);}while(node);return 0;
}
bool overlaps(const MissionPeriodicBindings& b,U node,U candidate,U total){
    const auto next=read(b,node+0xc);
    if(candidate>=node){const auto size=read(b,node);if(candidate<node+0x10+size)return true;}
    return next&&candidate+total>next;
}
void stamp(const MissionPeriodicBindings& b,U header,U frame){
    const auto context=read(b,0x5013a0);const auto generation=read(b,context+0xc);write(b,header+4,generation);
    const auto label=read(b,frame+0x2c);write(b,header+8,label);
    const auto current=read(b,0x5013a0);const auto prior=read(b,current+0xc);write(b,current+0xc,prior+1);
}
}
U mission_heap_initialize(const MissionPeriodicBindings& b){
    const auto initial=read(b,0x5013a0);call(b,0x49a620,{initial,0,0x14});
    const auto context=read(b,0x5013a0);write(b,context+0xc,0x1000000);
    const auto budget=read(b,0xcc0870);
    if(budget){
        const auto arena=call(b,0x49dc80,{budget});const auto first=read(b,0x5013a0);write(b,first,arena);
        const auto current=read(b,0x5013a0);const auto size=read(b,0xcc0870);write(b,current+4,size);
    }else{
        const auto arena=call(b,0x49dc80,{0xf00000});const auto first=read(b,0x5013a0);write(b,first,arena);
        const auto current=read(b,0x5013a0);write(b,current+4,0xf00000);
    }
    const auto selected=read(b,0x5013a0);const auto base=read(b,selected);if(!base)return 0xffffffffu;
    write(b,base,0);write(b,base+0xc,0);
    const auto current=read(b,0x5013a0);const auto generation=read(b,current+0xc);
    write(b,base+8,0x4ceb74);write(b,base+4,generation);
    const auto increment=read(b,0x5013a0);const auto prior=read(b,increment+0xc);write(b,increment+0xc,prior+1);
    const auto head=read(b,0x5013a0);write(b,head+8,base);return 0;
}
U mission_heap_find_hole(const MissionPeriodicBindings& b,U start,U candidate,U total,U found){
    const auto before=range_start(b,start,candidate);
    if(!overlaps(b,before,candidate,total)){write(b,found,1);return before;}
    const auto end=range_end(b,before,candidate,total);write(b,found,0);return end;
}
U mission_heap_allocate(const MissionPeriodicBindings& b,U frame,U incoming_ebx){
    const auto size=read(b,frame+0x20);const auto alignment=read(b,frame+0x24);const auto context=read(b,0x5013a0);
    const U mask=alignment-1;U previous=0,node=read(b,context+8),payload=incoming_ebx;write(b,frame+4,size+0x10);
    for(;;){
        if(node!=previous){const auto next=read(b,node+0xc);const auto length=read(b,node);write(b,frame+8,next);payload=(node+0x20+length+mask)&~mask;}
        else payload+=read(b,frame+0x24);
        previous=read(b,frame+8);const U header=payload-0x10;
        if(previous){
            const auto total=read(b,frame+4);previous=node;const auto before=mission_heap_find_hole(b,node,header,total,frame);const auto found=read(b,frame);
            if(!found){node=before;continue;}
            const auto current_size=read(b,frame+0x20);write(b,header,current_size);const auto next=read(b,before+0xc);write(b,header+0xc,next);write(b,before+0xc,header);stamp(b,header,frame);return payload;
        }
        const auto current=read(b,0x5013a0);const auto base=read(b,current);const auto capacity=read(b,current+4);const auto current_size=read(b,frame+0x20);
        if(payload+current_size>base+capacity)return 0;
        const auto tail_size=read(b,frame+0x20);write(b,header+0xc,previous);write(b,header,tail_size);write(b,node+0xc,header);stamp(b,header,frame);return payload;
    }
}
U mission_heap_find_previous(const MissionPeriodicBindings& b,U header){
    const auto context=read(b,0x5013a0);U node=read(b,context+8);
    while(node){if(read(b,node+0xc)==header)return node;node=read(b,node+0xc);}return 0;
}
void mission_heap_notify_free(const MissionPeriodicBindings& b,U payload){
    const auto context=read(b,0x5013a0);U row=read(b,context+0x10);
    for(U i=0;i<0x800;++i,row+=8){const auto key=read(b,row);if(key==payload){if(read(b,row+4)){const auto callback=read(b,row+4);call(b,callback,{key});}}}
}
U mission_heap_release(const MissionPeriodicBindings& b,U frame){
    const auto payload=read(b,frame+8);const U header=payload-0x10;const auto before=mission_heap_find_previous(b,header);if(!before)return 0xffffffffu;
    const auto next=read(b,header+0xc);write(b,before+0xc,next);
    const auto context=read(b,0x5013a0);if(read(b,context+0x10)){const auto current_payload=read(b,frame+8);mission_heap_notify_free(b,current_payload);}return 0;
}
}
