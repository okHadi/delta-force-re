#include "dfre/motion_step.hpp"
#include "dfre/models.hpp"
#include <cstring>

namespace dfre {
namespace {
std::int32_t signed_bits(std::uint32_t value) noexcept {std::int32_t result;std::memcpy(&result,&value,sizeof(result));return result;}
// All arithmetic below mirrors 32-bit x86: add, subtract and negate wrap.
std::int32_t wrap_add(std::int32_t value,std::int32_t delta) noexcept {return signed_bits(std::uint32_t(value)+std::uint32_t(delta));}
std::int32_t wrap_sub(std::int32_t value,std::int32_t delta) noexcept {return signed_bits(std::uint32_t(value)-std::uint32_t(delta));}
std::int32_t wrap_neg(std::int32_t value) noexcept {return signed_bits(std::uint32_t(0)-std::uint32_t(value));}
// Original shrd eax,edx,count keeps the low 32 bits of the signed product.
std::int32_t product_shift(std::int32_t left,std::int32_t right,unsigned shift) noexcept {
    return signed_bits(std::uint32_t((std::int64_t(left)*std::int64_t(right))>>shift));
}
std::int32_t abs_value(std::int32_t value) noexcept {
    const std::uint32_t bits=std::uint32_t(value);
    return signed_bits(bits>>31?std::uint32_t(0)-bits:bits);
}
// Original 4878A1/4878A9/4878C2: negate the 32-bit value, then sar 1.
std::int32_t halve_negated(std::int32_t value) noexcept {return signed_bits(std::uint32_t(0)-std::uint32_t(value))>>1;}
constexpr std::int32_t definition_scale=0x1d2b0; // 4873A2
}
SineCosine frame_base_sincos(std::uint32_t yaw,std::uint32_t frame_angle) noexcept {return fixed_sine_cosine(yaw+frame_angle);}
SineCosine frame_pitch_sincos(std::uint32_t pitch) noexcept {return fixed_sine_cosine(pitch);}
// Original 487432, 487834 and 487883.
StepVector frame_step(const FrameStepInput& input) noexcept {
    if(input.frame_vertical){
        const auto definition=[&](std::int32_t value) {return product_shift(value,definition_scale,16);};
        return {halve_negated(definition(input.definition_up)),halve_negated(definition(input.definition_side)),
                definition(input.definition_vertical)>>1};
    }
    if(input.walk_3d){
        const auto forward=product_shift(input.frame_speed,input.pitch_pair[1],22); // 48745F
        std::int32_t z=product_shift(input.frame_speed,input.pitch_pair[0],22)>>1;
        if(z>0&&double(input.water_level)<double(input.position_z)+26214.4)z=0; // 4874A7
        return {product_shift(forward,input.base[1],22),product_shift(forward,input.base[0],22),z};
    }
    return {product_shift(input.frame_speed,input.base[1],22)>>1,
            product_shift(input.frame_speed,input.base[0],22)>>1,
            product_shift(input.definition_vertical,definition_scale,16)};
}
// Original 4874BF-487561 with the inline gate.
SlopeResponse terrain_slope_response(const StepVector& step,std::uint32_t flags,std::int32_t slope_x,
    std::int32_t slope_y,const SineCosine& base) noexcept {
    SlopeResponse result;result.step=step;result.slope_blocked=(flags&actor_slope_blocked)!=0;
    if(!(flags&actor_active)||(flags&(actor_airborne|actor_in_water)))return result; // 4874C3-4874D0
    result.applied=true;
    const auto magnitude=[&](std::int32_t value) {return abs_value(value);};
    if(!result.slope_blocked){
        if(magnitude(slope_x)<0x14ccd&&magnitude(slope_y)<0x14ccd){
            if(magnitude(slope_x)>0xb333||magnitude(slope_y)>0xb333){
                const auto across=product_shift(base[1],slope_x,22); // 48794C
                if(wrap_add(product_shift(base[0],slope_y,22),across)>0xb333)result.wall=true;
            }
        }
        else result.slope_blocked=true;
    }
    else if(magnitude(slope_x)<0xb333&&magnitude(slope_y)<0xb333)result.slope_blocked=false;
    else {
        result.step.x=wrap_neg(product_shift(slope_x,0x1999,16)); // 4878CB pushback
        result.step.y=wrap_neg(product_shift(slope_y,0x1999,16));
    }
    return result;
}
// Original 487991-4879CC.
StepVector cached_step(const StepVector& step,bool airborne,CachedStep& cache) noexcept {
    if(airborne)return {cache.x,cache.y,step.z};
    cache={step.x,step.y,step.z};
    return step;
}
// Original 487565-4877DE.
GroundResult ground_update(MotionActor& actor,const GroundObject* ground_object,const GroundQuery& query,
    const StepVector& step,bool is_local_player) noexcept {
    GroundResult result;
    if(actor.on_ground_object&&ground_object){
        actor.position[0]=wrap_add(actor.position[0],ground_object->offset_x);
        actor.position[1]=wrap_add(actor.position[1],ground_object->offset_y);
        if(ground_object->rotation){
            const auto base=fixed_sine_cosine(ground_object->rotation);
            const auto offset_x=wrap_sub(actor.position[0],ground_object->position[0]);
            const auto offset_y=wrap_sub(actor.position[1],ground_object->position[1]);
            actor.position[0]=wrap_add(ground_object->position[0],
                wrap_sub(product_shift(base[1],offset_x,22),product_shift(base[0],offset_y,22))); // 4875DA-487607
            actor.position[1]=wrap_add(ground_object->position[1],
                wrap_add(product_shift(base[0],offset_x,22),product_shift(base[1],offset_y,22)));
            actor.yaw+=ground_object->rotation;
        }
    }
    ActorMotion motion;motion.position=actor.position;motion.vertical_velocity=actor.vertical;
    apply_step(motion,step.x,step.y,step.z);
    actor.position=motion.position;
    if(query.full_pass||is_local_player){
        const std::int32_t height=query.height;
        const std::int32_t difference=wrap_sub(wrap_add(actor.position[2],query.clearance),height);
        if(actor.flags&actor_airborne){
            if(difference<0){ // 487AC1: snap up and clear the airborne bit
                actor.vertical=0;actor.flags&=~actor_airborne; // 487ACD clears only the 6D byte bit 5
                actor.position[2]=wrap_sub(actor.position[2],difference);result.snapped=true;
            }
            else if(query.water_level>actor.position[2]){
                actor.flags&=~(actor_airborne|actor_in_water);actor.flags|=actor_in_water;
                result.entered_water=true;
            }
            else actor.vertical=wrap_add(actor.vertical,wrap_neg(is_local_player?0xb2:0xb20));
            return result;
        }
        if(actor.flags&actor_in_water){
            if(actor.position[2]>query.water_level){actor.flags&=~actor_in_water;result.left_water=true;}
            else if(difference>=0)actor.vertical=product_shift(actor.vertical,is_local_player?0xcccc:0x68db,16);
            else {actor.vertical=0;actor.position[2]=wrap_sub(actor.position[2],difference);result.snapped=true;}
            return result;
        }
        if(actor.flags&actor_slope_blocked){
            if(difference<0)actor.position[2]=wrap_sub(actor.position[2],difference);
            actor.vertical=wrap_add(actor.vertical,wrap_neg(is_local_player?0xb2:0xb20));
            return result;
        }
        if(difference>=0){
            if(query.water_level>actor.position[2]){
                actor.flags|=actor_in_water;result.entered_water=true;
                actor.vertical=wrap_add(actor.vertical,wrap_neg(is_local_player?0xb2:0xb20));
                return result;
            }
            if(difference>0x800){
                actor.vertical=wrap_add(actor.vertical,wrap_neg(is_local_player?0xb2:0xb20));
                if(difference>0x8000||actor.vertical>0){actor.flags|=actor_airborne;result.airborne=true;}
            }
            return result;
        }
        actor.vertical=0;actor.position[2]=wrap_sub(actor.position[2],difference);result.snapped=true;
    }
    return result;
}
// Original 48834E-488969, minus the turn/look application owned by movement.hpp.
std::int32_t movement_animation(MovementActorState& state,std::uint8_t slot_flags,bool steady,
    std::int32_t jump_impulse) noexcept {
    const std::uint16_t control=state.control;
    static constexpr int octant[16]={-1,0,4,-1,2,1,3,2,6,7,5,6,-1,0,4,-1};
    const int move_bits=(control>>1)&0x0f;
    int direction=0,mode=0;
    std::int32_t animation=0;
    if((state.flags&actor_airborne)==0){
        if((state.flags&actor_in_water)==0){
            if((state.flags&actor_slope_blocked)==0){
                if(control&0x1e){
                    const int entry=octant[move_bits];
                    if((state.flags&actor_slope_step)!=0){
                        mode=2;
                        if(entry<0){direction=0;mode=0;}else direction=entry;
                    }
                    else {
                        mode=(control&1)?2:3;
                        if(entry<0){direction=0;mode=0;}else direction=entry;
                    }
                }
                if(control&0x400){
                    if(state.prone==0){
                        if((state.flags&actor_slope_blocked)==0&&state.pulse_jump!=0){
                            state.vertical=jump_impulse;state.z=wrap_add(state.z,0x1000);state.pulse_jump=0;
                        }
                    }
                    else {if(state.has_ground_object)state.z=wrap_add(state.z,0x8000);state.prone=0;}
                }
                if(state.pulse_prone!=0){
                    const std::int32_t previous=state.prone;state.pulse_prone=0;
                    if(state.prone==0){state.prone=1;state.mode=previous;}
                    else {if(state.has_ground_object)state.z=wrap_add(state.z,0x8000);state.prone=0;}
                }
                if(state.pulse_crouch!=0){
                    state.pulse_crouch=0;
                    if(state.mode==0){if(state.prone!=0&&state.has_ground_object)state.z=wrap_add(state.z,0x4000);state.mode=1;state.prone=0;}
                    else {if(state.has_ground_object)state.z=wrap_add(state.z,0x4000);state.mode=0;}
                }
                if(state.pulse_stand!=0){
                    state.pulse_stand=0;
                    if(state.mode==0){if(state.prone!=0){if(state.has_ground_object)state.z=wrap_add(state.z,0x8000);state.prone=0;}}
                    else {if(state.has_ground_object)state.z=wrap_add(state.z,0x4000);state.mode=0;}
                }
                if(state.prone==0){
                    if(mode==0){
                        if(state.mode==0){
                            if(state.stance<1){
                                if((slot_flags&8)==0){
                                    if(!steady)animation=(slot_flags&4)==0?((slot_flags&0x40)?7:1):2;
                                    else animation=4;
                                }
                                else animation=3;
                            }
                            else if((slot_flags&8)==0){
                                if(!steady){
                                    if((slot_flags&4)==0)animation=(slot_flags&0x40)?0xe:0xa;
                                    else animation=0xb;
                                }
                                else animation=0xd;
                            }
                            else animation=0xc;
                        }
                        else {
                            const std::uint32_t scaled=std::uint32_t(slot_flags&0xff)<<8;
                            if((scaled&0x800)==0)animation=(slot_flags&4)==0?((scaled&0x4000)?0x6a:100):0x65;
                            else animation=0x66;
                        }
                    }
                    else if(mode==2)animation=direction+(state.stance<1?((slot_flags&0xc)?0x1c:0x14):((slot_flags&0xc)?0x30:0x28));
                    else if(mode==3)animation=direction+(state.stance<1?((slot_flags&0xc)?0x44:0x3c):((slot_flags&0xc)?0x58:0x50));
                }
                else if(mode==0){
                    if(slot_flags&8)animation=0x70;
                    else animation=(slot_flags&4)==0?0x6e:0x6f;
                }
                else animation=direction+0x78;
            }
            else animation=0x82;
        }
        else animation=(control&2)?0x8d:0x8c;
    }
    else if(state.prone==0)animation=state.stance<1?((slot_flags&1)?0x83:0x82):((slot_flags&1)?0x85:0x84);
    else {
        const std::uint32_t scaled=std::uint32_t(slot_flags&0xff)<<8;
        if((scaled&0x800)==0)animation=(slot_flags&4)==0?((scaled&0x4000)?0x74:0x6e):0x6f;
        else animation=0x70;
    }
    return animation;
}
} // namespace dfre
