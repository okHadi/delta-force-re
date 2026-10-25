#include "dfre/mission_df_network_callbacks.hpp"
namespace dfre {namespace {using U=std::uint32_t;
U read(const MissionClientAddedBindings&b,U at,U width=4){if(!b.read)throw MissionClientAddedError("Network callback current read is not bound");return b.read(at,width);}
void write(const MissionClientAddedBindings&b,U at,U value,U width=4){if(!b.write)throw MissionClientAddedError("Network callback current store is not bound");b.write(at,width,value);}
U call(const MissionClientAddedBindings&b,U target,const std::vector<U>&args){if(!b.call)throw MissionClientAddedError("Network callback required body is not bound");return b.call(target,args);}
}
void mission_df_network_callback(const MissionClientAddedBindings&b,U entry,U f,U bytes){
    if(entry==0x43ff20){write(b,0x67db68,1);return;}
    if(entry==0x43fef0){const auto current=read(b,0x67db40);const auto text=call(b,0x404cf0,{0x4cc178,0x4cc1d0});call(b,0x479f00,{text,current});return;}
    if(entry!=0x43fd60)throw MissionClientAddedError("Network callback entry is not owned");
    if(!f||bytes<0xe8||std::uint64_t(f)+bytes>0x100000000ull)throw MissionClientAddedError("Network callback local frame is incomplete");
    const auto peer=read(b,0x501344);call(b,0x493a80,{f,0x4cc180,peer});call(b,0x4564b0,{f});
    if(!read(b,0x67f528))return;
    U slot=0,offset=0;
    for(;;){
        if(read(b,0x6e9c1c+offset,1)&&peer==read(b,0x6e9a90+offset))break;
        offset+=0x9b84;++slot;if(offset>=0xa53c4){call(b,0x4564b0,{0x4cc1a4});return;}
    }
    const auto text=call(b,0x404cf0,{0x4cc178,0x4cc198});const auto message=call(b,0x43f770,{text,0x6e9a9c+offset});
    U source=message,destination=f+0x83;
    for(;;){const auto first=read(b,source,1);write(b,destination,first,1);if(!first)break;const auto second=read(b,source+1,1);source+=2;write(b,destination+1,second,1);destination+=2;if(!second)break;}
    U length=0;while(read(b,f+0x83+length,1))++length;++length;
    write(b,f+0x82,0,1);const auto actor=read(b,0x6e9a98+offset);write(b,f+0x80,length+3,2);
    if(actor){write(b,f+0xe4,(actor-0x7cca80u)/0x114u);call(b,0x45da30,{actor});call(b,0x4423f0,{0x10,f+0xe4,2});}
    const U client=0x6e9a90+slot*0x9b84;call(b,0x49a620,{client,0,0x9b84});call(b,0x4423f0,{0x12,f+0x80,length+3});call(b,0x442450,{client});
    U cursor=slot;const auto limit=slot+0xa53c4;do{cursor+=0x9b84;write(b,cursor+0x6e00ed,0,1);}while(cursor!=limit);
}
} // namespace dfre
