#include "dfre/mission_actor_insert.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>

namespace dfre {
namespace {
using U=std::uint32_t;
template<class F> const F& need(const F& f,const char* message){if(!f)throw std::runtime_error(message);return f;}
std::int32_t signed_word(U value){std::int32_t result;std::memcpy(&result,&value,4);return result;}
void width(U at,U n){if((n!=1&&n!=2&&n!=4)||std::uint64_t(at)+n>0x100000000ull)throw std::runtime_error("Actor insertion memory range is invalid");}
}
U mission_actor_insert(const MissionActorInsertBindings& b,U source,U mode){
    const auto read=[&](U at){return need(b.read,"Actor insertion memory read is not bound")(at,4);};
    const auto write=[&](U at,U value){need(b.write,"Actor insertion memory store is not bound")(at,4,value);};
    if(read(source)==0)return 0;
    if(mode==0){const auto definition=read(source+4);if(read(definition+0x28)!=6)return 0;}
    const U base=mode?0x822e80u:0x7dde80u;
    U destination=base,index=0;
    while(read(destination)!=0){++index;destination+=0x114;if(index>=0x400)return 0;}
    for(U offset=0;offset<0x114;offset+=4){const auto value=read(source+offset);write(destination+offset,value);}
    const U extent=index+1,count_address=mode?0x8cfa14u:0x8cfa1cu;
    const auto count=read(count_address);
    if(signed_word(extent)>signed_word(count))write(count_address,extent);
    if(mode==0){const auto definition=read(destination+4);const auto flags=need(b.read,"Actor insertion memory read is not bound")(definition+0x24,1);
        if(flags&4){const auto person=need(b.person,"Actor insertion person allocator is not bound")(destination);write(destination+0xc,person);if(!person)return 0;}}
    return destination;
}
MissionActorInsertBindings mission_transient_actor_bindings(MissionTransientActorPool& pool,MissionActorInsertBindings delegated){
    static constexpr U base=0x822e80,bytes=0x45000,count=0x8cfa14;
    MissionActorInsertBindings result;
    result.read=[&pool,foreign=std::move(delegated.read)](U at,U n){width(at,n);U value=0;
        if(at>=base&&std::uint64_t(at)+n<=std::uint64_t(base)+bytes){for(U i=0;i<n;++i){const U offset=at-base+i;value|=U(pool.records[offset/0x114][offset%0x114])<<(i*8);}return value;}
        if(at>=count&&std::uint64_t(at)+n<=std::uint64_t(count)+4){for(U i=0;i<n;++i)value|=((pool.count_8cfa14>>((at-count+i)*8))&255u)<<(i*8);return value;}
        if((at<base+bytes&&std::uint64_t(at)+n>base)||(at<count+4&&std::uint64_t(at)+n>count))throw std::runtime_error("Actor insertion read crosses its owner");
        return need(foreign,"Actor insertion delegated memory read is not bound")(at,n);};
    result.write=[&pool,foreign=std::move(delegated.write)](U at,U n,U value){width(at,n);
        if(at>=base&&std::uint64_t(at)+n<=std::uint64_t(base)+bytes){for(U i=0;i<n;++i){const U offset=at-base+i;pool.records[offset/0x114][offset%0x114]=std::uint8_t(value>>(i*8));}return;}
        if(at>=count&&std::uint64_t(at)+n<=std::uint64_t(count)+4){for(U i=0;i<n;++i){const U shift=(at-count+i)*8;pool.count_8cfa14=(pool.count_8cfa14&~(255u<<shift))|(((value>>(i*8))&255u)<<shift);}return;}
        if((at<base+bytes&&std::uint64_t(at)+n>base)||(at<count+4&&std::uint64_t(at)+n>count))throw std::runtime_error("Actor insertion store crosses its owner");
        need(foreign,"Actor insertion delegated memory store is not bound")(at,n,value);};
    result.person=std::move(delegated.person);return result;
}
void mission_transient_actor_clear(MissionTransientActorPool& pool){for(auto& record:pool.records)record.fill(0);}
} // namespace dfre
