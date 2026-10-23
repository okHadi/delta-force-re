#include "dfre/mission_event_actor_orders.hpp"
#include <cstring>
#include <limits>
namespace dfre { namespace {
using U=std::uint32_t;
constexpr U persons=0x794680,lists=0x78f280;
std::int32_t si(U n){std::int32_t v;std::memcpy(&v,&n,4);return v;}
U read(const MissionEventActorOrderBindings& b,U address){if(!b.read)throw MissionTickError("Actor orders need current memory reads");return b.read(address,4);}
void write(const MissionEventActorOrderBindings& b,U address,U value){if(!b.write)throw MissionTickError("Actor orders need current memory writes");b.write(address,4,value);}
U difference(U a,U b){return U(si(a-b)>>16);}
void width(U n){if(n!=1&&n!=2&&n!=4)throw MissionTickError("Actor-order memory width is invalid");}
}
U mission_event_closest_marker(const MissionEventActorOrderBindings& b,U actor,U list){
    U selected=0,best=0x7fffffff;
    if(!read(b,actor+0x0c))return 0;
    const auto table=list*0x88u;
    if(si(read(b,table+lists+4))<=0)return 0;
    U entry=0,cursor=table;
    for(;;){
        const auto marker=read(b,cursor+lists+8)*0x114u+0x8ace80u;
        const auto actor_x=read(b,actor+0x24),marker_x=read(b,marker+0x24);
        const auto x=difference(actor_x,marker_x);
        const auto marker_y=read(b,marker+0x28),actor_y=read(b,actor+0x28);
        const auto y=difference(actor_y,marker_y);
        const auto actor_z=read(b,actor+0x2c),marker_z=read(b,marker+0x2c);
        const auto z=difference(actor_z,marker_z),score=x*x+y*y+z*z;
        if(si(score)<=si(best)){selected=entry;best=score;}
        ++entry;const auto count=read(b,table+lists+4);cursor+=4;
        if(si(entry)>=si(count))return selected;
    }
}
void mission_event_actor_orders(const MissionEventActorOrderBindings& b,std::int32_t side,U list){
    if(side<3)return;
    for(const auto group:{3u,0u}){
        const auto count=group==3?0x8cfa18u:0x8cfa1cu;
        if(si(read(b,count))<=0)continue;
        U index=0,actor=group==3?0x7cca80u:0x7dde80u;
        for(;;){
            if((si(read(b,actor+0x70))>>16)==side){
                const auto person=read(b,actor+0x0c);
                if(person){
                    write(b,person+0xa4,1);
                    write(b,read(b,actor+0x0c)+0xac,list);
                    const auto selected=mission_event_closest_marker(b,actor,list);
                    write(b,read(b,actor+0x0c)+0xb0,selected);
                }
            }
            const auto bound=read(b,count);++index;actor+=0x114;
            if(si(index)>=si(bound))break;
        }
    }
}
MissionEventActorOrderBindings mission_event_actor_order_bindings(MissionTickState& state){
    MissionEventActorOrderBindings b;
    b.read=[&state](U address,U n){
        width(n);
        if(address==0x8cfa18||address==0x8cfa1c){if(n!=4)throw MissionTickError("Actor count width is invalid");const auto count=state.tables.groups[address==0x8cfa18?3:0].size();if(count>std::size_t(std::numeric_limits<std::int32_t>::max()))throw MissionTickError("Actor count exceeds signed range");return U(count);}
        for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(address<base)continue;const auto delta=address-base,slot=delta/0x114,offset=delta%0x114;if(slot>=state.tables.groups[group].size()||offset+n>0x114)continue;
            auto image=snapshot_mission_actor(state,{group,slot});const auto& actor=state.tables.groups[group][slot];U pointer=0;
            if(offset<0x10&&offset+n>0xc){if(actor.person_slot){if(*actor.person_slot>=state.people.size())throw MissionTickError("Actor person exceeds its pool");pointer=persons+U(*actor.person_slot)*0xb4u;}std::memcpy(image.data()+0xc,&pointer,4);}
            U value=0;for(U i=0;i<n;++i)value|=U(image[offset+i])<<(8*i);return value;
        }
        if(address>=lists&&std::uint64_t(address-lists)+n<=state.world.behavior_world.lists.size()*0x88ull){U value=0;for(U i=0;i<n;++i){const auto delta=address-lists+i;const auto& row=state.world.behavior_world.lists[delta/0x88];const auto offset=delta%0x88,word=offset/4;const auto cell=word==0?row.flags:word==1?row.count:row.indices[word-2];value|=((cell>>((offset%4)*8))&255u)<<(i*8);}return value;}
        throw MissionTickError("Actor orders read has no current memory owner "+std::to_string(address));
    };
    b.write=[&state](U address,U n,U value){
        if(n!=4||address<persons)throw MissionTickError("Actor orders write has no current person owner");const auto delta=address-persons,slot=delta/0xb4,offset=delta%0xb4;
        if(slot>=state.people.size())throw MissionTickError("Actor orders person write exceeds its pool");
        if(offset==0xa4){state.person_extra(slot).field_a4=value;state.people[slot].tail_flag=value!=0;}
        else if(offset==0xac)state.people[slot].field_ac=value;
        else if(offset==0xb0)state.people[slot].field_b0=value;
        else throw MissionTickError("Actor orders person field has no owner");
    };
    return b;
}
}
