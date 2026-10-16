#include "dfre/actor_queries.hpp"
#include "dfre/world_hits.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace dfre {
namespace {
std::int32_t bits(std::uint32_t value){std::int32_t out;std::memcpy(&out,&value,4);return out;}
std::int32_t difference(std::uint32_t a,std::uint32_t b){return bits(a-b);}
std::int32_t wrapped_abs(std::int32_t value){const auto sign=std::uint32_t(value)>>31;return bits((std::uint32_t(value)^(0u-sign))+sign);}
AiActorState& actor_at(ActorQueryTable& table,std::size_t index){
    if(!table.actors || index>=table.actors->size())throw ActorQueryError("actor query index exceeds the table");
    return table.actors->at(index);
}
bool contains(const ActorZoneRect& zone,const AiActorState& actor){
    const auto x=bits(actor.position[0]),y=bits(actor.position[1]),z=bits(actor.position[2]);
    return zone.x_min<=x && x<=zone.x_max && zone.y_min<=y && y<=zone.y_max && zone.z_min<=z && z<=zone.z_max;
}
std::int32_t midpoint(std::int32_t first,std::int32_t last){return bits(std::uint32_t(first)+std::uint32_t(last))>>1;}
void zone_step(ActorQueryTable& table,std::size_t self,const ActorQueryData& data,BehaviorState& state,bool second){
    auto& actor=actor_at(table,self);
    for(const auto& zone:data.zones) {
        if(second && (!(zone.flags&2) || zone.type==std::uint32_t(std::int32_t(std::int16_t(actor.field_70)))))continue;
        if(!contains(zone,actor))continue;
        table.person(self).field_9c=2;
        if(!second)state.emit(BehaviorCallKind::effect,behavior_site::kTypeSlotMark,std::uint32_t(std::int32_t(std::int16_t(actor.field_72))));
        actor.angle_yaw=query_angle_477770(difference(std::uint32_t(midpoint(zone.x_min,zone.x_max)),actor.position[0]),
            difference(std::uint32_t(midpoint(zone.y_min,zone.y_max)),actor.position[1]));
        return;
    }
}
}
AiPersonState& ActorQueryTable::person(std::size_t index) const {
    if(!actors || index>=actors->size() || !actors->at(index).person_slot || !people || *actors->at(index).person_slot>=people->size())
        throw ActorQueryError("actor query needs its person record");
    return people->at(*actors->at(index).person_slot);
}
BehaviorPersonExtra& ActorQueryTable::person_extra(std::size_t index) const {
    if(!actors || index>=actors->size() || !actors->at(index).person_slot || !person_extras || *actors->at(index).person_slot>=person_extras->size())
        throw ActorQueryError("actor query needs its person extra record");
    return person_extras->at(*actors->at(index).person_slot);
}
#if defined(_MSC_VER) && defined(_M_X64)
extern "C" std::int64_t dfre_actor_query_angle_x87(std::int32_t,std::int32_t);
#endif
std::uint32_t query_angle_477770(std::int32_t dx,std::int32_t dy){
    const auto ax=wrapped_abs(dx),ay=wrapped_abs(dy);
    if(ax<(ay>>10))return dy<0?0xc0000000u:0x40000000u;
    if(ay<(ax>>10))return dx<0?0x80000000u:0;
    std::int64_t result;
#if defined(_MSC_VER) && defined(_M_X64)
    result=dfre_actor_query_angle_x87(dx,dy);
#elif defined(__i386__) || defined(__x86_64__)
    const double scale=1.52587890625e-05,pi=3.1415926539,half=0.5,turn=4294967295.0;
    std::uint16_t previous;
    __asm__ volatile("fnstcw %0":"=m"(previous));
    const auto calculate=std::uint16_t((previous&0xf0ffu)|0x0300u);
    const auto truncate=std::uint16_t(calculate|0x0c00u);
    __asm__ volatile(
        "fldcw %[calculate]\n\t"
        "fildl %[dx]\n\tfmull %[scale]\n\t"
        "fildl %[dy]\n\tfmull %[scale]\n\t"
        ".byte 0xde,0xf1\n\t" // Original FDIVRP ST(1).
        "fld1\n\tfpatan\n\tfdivl %[pi]\n\tfmull %[half]\n\tfmull %[turn]\n\t"
        "fldcw %[truncate]\n\tfrndint\n\tfldcw %[calculate]\n\tfistpll %[result]\n\t"
        "fldcw %[previous]"
        :[result] "=m"(result)
        :[dx] "m"(dx),[dy] "m"(dy),[scale] "m"(scale),[pi] "m"(pi),[half] "m"(half),[turn] "m"(turn),
         [calculate] "m"(calculate),[truncate] "m"(truncate),[previous] "m"(previous)
        :"st","st(1)","memory");
#else
    throw ActorQueryError("exact actor angles need an x87 target");
#endif
    return std::uint32_t(result)+(dx<0?0x80000000u:0u);
}
std::uint32_t query_isqrt_477620(std::uint32_t value){return std::uint32_t(collision_sqrt(bits(value)));}
std::uint32_t query_nearby_45f010(ActorQueryTable& table,std::size_t self){
    const auto& actor=actor_at(table,self);
    for(std::size_t i=0;i<table.size();++i){
        const auto& other=table.actors->at(i);
        if(i==self || other.definition_index==0)continue;
        bool near=true;
        for(std::size_t axis=0;axis<3;++axis)near=near && wrapped_abs(difference(other.position[axis],actor.position[axis]))<=0x20000;
        if(near)return 0;
    }
    return 1;
}
void query_zone_45fd90(ActorQueryTable& table,std::size_t self,const ActorQueryData& data,BehaviorState& state){zone_step(table,self,data,state,false);}
void query_zone2_45fe30(ActorQueryTable& table,std::size_t self,const ActorQueryData& data,BehaviorState& state){zone_step(table,self,data,state,true);}
std::uint32_t query_anim_id_460410(const AiActorState& actor,std::uint32_t p2,std::uint32_t p3,std::uint32_t p4,
    std::int32_t water,std::optional<std::uint32_t> incoming_eax){
    if(water!=0 && bits(actor.position[2])<=water)return 0xfe;
    if(p4==1)return p2*4u+0xbeu+p3;
    if(p4==2)return p3+0xfau;
    if(p4==3)return 0xff;
    if(incoming_eax)return *incoming_eax;
    throw ActorQueryError("animation mode needs the incoming EAX value");
}
namespace {
std::int32_t product(std::int32_t a,std::int32_t b,unsigned shift=16){return bits(std::uint32_t((std::int64_t(a)*b)>>shift));}
std::int32_t add(std::int32_t a,std::int32_t b){return bits(std::uint32_t(a)+std::uint32_t(b));}
std::uint32_t pointer(const ActorQueryData& data,std::size_t index){
    if(!data.raw_pointer)throw ActorQueryError("actor query needs a pointer map");
    return data.raw_pointer(index);
}
std::optional<std::size_t> index_of(const ActorQueryTable& table,const ActorQueryData& data,std::uint32_t raw){
    if(!raw)return std::nullopt;
    for(std::size_t i=0;i<table.size();++i)if(pointer(data,i)==raw)return i;
    throw ActorQueryError("actor query pointer is outside the table");
}
BehaviorActorExtra& extra(ActorQueryTable& table,std::size_t index){
    if(!table.actor_extras || index>=table.actor_extras->size())throw ActorQueryError("actor query needs actor extras");
    return table.actor_extras->at(index);
}
std::int32_t sight(const ActorQueryData& data,std::size_t index){
    if(!data.sight_line)throw ActorQueryError("actor query needs line of sight");
    return data.sight_line(index);
}
void copy_target(BehaviorPersonExtra& dst,const BehaviorPersonExtra& src){
    dst.field_44=src.field_44;dst.field_48=src.field_48;dst.field_4c=src.field_4c;dst.field_50=src.field_50;
}
}
std::uint32_t query_selector_45f090(ActorQueryTable& table,std::size_t self,BehaviorState& state){
    auto& actor=actor_at(table,self);auto& person=table.person(self);auto& own=table.person_extra(self);
    if(person.field_9c!=2 || !own.field_44)return 0;
    std::uint32_t result=0;
    for(std::size_t i=0;i<table.size();++i){
        auto& other=actor_at(table,i);
        if(i==self || !other.definition_index || other.field_70!=actor.field_70 || (other.field_6c&4))continue;
        const auto x=wrapped_abs(difference(other.position[0],actor.position[0]));
        const auto y=wrapped_abs(difference(other.position[1],actor.position[1]));
        if(x>0x320000 || y>0x320000 || add(product(x,x),product(y,y))>0x9c40000)continue;
        auto& peer=table.person(i);auto& target=table.person_extra(i);
        if(target.field_44==own.field_44){
            if(!(person.flags24&0x40) || (peer.flags24&0x40)){
                if(!(person.flags24&0x40) && bits(own.field_30)<bits(target.field_30)){
                    if(difference(target.field_30,own.field_30)>0x1f0)result=3;
                    target.field_30=own.field_30;copy_target(target,own);
                }
            }else{result=2;peer.flags24|=0x40;}
        }else if(!target.field_44){
            if(!(person.flags24&0x40)){
                if(bits(own.field_30)<bits(peer.field_94)){
                    peer.field_9c=2;
                    state.emit(BehaviorCallKind::effect,behavior_site::kTypeSlotMark,std::uint32_t(std::int32_t(std::int16_t(other.field_72))));
                    copy_target(target,own);result=1;
                }
            }else{
                peer.field_9c=2;
                state.emit(BehaviorCallKind::effect,behavior_site::kTypeSlotMark,std::uint32_t(std::int32_t(std::int16_t(other.field_72))));
                peer.flags24|=0x40;copy_target(target,own);result=2;
            }
        }
    }
    return result;
}
std::uint32_t query_facing_45edd0(ActorQueryTable& table,std::size_t self,const ActorQueryData& data,BehaviorState& state){
    auto& actor=actor_at(table,self);auto& person=table.person(self);
    if(!data.sin_cos_477820 || !data.terrain_height)throw ActorQueryError("facing query needs sine and terrain");
    const auto span=person.field_7c;std::uint32_t angle=actor.angle_yaw-span*0x1000000u,step=0xfc000000u;
    if(!(person.flags24&4)){
        if(!(person.flags24&8)){
            if(behavior_rng_next(state)<0x8000)person.flags24|=4;
            else{person.flags24|=8;angle=actor.angle_yaw+span*0x1000000u;step=0x4000000u;}
        }else{angle=actor.angle_yaw+span*0x1000000u;step=0x4000000u;}
    }
    const auto count=(span>>2)+8;const auto z=bits(actor.position[2]);
    for(std::uint32_t n=0;n<count;++n,angle+=step){
        std::int32_t sine,cosine;data.sin_cos_477820(angle,sine,cosine);sine>>=3;cosine>>=3;
        const auto a=data.terrain_height(bits(actor.position[0]+std::uint32_t(cosine)),bits(actor.position[1]+std::uint32_t(sine)));
        const auto b=data.terrain_height(bits(actor.position[0]+std::uint32_t(cosine)*2u),bits(actor.position[1]+std::uint32_t(sine)*2u));
        if((data.water_level==0 || difference(data.water_level,a)<0xc001) && difference(a,z)<0x8001 && difference(b,z)<0x30001 && difference(z,a)<0x10001 && difference(z,b)<0x40001)return angle;
    }
    angle=actor.angle_yaw+0x10000000u;
    for(unsigned n=0;n<128;++n,angle+=step){
        std::int32_t sine,cosine;data.sin_cos_477820(angle,sine,cosine);
        const auto z2=data.terrain_height(bits(actor.position[0]+std::uint32_t(cosine>>3)),bits(actor.position[1]+std::uint32_t(sine>>3)));
        if((data.water_level==0 || difference(data.water_level,z2)<0xc001) && difference(z2,z)<0x20001 && difference(z,z2)<0x40001)return angle;
    }
    return actor.angle_yaw;
}
std::optional<std::size_t> query_target_search_45e280(ActorQueryTable& table,std::size_t self,const ActorQueryData& data,BehaviorState& state,std::uint32_t& distance){
    auto& actor=actor_at(table,self);auto& person=table.person(self);
    const auto cone=(person.field_98>>9)|1u;const auto original_mode=person.field_9c;
    std::optional<std::size_t> best;std::uint32_t score=0;
    for(std::size_t i=0;i<table.size() && i<256;++i){
        auto& other=actor_at(table,i);if(!other.definition_index)continue;
        if(other.field_70==actor.field_70){
            if((other.field_6c&4) && original_mode!=2){
                const auto visible=sight(data,i);
                if(visible>0 && visible<3 && bits(behavior_octagon3(wrapped_abs(difference(other.position[0],actor.position[0])),wrapped_abs(difference(other.position[1],actor.position[1])),0))<0x320000){
                    person.field_9c=2;state.emit(BehaviorCallKind::effect,behavior_site::kTypeSlotMark,std::uint32_t(std::int32_t(std::int16_t(actor.field_72))));
                    state.emit(BehaviorCallKind::effect,behavior_site::kMessage,std::uint32_t(self),1);
                }
            }
            continue;
        }
        if(other.field_70==0 || (other.field_6c&4) || (data.local_player_excluded_67dbc9_bit8 && data.local_player==i))continue;
        const auto dx=difference(other.position[0],actor.position[0]),dy=difference(other.position[1],actor.position[1]),dz=difference(other.position[2],actor.position[2]);
        auto error=(query_angle_477770(dx,dy)-actor.angle_yaw)>>24;
        if(error>0x7f)error=0x100-error;
        if(error>cone)continue;
        const auto length=bits(behavior_octagon3(wrapped_abs(dx),wrapped_abs(dy),wrapped_abs(dz)))>>16;
        const auto inverse=length==0?0x640000:bits(std::uint32_t(std::int64_t(0x640000)/length));
        if(length>=1000)continue;
        actor.position[2]+=0x10000;const auto visible=sight(data,i);actor.position[2]-=0x10000;
        if(visible<=0 || visible>=3)continue;
        if(!data.weapon_frame_10)throw ActorQueryError("target query needs weapon frame +0x10");
        const auto weight=(extra(table,i).field_6d&0x40)?0x60000:0x10000;
        auto next=std::uint32_t(product(product(product(inverse,bits((cone-error)*0x10000u/cone)),data.weapon_frame_10(i)),weight));
        if(data.double_score_67f548_bit80 && actor.field_70==2)next*=2;
        if(bits(score)<bits(next)){score=next;best=i;}
    }
    distance=std::uint32_t(product(bits(score),bits(person.field_74)));
    if(person.field_9c==0)distance=std::uint32_t(bits(distance)>>2);
    return best;
}
std::optional<std::size_t> query_ray_45fb00(ActorQueryTable& table,std::size_t self,const ActorQueryData& data,std::uint32_t range,std::uint32_t radius){
    auto& actor=actor_at(table,self);
    if(!data.sin_cos_4776c0)throw ActorQueryError("ray query needs sine and cosine");
    std::int32_t sy,cy,sp,cp;data.sin_cos_4776c0(actor.angle_yaw,sy,cy);data.sin_cos_4776c0(actor.angle_pitch,sp,cp);
    const auto xdir=product(cp,cy,22)>>6,ydir=product(cp,sy,22)>>6,zdir=sp>>6;
    const auto eye=std::int64_t(bits(actor.position[2]))+49152;
    const auto z=std::uint32_t(eye>std::numeric_limits<std::int32_t>::max()?std::numeric_limits<std::int32_t>::min():std::int32_t(eye));
    for(std::size_t i=0;i<table.size();++i){
        const auto& other=actor_at(table,i);
        if(i==self || !other.definition_index || other.field_70!=actor.field_70 || (other.field_6c&6))continue;
        const auto dx=difference(other.position[0],actor.position[0]),dy=difference(other.position[1],actor.position[1]),dz=difference(other.position[2],z);
        const auto along=add(add(product(dx,xdir),product(dy,ydir)),product(dz,zdir));
        if(along<=0xff || along>bits(range))continue;
        const auto px=wrapped_abs(difference(std::uint32_t(dx),std::uint32_t(product(along,xdir))));
        const auto py=wrapped_abs(difference(std::uint32_t(dy),std::uint32_t(product(along,ydir))));
        const auto pz=wrapped_abs(difference(std::uint32_t(dz),std::uint32_t(product(along,zdir))));
        if(bits(behavior_octagon3(px,py,pz))<=bits(radius))return i;
    }
    return std::nullopt;
}
void query_combat_step_45f2c0(ActorQueryTable& table,std::size_t self,const ActorQueryData& data,BehaviorState& state){
    auto& actor=actor_at(table,self);auto& person=table.person(self);auto& own=table.person_extra(self);
    const auto target=index_of(table,data,own.field_44);if(!target || !actor_at(table,*target).active)return;
    std::uint32_t x=own.field_48,y=own.field_4c,z=own.field_50;std::int32_t error=5;
    if(person.flags24&0x80){
        const auto& other=actor_at(table,*target);x=other.position[0];y=other.position[1];z=other.position[2];
        if(!data.weapon_frame_14)throw ActorQueryError("combat query needs weapon frame +0x14");
        const auto limit=std::uint32_t(16u-std::uint32_t(data.weapon_frame_14(*target)))*62u;
        if(bits(limit)<bits(own.field_34))own.field_34=limit;
        error=bits(16u-std::uint32_t(bits(own.field_34)/62));
    }
    const auto dx=difference(x,actor.position[0]),dy=difference(y,actor.position[1]),dz=difference(z,actor.position[2]);
    const auto xx=std::uint32_t(dx>>16),yy=std::uint32_t(dy>>16);
    const auto horizontal=query_isqrt_477620(xx*xx+yy*yy)<<16;
    const auto yaw=query_angle_477770(dx,dy),pitch=query_angle_477770(bits(horizontal),dz);
    const auto a=std::uint32_t(behavior_rng_next(state))*2u-0x10000u,b=std::uint32_t(behavior_rng_next(state))*2u-0x10000u;
    auto scale=std::uint32_t(product(bits(person.field_70),30));
    if(data.double_score_67f548_bit80 && actor.field_70==2)scale=std::uint32_t(bits(scale)>>2);
    if(!own.field_6c){actor.angle_yaw=yaw+a*scale*std::uint32_t(error);actor.angle_pitch=pitch+std::uint32_t(bits(scale*std::uint32_t(error)*b)>>2);state.emit(BehaviorCallKind::effect,0x45e7a0);}
    else{
        const auto old_yaw=actor.angle_yaw,old_pitch=actor.angle_pitch;
        actor.angle_yaw+=std::uint32_t(bits(a)>>3);actor.angle_pitch+=std::uint32_t(bits(b)>>5);
        state.emit(BehaviorCallKind::effect,0x45e7a0);actor.angle_yaw=old_yaw;actor.angle_pitch=old_pitch;
    }
}
ActorQueryProvider::ActorQueryProvider(ActorQueryTable table,ActorQueryData data):table_(table),data_(std::move(data)){}
std::optional<BehaviorInput> ActorQueryProvider::answer(const AiActorRef& ref,std::uint32_t site,const BehaviorAction& request,BehaviorActor& view,BehaviorState& state){
    if(ref.group!=3)return std::nullopt;
    if(ref.index>=table_.size() || &actor_at(table_,ref.index)!=&view.actor)throw ActorQueryError("actor query view differs from its table");
    BehaviorInput result{site,0,0};
    switch(site){
    case behavior_site::kAnimInit:result.value=query_anim_id_460410(view.actor,request.args[0],request.args[1],request.args[2],data_.water_level);break;
    case behavior_site::kCombatResult:result.value=view.person_extra.field_6c;break;
    case behavior_site::kTargetSearch:{auto target=query_target_search_45e280(table_,ref.index,data_,state,result.value2);result.value=target?pointer(data_,*target):0;break;}
    case behavior_site::kSelector:result.value=query_selector_45f090(table_,ref.index,state);break;
    case behavior_site::kNearby:result.value=query_nearby_45f010(table_,ref.index);break;
    case behavior_site::kRay:{auto target=query_ray_45fb00(table_,ref.index,data_,request.args[0],request.args[1]);result.value=target?pointer(data_,*target):0;break;}
    case behavior_site::kFacing:result.value=query_facing_45edd0(table_,ref.index,data_,state);break;
    case behavior_site::kAngle:result.value=query_angle_477770(bits(request.args[0]),bits(request.args[1]));break;
    case behavior_site::kSight:{auto target=index_of(table_,data_,request.args[0]);if(!target)throw ActorQueryError("sight query needs its target");result.value=std::uint32_t(sight(data_,*target));break;}
    default:return std::nullopt;
    }
    return result;
}
bool ActorQueryProvider::perform_effect(const AiActorRef& ref,const BehaviorAction& action,BehaviorState& state){
    if(ref.group!=3)return false;
    switch(action.site){
    case behavior_site::kCombatStep:query_combat_step_45f2c0(table_,ref.index,data_,state);return true;
    case behavior_site::kZoneStep:query_zone_45fd90(table_,ref.index,data_,state);return true;
    case behavior_site::kZoneStep2:query_zone2_45fe30(table_,ref.index,data_,state);return true;
    default:return false;
    }
}
} // namespace dfre
