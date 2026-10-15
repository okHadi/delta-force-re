#include "dfre/movement.hpp"
#include <cstring>

namespace dfre {
namespace {
std::int32_t signed_bits(std::uint32_t value) noexcept {std::int32_t result;std::memcpy(&result,&value,sizeof(result));return result;}
std::uint32_t wrap_add(std::uint32_t a,std::uint32_t b) noexcept{return a+b;}
}
// Original479630: axis values reach the abs actions already scaled. The flip
// setting negates the Y axis, and the steady predicate divides the scale by 4.
MouseLook mouse_look(std::int32_t axis_x,std::int32_t axis_y,std::int32_t sensitivity,bool flip,bool steady) noexcept {
    if(!flip)axis_y=signed_bits(std::uint32_t(0)-std::uint32_t(axis_y)); // 0x47966A
    std::int32_t scale=signed_bits(std::uint32_t(sensitivity)<<14); // 0x479677
    if(steady)scale>>=2; // 0x47967E
    const auto scale_axis=[scale](std::int32_t axis) {
        const auto numerator=signed_bits(std::uint32_t(axis)<<16);
        const auto product=std::int64_t(numerator)*std::int64_t(scale); // imul ecx
        return signed_bits(std::uint32_t(product>>16)); // shrd eax,edx,10h
    };
    return {scale_axis(axis_x),scale_axis(axis_y)};
}
// Original488379-4883FD with the48892D double step and the4883CB clamp.
PlayerPose apply_look_step(const PlayerPose& pose,std::uint8_t flags0,std::uint8_t flags1) noexcept {
    PlayerPose result=pose;
    const std::uint32_t step=(flags0&control_no_run)?look_step_base:2*look_step_base;
    if(flags1&control_turn_left)result.angles[0]=wrap_add(result.angles[0],step);
    if(flags1&control_turn_right)result.angles[0]=wrap_add(result.angles[0],std::uint32_t(0)-step);
    if(flags0&control_look_up)result.angles[1]=wrap_add(result.angles[1],step);
    if(flags0&control_look_down)result.angles[1]=wrap_add(result.angles[1],std::uint32_t(0)-step);
    result.angles[1]=clamp_pitch(result.angles[1]);
    return result;
}
// Original4883CB-48894B: unsigned pitch limits, 0 is level.
std::uint32_t clamp_pitch(std::uint32_t pitch) noexcept {
    if(pitch<=0x80000000u){if(pitch>0x40000000u)return 0x40000000u;}
    else if(pitch<0xd0000000u)return 0xd0000000u;
    return pitch;
}
// Original478EE2/478F35/478F16/47823C: absolute actions add or subtract the value.
PlayerPose apply_look_action(std::uint16_t action,std::int32_t value,const PlayerPose& pose) noexcept {
    PlayerPose result=pose;const std::uint32_t delta=std::uint32_t(value);
    switch(action){
    case action_look_up_abs:result.angles[1]=wrap_add(result.angles[1],delta);break;
    case action_look_dn_abs:result.angles[1]=wrap_add(result.angles[1],std::uint32_t(0)-delta);break;
    case action_turn_left_abs:result.angles[0]=wrap_add(result.angles[0],delta);break;
    case action_turn_right_abs:result.angles[0]=wrap_add(result.angles[0],std::uint32_t(0)-delta);break;
    default:break;
    }
    return result;
}
// Original487658-487674.
void apply_step(ActorMotion& actor,std::int32_t dx,std::int32_t dy,std::int32_t dz) noexcept {
    actor.position[0]=signed_bits(wrap_add(std::uint32_t(actor.position[0]),std::uint32_t(dx)));
    actor.position[1]=signed_bits(wrap_add(std::uint32_t(actor.position[1]),std::uint32_t(dy)));
    const std::uint32_t vertical=wrap_add(std::uint32_t(actor.vertical_velocity),std::uint32_t(dz));
    actor.position[2]=signed_bits(wrap_add(std::uint32_t(actor.position[2]),vertical));
}
// Original4884FA-488553 plus the jump table at4881BC. The two zero entries are
// the 48855A pair that also clears the selector.
int move_octant(std::uint8_t flags0) noexcept {
    const int bits=(flags0>>1)&0x0f;
    static constexpr int table[16]={0,0,4,0,2,1,3,2,6,7,5,6,0,0,4,0};
    return table[bits];
}
// Original48853D-488650: edi, the second original output.
int move_anim_selector(std::uint8_t flags0,bool actor_flag_6e_bit3) noexcept {
    const int bits=(flags0>>1)&0x0f;
    static constexpr bool idle[16]={true,false,false,true,false,false,false,false,false,false,false,false,true,false,false,true};
    if(bits==0||idle[bits])return 0; // 488507 and the 48855A pair
    if(actor_flag_6e_bit3)return 2;  // 488549
    return (flags0&control_no_run)?2:3; // 488650, sete tests the zero flag
}
} // namespace dfre
