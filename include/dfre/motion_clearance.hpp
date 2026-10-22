#pragma once
#include <array>
#include <cstdint>
#include <vector>

// Ground clearance scan and entity ground query for the motion step.
// Df.exe SHA256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// See docs/motion-clearance.md.

namespace dfre {
// Original 0x4876E0 scans current CHR instance root positions at +0x8C.
// Instance +0x0C selects the animation, +0x10 selects the frame.
// Animation +8 holds the root count; current frame +4 holds the root mask.
inline constexpr std::int32_t clearance_none=0xfffffff; // 4876E8
inline constexpr std::int32_t clearance_scale=0x1d2b0;  // 4873A2
inline constexpr unsigned clearance_bit_base=16;        // 4876FC

// One 12-byte pose position record. The scan reads the record's second word.
using PosePosition=std::array<std::int32_t,3>;

// Returns the minimum of (record[1] * 0x1D2B0) >> 16 over every bone whose
// frame bit is clear, or clearance_none when nothing qualifies. The original
// shift uses count & 31 (`shl eax,cl`), so bones beyond 15 reuse low bits.
std::int32_t ground_clearance(const std::vector<PosePosition>& pose_positions,std::uint32_t frame_mask) noexcept;

// Original 0x486FD0 entity walk. The mesh geometry stages stay explicit inputs
// because the collision module owns 0x42D2A0 and the transform family:
//   0x45D170/0x45CE60 produce the query point in instance space -> GroundPiece::point
//   0x45CDA0/0x42D2A0 produce each volume's accepted height    -> GroundVolume::height/accepted
struct GroundVolume {                       // 72-byte record fields used here
    bool skip=false;                        // (record +0x44)->+0x13 bit 0
    std::array<std::int32_t,4> bounds{};    // words 11..14: +0x2C, +0x30, +0x34, +0x38
    bool accepted=false;                    // 0x42D2A0 verdict
    std::int32_t height=0;                  // 0x45CDA0 output (original local_48)
};
// The original walks one part count from [definition+0x50]+0x98 and advances
// the 0x40-stride instance record and the 0x70-stride part record together, so
// one piece carries both records for one iteration.
struct GroundPiece {
    std::uint32_t flags=0;                  // instance +0x3C; bits 0 and 1 skip the piece
    std::array<std::int32_t,2> point{};     // 0x45CE60 query point for this iteration
    std::array<std::int32_t,4> bounds{};    // part +0x40, +0x44, +0x48, +0x4C
    std::vector<GroundVolume> volumes;      // part +0xC count, records at +0x10, stride 0x48
};
struct GroundDefinition {
    std::int32_t radius=0;                  // definition +0x64
    std::vector<GroundPiece> pieces;        // [definition+0x50]+0x98 count, +0x9C part list
};
struct GroundEntity {
    std::array<std::int32_t,2> position{};  // entity +0x24, +0x28
    std::uint32_t flags=0;                  // entity +0x6C; bit 8 prefers the alternate definition
    const GroundDefinition* definition=nullptr; // entity +0x10
    const GroundDefinition* alternate=nullptr;  // entity +0x14
};
struct GroundEntityResult {
    std::int32_t height=0;                  // original return; starts at the terrain height
    const GroundEntity* entity=nullptr;     // written to actor +0x100, null when nothing matched
    bool operator==(const GroundEntityResult& other) const noexcept {
        return height==other.height&&entity==other.entity;
    }
};
// terrain_height is the 0x47DF40 result; position is actor +0x24/+0x28/+0x2C;
// depth is the original third argument (0x8000 from 0x487370).
GroundEntityResult ground_entity_height(std::int32_t terrain_height,const std::array<std::int32_t,3>& position,
    std::int32_t depth,const std::vector<GroundEntity>& entities) noexcept;
} // namespace dfre
