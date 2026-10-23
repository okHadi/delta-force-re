#include "dfre/mission_banned_record.hpp"
namespace dfre {
std::uint32_t mission_banned_record(const MissionLineFileBindings& b,std::uint32_t record,std::uint32_t frame){
    using U=std::uint32_t;
    const auto read=[&](U at,U width=4){if(!b.read)throw MissionSoundAssetError("Banned record memory reader is not bound");return b.read(at,width);};
    const auto write=[&](U at,U value,U width=4){if(!b.write)throw MissionSoundAssetError("Banned record memory writer is not bound");b.write(at,width,value);};
    const auto count=read(0x6e5888);write(frame,count);if(!record)return 0;
    const auto split=[&](U cursor){if(!read(cursor,1))return cursor;for(;;){const auto c=read(cursor,1);const auto next=cursor+1;if(c==46){write(cursor,0,1);return next;}const auto cnext=read(next,1);cursor=next;if(!cnext)return cursor;}};
    const auto first=read(record+4),second=split(first),third=split(second),fourth=split(third);
    const auto number=[&](U text){if(!b.call)throw MissionSoundAssetError("Banned record atoi49AFA0 is not bound");return b.call(0x49afa0,{text});};
    const auto n0=number(first);write(frame+4,n0);const auto n1=number(second),n2=number(third),n3=number(fourth);
    const auto saved=read(frame+4);const U ip=(n1<<8)+saved+(n2<<16)+(n3<<24);const auto index=read(frame);
    write(0x6e5890+index*4,ip);const auto destination=0x6e6090+index*16;const auto fields=read(record);U source=0x4cc6ec;
    if(fields!=1){const auto label=read(record+8);U n=0;while(read(label+n,1))++n;if(n<16)source=read(record+8);}
    U target=destination;for(;;){const auto a=read(source,1);write(target,a,1);if(!a)break;const auto c=read(source+1,1);source+=2;write(target+1,c,1);target+=2;if(!c)break;}
    const auto current=read(0x6e5888);write(0x6e5888,current+1);return 0;
}
}
