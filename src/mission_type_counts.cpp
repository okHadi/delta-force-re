#include "dfre/mission_type_counts.hpp"
#include <cstring>
#include <limits>
namespace dfre { namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t v;std::memcpy(&v,&n,4);return v;}
U read(const MissionTypeCountBindings& b,U a,U w=4){if(!b.read)throw MissionTickError("Type counts need current memory reads");return b.read(a,w);}
void write(const MissionTypeCountBindings& b,U a,U n){if(!b.write)throw MissionTickError("Type counts need current memory writes");b.write(a,4,n);}
}
void mission_type_counts(const MissionTypeCountBindings& b){
    for(U offset=0;offset<0x1000;offset+=0x40){const auto age=read(b,0x7936b8+offset);write(b,0x7936ac+offset,0);write(b,0x7936b8+offset,age+1);}
    for(const auto group:{1u,3u,0u}){
        const U count=group==1?0x8cf904:group==3?0x8cfa18:0x8cfa1c;
        if(si(read(b,count))<=0)continue;
        const U fixed=group==3?0:read(b,count);
        U index=0,actor=group==1?0x867e80:group==3?0x7cca80:0x7dde80;
        for(;;){if(!(read(b,actor+0x6c,1)&4)){const auto shifted=U(si(read(b,actor+0x70))>>16)<<6;const auto address=0x7936acu+shifted;write(b,address,read(b,address)+1);}
            const auto bound=group==3?read(b,count):fixed;++index;actor+=0x114;if(si(index)>=si(bound))break;}
    }
    write(b,0x7936ac,0);
}
MissionTypeCountBindings mission_type_count_bindings(MissionTickState& state,AiTypeSlotTable& types){
    MissionTypeCountBindings b;
    b.read=[&](U at,U width)->U{
        if(width!=1&&width!=4)throw MissionTickError("Type-count width is invalid");
        if(at==0x8cf904||at==0x8cfa18||at==0x8cfa1c){if(width!=4)throw MissionTickError("Actor count width is invalid");const auto n=state.tables.groups[at==0x8cf904?1:at==0x8cfa18?3:0].size();if(n>std::size_t(std::numeric_limits<std::int32_t>::max()))throw MissionTickError("Actor count exceeds signed range");return U(n);}
        if(at>=0x793680&&std::uint64_t(at-0x793680)+width<=types.bytes.size()){U n=0;for(U i=0;i<width;++i)n|=U(types.bytes[at-0x793680+i])<<(8*i);return n;}
        for(const auto group:{1u,3u,0u}){const auto base=mission_actor_source_address(std::uint8_t(group),0);if(at<base)continue;const auto delta=at-base,slot=delta/0x114,offset=delta%0x114;if(slot>=state.tables.groups[group].size()||offset+width>0x114)continue;const auto image=snapshot_mission_actor(state,{std::uint8_t(group),slot});U n=0;for(U i=0;i<width;++i)n|=U(image[offset+i])<<(8*i);return n;}
        throw MissionTickError("Type-count read has no current memory owner "+std::to_string(at));
    };
    b.write=[&types](U at,U width,U n){if(width!=4||at<0x793680||std::uint64_t(at-0x793680)+width>types.bytes.size())throw MissionTickError("Type-count write has no current table owner");for(U i=0;i<width;++i)types.bytes[at-0x793680+i]=std::uint8_t(n>>(8*i));};
    return b;
}
}
