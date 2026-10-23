#include "dfre/mission_actor_insert_live.hpp"
#include "dfre/mission_live.hpp"
#include <cstring>
#include <utility>

namespace dfre {
namespace {
using U=std::uint32_t;
constexpr U base=0x822e80,bytes=0x45000,count_at=0x8cfa14;
template<class F> const F& need(const F& f,const char* error){if(!f)throw MissionTickError(error);return f;}
void width(U at,U n){if((n!=1&&n!=2&&n!=4)||std::uint64_t(at)+n>0x100000000ull)throw MissionTickError("Live insertion memory range is invalid");}
U word(const AiActorImage& image,U at){U value;std::memcpy(&value,image.data()+at,4);return value;}
void put(AiActorImage& image,U at,U value){std::memcpy(image.data()+at,&value,4);}
void ready(const MissionTickState& state){if(state.tables.extra.size()!=1024||state.stored_extra_actor_images.size()!=1024||state.extra_actor_encoding.size()!=1024||state.actor_extras[4].size()!=1024||state.runtimes[4].size()!=1024)throw MissionTickError("Live insertion needs the canonical1024 physical extra records");}
AiActorImage raw_image(const MissionTickState& state,std::size_t index){
    auto image=snapshot_mission_actor(state,{kAiExtraTableGroup,index});
    put(image,4,state.extra_actor_encoding.at(index).raw_definition);
    const auto& actor=state.tables.extra.at(index);
    if(actor.person_slot){if(*actor.person_slot>=state.people.size())throw MissionTickError("Live insertion person exceeds its canonical pool");put(image,0xc,0x794680u+U(*actor.person_slot)*0xb4u);}
    else put(image,0xc,0);
    return image;
}
void store_image(MissionTickState& state,std::size_t index,AiActorImage image,
    bool definition_store,const std::function<U(U)>& resolver){
    const auto raw_definition=word(image,4),raw_person=word(image,0xc);
    const auto previous_definition=word(state.stored_extra_actor_images.at(index),4);
    U normalized_definition=previous_definition;
    if(definition_store){
        if(!raw_definition){if(word(image,0))throw MissionTickError("Active extra actor has a null definition pointer");normalized_definition=0;}
        else normalized_definition=need(resolver,"Extra actor definition pointer resolver is not bound")(raw_definition);
        image=raw_image(state,index);put(image,4,raw_definition);
        if(normalized_definition!=word(image,0))throw MissionTickError("Extra actor raw definition differs from its canonical binding");
    }
    put(image,4,word(image,0));
    const auto& actor=state.tables.extra.at(index);
    if(raw_person){
        if(raw_person<0x794680||(raw_person-0x794680)%0xb4)throw MissionTickError("Live insertion person pointer is outside its source pool");
        const auto slot=(raw_person-0x794680)/0xb4;
        if(slot>=state.people.size()||actor.person_slot!=std::optional<std::size_t>(slot))throw MissionTickError("Live insertion needs an explicit current person binding");
        put(image,0xc,state.extra_actor_encoding.at(index).person==ActorPersonEncoding::handle?person_handle(slot):slot);
    }
    apply_mission_actor_image(state,{kAiExtraTableGroup,index},image);
    put(state.stored_extra_actor_images.at(index),4,normalized_definition);
    state.extra_actor_encoding.at(index).raw_definition=raw_definition;
}
}
std::optional<U> mission_extra_actor_record_base(U at,U n){
    width(at,n);
    if(at<base||at>=base+bytes){if(at<base+bytes&&std::uint64_t(at)+n>base)throw MissionTickError("Extra actor range crosses its physical pool");return std::nullopt;}
    const U offset=at-base,record=base+(offset/0x114)*0x114;
    if(offset%0x114+n>0x114)throw MissionTickError("Extra actor range crosses its physical record");
    return record;
}
void prepare_mission_extra_actor_pool(MissionTickState& state){
    state.tables.extra.assign(1024,AiActorState{});
    state.stored_extra_actor_images.assign(1024,AiActorImage{});
    state.extra_actor_encoding.assign(1024,MissionExtraActorEncoding{});
    state.actor_extras[4].assign(1024,BehaviorActorExtra{});
    state.runtimes[4].assign(1024,std::nullopt);
}
MissionActorInsertBindings mission_actor_insert_live_bindings(MissionTickState& state,U& count,MissionActorInsertBindings delegated,std::function<U(U)> resolver){
    ready(state);MissionActorInsertBindings result;
    result.read=[&state,&count,foreign=std::move(delegated.read)](U at,U n){width(at,n);
        if(at>=base&&std::uint64_t(at)+n<=std::uint64_t(base)+bytes){ready(state);U value=0;std::size_t previous=1024;AiActorImage image{};for(U i=0;i<n;++i){const auto offset=at-base+i,index=offset/0x114;if(index!=previous){image=raw_image(state,index);previous=index;}value|=U(image[offset%0x114])<<(i*8);}return value;}
        if(at>=count_at&&std::uint64_t(at)+n<=std::uint64_t(count_at)+4)return (count>>((at-count_at)*8))&(n==4?0xffffffffu:((1u<<(n*8))-1));
        if((at<base+bytes&&std::uint64_t(at)+n>base)||(at<count_at+4&&std::uint64_t(at)+n>count_at))throw MissionTickError("Live insertion read crosses its memory owner");
        return need(foreign,"Live insertion delegated read is not bound")(at,n);};
    result.write=[&state,&count,foreign=std::move(delegated.write),resolver=std::move(resolver)](U at,U n,U value){width(at,n);
        if(at>=base&&std::uint64_t(at)+n<=std::uint64_t(base)+bytes){ready(state);U used=0;while(used<n){const auto offset=at-base+used,index=offset/0x114;auto image=raw_image(state,index);U length=0;while(used+length<n&&(offset+length)/0x114==index){image[(offset+length)%0x114]=std::uint8_t(value>>((used+length)*8));++length;}const U within=offset%0x114;const bool definition_store=within<8&&within+length>4;if(definition_store&&(within!=4||length!=4))throw MissionTickError("Partial extra actor definition pointer stores need an explicit binding");store_image(state,index,image,definition_store,resolver);used+=length;}return;}
        if(at>=count_at&&std::uint64_t(at)+n<=std::uint64_t(count_at)+4){const U shift=(at-count_at)*8,mask=n==4?0xffffffffu:((1u<<(n*8))-1);count=(count&~(mask<<shift))|((value&mask)<<shift);return;}
        if((at<base+bytes&&std::uint64_t(at)+n>base)||(at<count_at+4&&std::uint64_t(at)+n>count_at))throw MissionTickError("Live insertion store crosses its memory owner");
        need(foreign,"Live insertion delegated store is not bound")(at,n,value);};
    result.person=std::move(delegated.person);return result;
}
} // namespace dfre
