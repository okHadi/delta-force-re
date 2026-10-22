#include "dfre/mission_sound_assets.hpp"

namespace dfre {
namespace {
using U=std::uint32_t;
template<class F>F need(const F& f,const char* message){if(!f)throw MissionSoundAssetError(message);return f;}
U read(const MissionSoundAssetBindings& b,U at,U width=4){return need(b.read,"Sound asset reader is not bound")(at,width);}
void write(const MissionSoundAssetBindings& b,U at,U value,U width=4){need(b.write,"Sound asset writer is not bound")(at,width,value);}
U call(const MissionSoundAssetBindings& b,U site,std::initializer_list<U> args){return need(b.call,"Sound asset service is not bound")(site,{args});}
bool same_name(const MissionSoundAssetBindings& b,U first,U second){
    for(U i=0;;++i){const auto a=read(b,first+i,1),c=read(b,second+i,1);if(a!=c)return false;if(!a)return true;if(i==0xffffffffu)throw MissionSoundAssetError("Sound asset name has no end byte");}
}
void zero(const MissionSoundAssetBindings& b,U at,U size){
    U i=0;for(;size-i>=4;i+=4)write(b,at+i,0);
    for(;i<size;++i)write(b,at+i,0,1);
}
}
U mission_sound_file_open(const MissionSoundAssetBindings& b,U name,std::optional<U> result){
    if(read(b,0xd096e4)||(read(b,0xd096d8)==0&&read(b,0xd0973c)==0)){
        result=call(b,0x498cf0,{name});
        if(!read(b,0xd096e8)){
            write(b,0xd09730,0);write(b,0xd09734,0);write(b,0xd09738,0);write(b,0xd096e0,0);return *result;
        }
    }
    if(read(b,0xd096d8)){
        U selected=name;const auto archive=read(b,0xd096d8);
        if(read(b,0xd096dc))selected=call(b,0x498fb0,{name,archive});
        result=call(b,0x49e630,{selected,archive});
        const auto error=read(b,read(b,0xd096d8)+0xac);
        if(!error){write(b,0xd09730,0);write(b,0xd09734,0);write(b,0xd09738,0);write(b,0xd096e0,1);return *result;}
    }
    if(read(b,0xd0973c))for(U i=0;i<16;++i){
        const auto slot=0xd096f0+i*4,archive=read(b,slot);if(!archive)continue;
        U selected=name;if(read(b,0xd096dc))selected=call(b,0x498fb0,{name,archive});
        result=call(b,0x49e630,{selected,archive});
        const auto error=read(b,read(b,slot)+0xac);
        if(!error){write(b,0xd096e0,0);write(b,0xd09734,i);write(b,0xd09738,slot);write(b,0xd09730,1);return *result;}
    }
    call(b,0x499750,{name,0x4ce2f8});
    if(!result)throw MissionSoundAssetError("File open failure needs its incoming EDI value");
    return *result;
}
U mission_sound_allocate(const MissionSoundAssetBindings& b,U size,U alignment,U flags,U label,U tag){
    const auto allocation=call(b,0x497f60,{size,alignment,flags,label});
    if(allocation&&tag<0x1000000u)write(b,allocation-12,tag);
    return allocation;
}
U mission_sound_find_sample(const MissionSoundAssetBindings& b,U name,U context){
    U cursor=read(b,context+0x1c),i=0;
    while(i<read(b,context+8)){
        if(same_name(b,cursor,name))return read(b,cursor+0x1c)?cursor:0;
        ++i;cursor+=0x34;
    }
    return 0;
}
U mission_sound_find_bank(const MissionSoundAssetBindings& b,U name,U context){
    U cursor=read(b,context+0x38),i=0;
    while(i<read(b,context+0x28)){
        if(same_name(b,cursor+4,name))return cursor;
        ++i;cursor+=0x50;
    }
    return 0;
}
U mission_sound_mark_sample(const MissionSoundAssetBindings& b,U name,U context){
    const auto asset=mission_sound_find_sample(b,name,context);if(!asset)return 0;
    write(b,asset+0x22,1,1);return 1;
}
U mission_sound_preload(const MissionSoundAssetBindings& b,U context){
    U cursor=read(b,context+0x1c),loaded=0,i=0;
    call(b,0x499120,{context+0x3c});
    while(i<read(b,context+8)){
        if(read(b,cursor+0x22,1)){
            call(b,0x49d050,{cursor,0});++loaded;
            const auto tag=read(b,context+0xc4),allocation=read(b,cursor+0x24);
            call(b,0x498230,{allocation,tag});
        }
        ++i;cursor+=0x34;
    }
    call(b,0x499020,{});return loaded;
}
void mission_sound_fixup(const MissionSoundAssetBindings& b,U context,U scratch){
    if(!context||!read(b,context+0x10))return;
    call(b,0x4992d0,{read(b,context+0x10),0});call(b,0x499280,{scratch,4});
    call(b,0x4992d0,{read(b,context+0x10),0});call(b,0x499280,{context+0x24,read(b,scratch)});
    const auto sets=mission_sound_allocate(b,read(b,context+0x28)*0x50,4,0,0x4ce43c,read(b,context+0xc4));
    write(b,context+0x38,sets);call(b,0x4992d0,{read(b,context+0x2c),0});
    if(!read(b,context+0x38))return;
    call(b,0x499280,{read(b,context+0x38),read(b,context+0x28)*0x50});
    U bank=read(b,context+0x38),bank_index=0;
    while(bank_index<read(b,context+0x28)){
        if(read(b,bank+0x24)){
            const auto layers=mission_sound_allocate(b,read(b,bank+0x24)*0x30,4,0,0x4ce44c,read(b,context+0xc4));
            if(layers){
                U layer=layers,layer_slot=bank,layer_index=0;
                while(layer_index<read(b,bank+0x24)){
                    call(b,0x4992d0,{read(b,layer_slot+0x28),0});call(b,0x499280,{layer,0x30});
                    write(b,layer_slot+0x28,layer);
                    if(read(b,layer,2)){
                        const auto members=mission_sound_allocate(b,read(b,layer,2)*0x1c,4,0,0x4ce458,read(b,context+0xc4));
                        if(members){
                            U member=members,pointer=layer,member_index=0;
                            while(member_index<read(b,layer,2)){
                                call(b,0x4992d0,{read(b,pointer+0x10),0});call(b,0x499280,{member,0x1c});write(b,pointer+0x10,member);
                                const auto asset=read(b,context+0x1c)+read(b,member)*0x34;write(b,member,asset);
                                if((read(b,layer+8,2)&0x40)&&asset&&!read(b,asset+0x23,1))mission_sound_mark_sample(b,asset,context);
                                pointer+=4;++member_index;member+=0x1c;
                            }
                        }
                    }
                    layer+=0x30;layer_slot+=4;++layer_index;
                }
            }
        }
        ++bank_index;bank+=0x50;
    }
    mission_sound_preload(b,context);
}
U mission_sound_context_load(const MissionSoundAssetBindings& b,U name,U context,U tag,U scratch){
    zero(b,context,200);
    if(call(b,0x499120,{name})==0xffffffffu)return 0;
    write(b,context+0xc4,tag);
    for(U i=0;;++i){const auto c=read(b,name+i,1);write(b,context+0x3c+i,c,1);if(!c)break;if(i>=135)throw MissionSoundAssetError("Sound context filename exceeds its storage");}
    call(b,0x499280,{scratch,4});call(b,0x4992d0,{0,0});call(b,0x499280,{context,read(b,scratch)});
    const auto data=mission_sound_allocate(b,read(b,context+8)*0x34,4,0,0x4ce408,read(b,context+0xc4));
    write(b,context+0x1c,data);call(b,0x499280,{read(b,context+0x1c),read(b,context+8)*0x34});
    const auto triggers=mission_sound_allocate(b,read(b,context+0xc)*0xc,4,0,0x4ce418,read(b,context+0xc4));
    write(b,context+0x20,triggers);call(b,0x499280,{read(b,context+0x20),read(b,context+0xc)*0xc});
    mission_sound_fixup(b,context,scratch);call(b,0x499020,{});call(b,0x495160,{read(b,0x5014f8)});return 1;
}
} // namespace dfre
