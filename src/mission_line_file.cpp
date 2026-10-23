#include "dfre/mission_line_file.hpp"
namespace dfre {
std::uint32_t mission_line_file(const MissionLineFileBindings& b,std::uint32_t frame){
    using U=std::uint32_t;
    const auto read=[&](U at,U width=4){if(!b.read)throw MissionSoundAssetError("Line file memory reader is not bound");return b.read(at,width);};
    const auto write=[&](U at,U width,U value){if(!b.write)throw MissionSoundAssetError("Line file memory writer is not bound");b.write(at,width,value);};
    const auto call=[&](U site,const std::vector<U>& args){if(!b.call)throw MissionSoundAssetError("Line file body is not bound");return b.call(site,args);};
    const auto name=read(frame+0x3d4);const auto file=call(0x49a41c,{name,0x4ccac4});
    write(frame+0x3bc,4,file);if(!file)return 1;
    U line=1,result=0;
    for(;;){
        if(result)break;
        const auto current=read(frame+0x3bc);
        if(!call(0x49a550,{frame,700,current})){
            const auto callback=read(frame+0x3d8);result=call(callback,{0,line});break;
        }
        U cursor=frame;
        for(;;){const auto first=read(cursor,1);if(first==10)break;if(!first){cursor=0;break;}
            ++cursor;const auto second=read(cursor,1);if(second==10)break;++cursor;if(!second){cursor=0;break;}}
        write(cursor,1,0);
        if(!b.tokenize)throw MissionSoundAssetError("Line file tokenizer4565F0 is not bound");b.tokenize(frame,frame+0x2c4);
        for(U offset=0;offset<0x7c;offset+=4){const auto value=read(frame+0x2c4+offset);write(frame+0x340+offset,4,value);}
        if(read(frame+0x340)){const auto token=read(frame+0x344);if(read(token,1)!=47){const auto callback=read(frame+0x3d8);result=call(callback,{frame+0x340,line});}}
        ++line;
    }
    const auto current=read(frame+0x3bc);call(0x49a840,{current});return result;
}
}
