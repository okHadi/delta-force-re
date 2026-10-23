#include "dfre/mission_client_admission_helpers.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
U read(const MissionClientAddedBindings& b,U at,U width=1){if(!b.read)throw MissionClientAddedError("Client admission needs current memory reads");return b.read(at,width);}
void write(const MissionClientAddedBindings& b,U at,U value){if(!b.write)throw MissionClientAddedError("Client admission needs current memory writes");b.write(at,1,value);}
std::int32_t signed32(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
U length(const MissionClientAddedBindings& b,U at){U count=0;while(read(b,at+count))++count;return count;}
void copy(const MissionClientAddedBindings& b,U from,U to){for(;;){const auto first=read(b,from);write(b,to,first);if(!first)return;const auto second=read(b,from+1);from+=2;write(b,to+1,second);to+=2;if(!second)return;}}
}
void mission_client_clear_references(const MissionClientAddedBindings& b,U client){
    const U index=U(signed32(client-0x6e9a90)/0x9b84);
    for(U offset=0;offset!=0xa53c4;offset+=0x9b84){
        if(!read(b,0x6e9c1c+offset))continue;
        for(U byte=0;byte<17;++byte){const U at=0x6e9c60+offset+byte;
            if(read(b,at)&&read(b,at)==index)write(b,at,0);
        }
    }
}
U mission_client_name_message(const MissionClientAddedBindings& b,U format,U name){
    constexpr U output=0x78ee54;write(b,output,0);if(!name)return output;
    const auto format_length=length(b,format);if(!format_length||signed32(format_length)>128)return output;
    const auto name_length=length(b,name);if(!name_length||signed32(name_length)>16)return output;
    U count=0;if(!read(b,format)){write(b,output,0);return output;}
    for(;;){
        if(read(b,format)=='$'&&read(b,format+1)=='A'){copy(b,name,output+count);format+=2;count+=length(b,name);}
        else{++count;const auto value=read(b,format);++format;write(b,output+count-1,value);}
        if(!read(b,format))break;
    }
    write(b,output+count,0);return output;
}
U mission_client_peer_name(const MissionClientAddedBindings& b,U peer){
    const auto table=read(b,0x501398,4);const auto target=read(b,table+0x6c,4);
    if(!b.call)throw MissionClientAddedError("Client peer-name transport is not bound");
    return b.call(target,{peer});
}
} // namespace dfre
