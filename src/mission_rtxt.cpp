#include "dfre/mission_rtxt.hpp"
#include <cstring>
namespace dfre {namespace {
using U=std::uint32_t;
U read(const MissionSoundAssetBindings& b,U at,U width=4){if(!b.read)throw MissionSoundAssetError("RTXT current memory is not bound");return b.read(at,width);}
void write(const MissionSoundAssetBindings& b,U at,U value){if(!b.write)throw MissionSoundAssetError("RTXT current memory writer is not bound");b.write(at,4,value);}
std::int32_t si(U n){std::int32_t value;std::memcpy(&value,&n,4);return value;}
void increment(const MissionSoundAssetBindings& b,U at){const auto n=read(b,at);write(b,at,n+1);}
void skip(const MissionSoundAssetBindings& b,U& text){while(read(b,text++,1)&255u){}}
bool equal(const MissionSoundAssetBindings& b,U& text,U pattern){
    for(;;){const auto c=read(b,text++,1)&255u;if(!c)return (read(b,pattern,1)&255u)==0;
        if(c!=(read(b,pattern,1)&255u))return false;++pattern;}
}
U entry(const MissionSoundAssetBindings& b,U table,U index){
    if(!table||si(index)<0)return 0;
    if(si(index)>=si(read(b,table+12)))return 0;
    return table+16+index*16;
}
U publish(const MissionSoundAssetBindings& b,U result){write(b,0x499e78,result);return read(b,0x499e78);}
}
U mission_rtxt_fixup(const MissionSoundAssetBindings& b,U allocation){
    write(b,0x499e70,allocation);
    auto base=read(b,0x499e70);const auto relative=read(b,base+4);write(b,base+4,relative+base);
    const auto count=read(b,base+12);base+=16;write(b,0x499e74,count);
    const U text=base+count*16;auto remaining=read(b,0x499e74);
    while(remaining){const auto offset=read(b,base);write(b,base,offset+text);base+=16;--remaining;}
    base=read(b,0x499e70);base=read(b,base+4);remaining=read(b,base);base+=4;const auto rows=base;
    while(remaining){const auto offset=read(b,base);write(b,base,offset+rows);base+=8;--remaining;}
    return read(b,0x499e70);
}
U mission_rtxt_load(const MissionSoundAssetBindings& b,U filename){
    if(!b.call)throw MissionSoundAssetError("RTXT file loader499410 is not bound");
    const auto file=b.call(0x499410,{filename});
    return publish(b,si(file)>0?mission_rtxt_fixup(b,file):0);
}
U mission_rtxt_entry(const MissionSoundAssetBindings& b,U table,U index){return publish(b,entry(b,table,index));}
U mission_rtxt_lookup(const MissionSoundAssetBindings& b,U table,U group,U key){
    write(b,0x499e7c,group);write(b,0x499e80,key);write(b,0x499e70,table);
    write(b,0x499e88,0);write(b,0x499e8c,0);write(b,0x499e90,0);
    auto text=read(b,table+4);auto remaining=read(b,text);text+=4;write(b,0x499e84,text);
    auto row=text;text+=remaining*8;bool found=false;
    while(remaining){
        const auto pattern=read(b,0x499e7c);write(b,0x499e94,text);
        if(equal(b,text,pattern)){found=true;break;}
        increment(b,0x499e88);text=read(b,0x499e94);skip(b,text);
        const auto n=read(b,row+4),total=read(b,0x499e90);write(b,0x499e90,total+n);
        row+=8;--remaining;
    }
    if(!found)return publish(b,0);
    row=read(b,0x499e84);row+=read(b,0x499e88)*8;
    remaining=read(b,row+4);text=read(b,row);found=false;
    while(remaining){
        write(b,0x499e94,text);const auto pattern=read(b,0x499e80);
        if(equal(b,text,pattern)){found=true;break;}
        text=read(b,0x499e94);skip(b,text);increment(b,0x499e8c);--remaining;
    }
    if(!found)return publish(b,0);
    const auto total=read(b,0x499e90),local=read(b,0x499e8c);table=read(b,0x499e70);
    return publish(b,entry(b,table,total+local));
}
}
