#include "dfre/mission_projectile_stage.hpp"
#include "dfre/actor_queries.hpp"
#include "dfre/world_hits.hpp"
#include <cstring>
#include <stdexcept>

namespace dfre {
namespace {
std::int32_t si(std::uint32_t x) {std::int32_t r;std::memcpy(&r,&x,4);return r;}
std::int32_t add(std::int32_t a,std::int32_t b) {return si(std::uint32_t(a)+std::uint32_t(b));}
std::int32_t sub(std::int32_t a,std::int32_t b) {return si(std::uint32_t(a)-std::uint32_t(b));}
std::int32_t absolute(std::int32_t a) {return a<0?sub(0,a):a;}
std::uint32_t word(const AiActorImage& a,unsigned at) {return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void put(AiActorImage& a,unsigned at,std::uint32_t n) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(8*i));}
template<class T> const T& need(const T& fn) {if(!fn)throw std::invalid_argument("Mission projectile stage needs a binding");return fn;}
AiActorImage& image(MissionAreaActor actor) {if(!actor.image)throw std::invalid_argument("Mission projectile stage needs an actor image");return *actor.image;}
MissionPendingSpawn& record(MissionProjectileStageState& s,std::size_t i) {if(!s.pending||i>=s.pending->size())throw std::invalid_argument("Mission area record outside queue");return (*s.pending)[i];}
MissionAreaActor actor(MissionAreaActorRef ref,const MissionProjectileStageBindings& b) {return need(b.actor)(ref);}
Fixed3 position(const AiActorImage& a) {return {si(word(a,0x24)),si(word(a,0x28)),si(word(a,0x2c))};}
void health(AiActorImage& a,std::int32_t damage) {auto n=std::uint16_t(word(a,0x74)-std::uint32_t(damage));a[0x74]=std::uint8_t(n);a[0x75]=std::uint8_t(n>>8);}
MissionAreaDefinition definition(MissionAreaActorRef ref,const MissionProjectileStageBindings& b) {return need(b.definition)(word(image(actor(ref,b)),4));}
std::uint32_t direction(MissionAreaActorRef ref,std::size_t index,MissionProjectileStageState& s,const MissionProjectileStageBindings& b) {
    const auto p=position(image(actor(ref,b)));const auto r=record(s,index);
    const auto angle=query_angle_477770(sub(r.position[0],p.x),sub(r.position[1],p.y));
    return (angle-word(image(actor(ref,b)),0x30)+0x1fffffffu)>>30;
}
void animation(MissionAreaActorRef ref,std::uint32_t kind,std::uint32_t facing,std::uint32_t mode,
    MissionProjectileStageState& s,const MissionProjectileStageBindings& b) {
    auto& a=image(actor(ref,b));AiActorState view;view.position[2]=word(a,0x2c);
    put(a,0x84,query_anim_id_460410(view,kind,facing,mode,s.water));
}
void update(MissionAreaActorRef ref,std::uint32_t message,const MissionProjectileStageBindings& b) {
    const auto callback=word(image(actor(ref,b)),0x110);need(b.update)(ref,callback,message);
}
std::int32_t approximate(const std::array<std::int32_t,3>& d) {return collision_approximate_length(d[0],d[1],d[2]);}
}

void mission_listener_stage(MissionProjectileStageState& s,const MissionProjectileStageBindings& b) {
    if(s.alternate_listener) {need(b.listener)(s.alternate_pose);return;}
    if(!s.local_player.image)return;
    const auto& a=image(s.local_player);need(b.listener)({position(a),word(a,0x30),word(a,0x34)});
}

void mission_area_apply(MissionAreaSite site,MissionAreaActorRef ref,std::size_t index,std::int32_t distance,
    MissionProjectileStageState& s,const MissionProjectileStageBindings& b) {
    const auto def=definition(ref,b);
    if(site==MissionAreaSite::blast) {
        auto start=position(image(actor(ref,b)));const auto initial=record(s,index);
        const Fixed3 end{initial.position[0],initial.position[1],add(initial.position[2],0x8000)};
        const bool person=def.type==3;
        if(person&&(image(actor(ref,b))[0x6c]&4))return;
        std::int32_t damage=person?0:sub(initial.type==3||initial.type==5?0x80:0x20,
            distance>>(initial.type==3||initial.type==5?12:14));
        bool clear=need(b.segment)(ref,start,end)>0;
        if(!clear) {start.z=add(start.z,0x8000);clear=need(b.segment)(ref,start,end)>0;}
        if(!clear) {
            if(record(s,index).type==1)return;
            if(person)distance=add(distance,distance);else damage>>=1;
        }
        if(person) {
            const auto facing=direction(ref,index,s,b);
            std::uint32_t mode=2;
            if(distance<0x40000)damage=4;
            else if(distance<0x80000) {
                if(!s.random)throw std::invalid_argument("Mission area blast needs shared RNG");
                mode=behavior_rng_next(*s.random)<0x4000?3:2;damage=2;
            } else damage=1;
            animation(ref,1,facing,mode,s,b);
            auto& a=image(actor(ref,b));put(a,0xf8,record(s,index).owner);health(a,damage);a[0x6d]|=4;
            update(ref,2,b);
            if(actor(ref,b).identity==s.local_player_identity)need(b.local_hit)();
        } else {
            if(damage<0)damage=0;
            auto& a=image(actor(ref,b));put(a,0xf8,record(s,index).owner);health(a,damage);update(ref,2,b);
        }
        return;
    }
    if(site==MissionAreaSite::shock) {
        if(def.type==3) {
            if((image(actor(ref,b))[0x6c]&4)||actor(ref,b).identity==record(s,index).owner)return;
            const auto facing=direction(ref,index,s,b);auto& a=image(actor(ref,b));
            put(a,0xf8,record(s,index).owner);health(a,8);animation(ref,2,facing,1,s,b);
            put(a,0x6c,word(a,0x6c)|0x10400);update(ref,3,b);
        } else if(def.id==0xfd0) {
            auto& a=image(actor(ref,b));put(a,0xf8,record(s,index).owner);health(a,8);a[0x6e]|=1;update(ref,3,b);
        }
        return;
    }
    if(site==MissionAreaSite::burn) {
        if(def.type!=3||(image(actor(ref,b))[0x6c]&4)||actor(ref,b).identity==record(s,index).owner)return;
        const auto facing=direction(ref,index,s,b);auto& a=image(actor(ref,b));
        put(a,0xf8,record(s,index).owner);health(a,1);animation(ref,2,facing,1,s,b);
        put(a,0x6c,word(a,0x6c)|0x100400);update(ref,5,b);return;
    }
    throw std::invalid_argument("Mission area helper outside supported sites");
}

void mission_pending_area_stage(MissionProjectileStageState& s,const MissionProjectileStageBindings& b) {
    if(!s.host)return;
    if(!s.pending)throw std::invalid_argument("Mission projectile stage needs its queue");
    auto dispatch=s.incoming_dispatch;
    for(std::size_t index=0;index<s.pending->size();++index) {
        switch(record(s,index).type) {
        case 0:dispatch=MissionAreaDispatch{0,MissionAreaSite::blast};continue;
        case 1:case 3:dispatch=MissionAreaDispatch{0xa0000,MissionAreaSite::blast};break;
        case 2:dispatch=MissionAreaDispatch{0x10000,MissionAreaSite::shock};break;
        case 4:dispatch=MissionAreaDispatch{0x10000,MissionAreaSite::burn};break;
        case 5:dispatch=MissionAreaDispatch{0xc0000,MissionAreaSite::blast};break;
        default:
            if(!dispatch)throw std::invalid_argument("Mission area dispatch needs incoming EDI and callback");
            if(!dispatch->range)continue;
        }
        for(auto group:{3u,1u,0u}) {
            for(std::size_t slot=0;slot<need(b.count)(group);++slot) {
                const MissionAreaActorRef ref{group,slot};const auto target=actor(ref,b);
                const auto& a=image(target);if(!word(a,0))continue;
                const auto r=record(s,index);const auto p=position(a);
                std::array<std::int32_t,3> delta{};std::int32_t range=dispatch->range;
                if(group==3) {
                    if((s.rules_67f549&0x40)&&r.owner) {
                        const auto& owner=image(need(b.owner)(r.owner));
                        if((word(a,0x70)&0xffff)==(word(owner,0x70)&0xffff)&&target.identity!=r.owner&&!(a[0x6c]&0x80))continue;
                    }
                    range=add(range,0x10000);
                    delta={absolute(sub(r.position[0],p.x)),absolute(sub(r.position[1],p.y)),absolute(sub(r.position[2],p.z))};
                    if(delta[0]>range||delta[1]>range||delta[2]>range||word(a,0x78))continue;
                } else {
                    const auto graphic=word(a,0x10);if(!graphic)continue;
                    const auto bounds=need(b.bounds)(graphic);
                    if(group==1) {
                        range=add(range,bounds.radius_64);
                        delta={absolute(sub(r.position[0],p.x)),absolute(sub(r.position[1],p.y)),absolute(sub(r.position[2],p.z))};
                    } else {
                        const std::array<std::int32_t,3> position_array{p.x,p.y,p.z};
                        for(unsigned axis=0;axis<3;++axis) {
                            const auto extent=axis==2?bounds.height_68:bounds.radius_64;
                            const auto upper=add(position_array[axis],extent),lower=sub(position_array[axis],extent);
                            if(upper<r.position[axis])delta[axis]=sub(r.position[axis],upper);
                            if(r.position[axis]<lower)delta[axis]=sub(lower,r.position[axis]);
                        }
                    }
                    if(delta[0]>range||delta[1]>range||delta[2]>range)continue;
                }
                const auto distance=approximate(delta);
                if(distance<=range)mission_area_apply(dispatch->site,ref,index,distance,s,b);
            }
        }
    }
    s.pending->clear();
}
} // namespace dfre
