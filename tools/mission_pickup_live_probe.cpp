#include "dfre/mission_pickup_live.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace dfre;
namespace {
std::uint32_t word(const AiActorImage& a,unsigned at){return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void put(AiActorImage& a,unsigned at,std::uint32_t n){for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(i*8));}
std::uint32_t next(std::istream& in){std::uint64_t n;if(!(in>>n))throw std::invalid_argument("Short pickup live row");return std::uint32_t(n);}
std::string hex(const std::vector<std::uint8_t>& bytes){std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto n:bytes)s<<std::setw(2)<<unsigned(n);return s.str();}
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    try{
        const auto site=std::stoul(argv[1],nullptr,16);std::string line;
        while(std::getline(std::cin,line)){
            if(line.empty())continue;std::istringstream in(line);std::array<std::uint32_t,4> pose{},home{};for(auto& n:pose)n=next(in);for(auto& n:home)n=next(in);
            const auto health=next(in),rules=next(in),flags=next(in),host=next(in),owner=next(in),slot=next(in),mutation=next(in),item=next(in);
            if(slot>=256)throw std::invalid_argument("Pickup live probe slot exceeds its fixture");
            AiActorTables tables;for(auto group:{0u,1u,2u,3u})tables.groups[group].resize(slot+1);
            for(auto group:{0u,1u,2u,3u}){auto& a=tables.groups[group][slot];a.active=true;a.definition_index=1;a.field_6c=std::uint8_t(flags);for(unsigned i=0;i<3;++i)a.position[i]=pose[i];a.angle_yaw=pose[3];}
            tables.groups[3][slot].person_slot=0;std::vector<AiPersonState> people(1);for(unsigned i=0;i<3;++i)people[0].position[i]=home[i];people[0].angle_yaw=home[3];
            std::vector<AiItemDefinition> defs(2);defs[1].id=item;defs[1].field_94=std::uint16_t(health);
            MissionTickState live;init_mission_tick(live,{},std::move(tables),AiItemDefinitionTable(defs),std::move(people),0);live.globals.mission_67f528=host;live.config.local_actor=slot;live.extra_for({3,slot}).field_74=0x1234;
            for(auto group:{0u,1u,2u,3u}){auto image=snapshot_mission_actor(live,{std::uint8_t(group),slot});put(image,0x6c,flags);apply_mission_actor_image(live,{std::uint8_t(group),slot},image);}
            ActorPickupState shared;shared.rules_67f549=std::uint8_t(rules);const auto target_group=std::uint8_t(rules&3);unsigned sounds=0,local_flags=0;std::string packet="-";MissionPickupLiveBindings b;
            b.sound=[&](auto& state,auto&,auto id){if(id!=0x165)throw std::runtime_error("Wrong pickup sound");++sounds;if(mutation&1){++state.extra_for({3,slot}).field_74;state.tables.groups[3][slot].position[0]+=7;}return 0u;};
            b.allocate=[&](auto& state,auto&,auto owner_id,auto)->std::optional<AiActorRef>{if(owner_id!=(owner?actor_handle(3,slot):0))throw std::runtime_error("Wrong pickup owner");if(mutation&2)return std::nullopt;if(mutation&4){auto image=snapshot_mission_actor(state,{0,slot});put(image,0x6c,0x87654321);apply_mission_actor_image(state,{0,slot},image);}return AiActorRef{0,slot};};
            b.message=[&](auto& state,auto&,auto transport,auto id,const auto& bytes){if(site==0x460470){if(transport!=ActorPickupTransport::broadcast||id!=0x20||bytes!=std::vector<std::uint8_t>{0x14,std::uint8_t(slot),0})throw std::runtime_error("Wrong pickup flag packet");if(state.runtime_for({3,slot}).link_90!=mission_actor_source_address(target_group,slot))throw std::runtime_error("Pickup link was not flushed as a source address");if(mutation&1)state.tables.groups[3][slot].position[0]+=7;if(mutation&2)state.config.local_actor.reset();}else if(transport!=ActorPickupTransport::reliable||id!=0x2b)throw std::runtime_error("Wrong pickup packet");packet=hex(bytes);};
            b.local_flag=[&](auto& state,auto&){++local_flags;if(mutation&4)state.runtime_for({3,slot}).link_90=0;};
            auto provider=mission_pickup_live_effect(shared,b);ActorCollisionCall call{};
            if(site==0x446f60){call.site=ActorCollisionSite::heal;call.count=1;call.arguments[0]=actor_handle(3,slot);}
            else if(site==0x460470){call.site=ActorCollisionSite::pickup;call.count=2;call.arguments={actor_handle(3,slot),actor_handle(target_group,slot),0,0};}
            else if(site==0x4605d0){call.site=ActorCollisionSite::can_use;call.count=1;call.arguments[0]=actor_handle(3,slot);}
            else if(site==0x447610){call.site=ActorCollisionSite::objective_effect;call.count=4;call.arguments={owner?actor_handle(3,slot):0,1,2,3};}
            else throw std::invalid_argument("Unknown pickup live probe site");
            const auto result=provider(live,call).value();const auto image=snapshot_mission_actor(live,{3,slot});
            std::cout<<result;for(auto at:{0x24u,0x28u,0x2cu,0x30u,0x6cu})std::cout<<' '<<word(image,at);
            std::cout<<' '<<(word(image,0x74)&0xffff)<<' '<<word(snapshot_mission_actor(live,{0,slot}),0x6c)<<' '<<sounds<<' '<<word(image,0x90)<<' '<<word(snapshot_mission_actor(live,{target_group,slot}),0x6c)<<' '<<local_flags<<' '<<packet<<'\n';
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
