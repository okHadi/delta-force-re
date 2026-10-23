#include "dfre/mission_crt_thread.hpp"
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;
void width(U n){if(n!=1&&n!=2&&n!=4)throw MissionSoundAssetError("CRT thread width is invalid");}
void range(U offset,U n){width(n);if(offset<12||std::uint64_t(offset)+n>20)throw MissionSoundAssetError("CRT thread field has no owner");}
U byte(const MissionCrtThreadCells& c,U offset){return ((offset<4?c.seed:c.tokenizer_cursor)>>((offset%4)*8))&255u;}
}
MissionSoundRuntimeBindings mission_crt_thread_bindings(MissionSoundRuntimeBindings b,
    MissionCrtThreadCells& cells,U source){
    if(!source||std::uint64_t(source)+20>0x100000000ULL)throw MissionSoundAssetError("CRT thread source is invalid");
    const auto base=b.record_base;const auto read=b.record_read;const auto store=b.record_store;
    b.record_base=[base,source](U at,U n)->std::optional<U>{
        width(n);const auto end=std::uint64_t(at)+n;
        if(end>0x100000000ULL)throw MissionSoundAssetError("CRT thread access wraps");
        if(std::uint64_t(at)<std::uint64_t(source)+20&&end>std::uint64_t(source)+12){
            if(at<source)throw MissionSoundAssetError("CRT thread access starts outside its field");range(at-source,n);return source;
        }
        return base?base(at,n):std::nullopt;
    };
    b.record_read=[read,source,&cells](U at,U offset,U n){
        if(at!=source){if(!read)throw MissionSoundAssetError("Current record reader is not bound");return read(at,offset,n);}
        range(offset,n);U value=0;for(U i=0;i<n;++i)value|=byte(cells,offset-12+i)<<(i*8);return value;
    };
    b.record_store=[store,source,&cells](U at,U offset,U n,U value){
        if(at!=source){if(!store)throw MissionSoundAssetError("Current record writer is not bound");store(at,offset,n,value);return;}
        range(offset,n);for(U i=0;i<n;++i){const auto key=offset-12+i;auto& cell=key<4?cells.seed:cells.tokenizer_cursor;const auto shift=(key%4)*8;cell=(cell&~(255u<<shift))|(((value>>(i*8))&255u)<<shift);}
    };
    return b;
}
}
