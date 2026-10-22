#include "dfre/motion_clearance.hpp"
#include <cstring>

namespace dfre {
namespace {
std::int32_t signed_bits(std::uint32_t value) noexcept {std::int32_t result;std::memcpy(&result,&value,sizeof(result));return result;}
std::int32_t wrap_add(std::int32_t value,std::int32_t delta) noexcept {return signed_bits(std::uint32_t(value)+std::uint32_t(delta));}
std::int32_t wrap_sub(std::int32_t value,std::int32_t delta) noexcept {return signed_bits(std::uint32_t(value)-std::uint32_t(delta));}
// Original shrd keeps the low 32 bits of the signed 64-bit product.
std::int32_t product_shift(std::int32_t left,std::int32_t right,unsigned shift) noexcept {
    return signed_bits(std::uint32_t((std::int64_t(left)*std::int64_t(right))>>shift));
}
}
// Original 4876E8-487A9A.
std::int32_t ground_clearance(const std::vector<PosePosition>& pose_positions,std::uint32_t frame_mask) noexcept {
    std::int32_t minimum=clearance_none;
    for(std::size_t index=0;index<pose_positions.size();++index) {
        const auto bit=unsigned(clearance_bit_base+index)&31u; // shl eax,cl uses count & 31
        if(frame_mask&(std::uint32_t(1)<<bit))continue;        // 487713: a set bit skips the bone
        const auto candidate=product_shift(pose_positions[index][1],clearance_scale,16);
        if(candidate<minimum)minimum=candidate;                 // 487A9A signed minimum
    }
    return minimum;
}
// Original 486FD0.
GroundEntityResult ground_entity_height(std::int32_t terrain_height,const std::array<std::int32_t,3>& position,
    std::int32_t depth,const std::vector<GroundEntity>& entities) noexcept {
    GroundEntityResult result;result.height=terrain_height;
    for(const auto& entity:entities) {
        const GroundDefinition* definition=entity.definition;
        if((entity.flags&8)!=0&&entity.alternate!=nullptr)definition=entity.alternate; // 486FEC
        if(definition==nullptr)continue;
        const auto radius=definition->radius;
        if(!(wrap_sub(entity.position[0],radius)<=position[0]&&position[0]<=wrap_add(entity.position[0],radius)))continue;
        if(!(wrap_sub(entity.position[1],radius)<=position[1]&&position[1]<=wrap_add(entity.position[1],radius)))continue;
        for(const auto& piece:definition->pieces) {
            if((piece.flags&3)!=0)continue;                                             // 487101
            const auto point_x=piece.point[0],point_y=piece.point[1];
            if(point_x<piece.bounds[0]||piece.bounds[1]<point_x)continue;               // 4870D8
            if(point_y<piece.bounds[2]||piece.bounds[3]<point_y)continue;
            for(const auto& volume:piece.volumes) {
                if(volume.skip)continue;                                                // 4870B0
                if(point_x<volume.bounds[0]||volume.bounds[1]<point_x)continue;         // 48709C
                if(point_y<volume.bounds[2]||volume.bounds[3]<point_y)continue;
                if(!volume.accepted)continue;                                           // 0x42D2A0 verdict
                if(volume.height<result.height)continue;                                // 4870E0 reject lower surfaces
                if(wrap_add(position[2],depth)<volume.height)continue;                  // 4870E4 reject above the query top
                result.entity=&entity;                                                 // actor +0x100
                result.height=volume.height;
            }
        }
    }
    return result;
}
} // namespace dfre
