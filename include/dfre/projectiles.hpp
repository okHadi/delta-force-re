#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
// Original projectile spawn and motion for Delta Force (1998).
// Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// Fixed units are original: positions are 16.16, angles are full turns in
// 2^32, the Q22 sine table matches the runtime build at 0xBD0068.

struct Fixed3 { std::int32_t x = 0, y = 0, z = 0; };
struct Angle3 { std::uint32_t yaw = 0, pitch = 0, roll = 0; };

// ---- 477510 table build / 4776C0 interpolated sincos ----
extern const std::array<std::int32_t, 2561> kSineTableQ22;
void sincos_q22(std::uint32_t angle, std::int32_t& sin_q22, std::int32_t& cos_q22) noexcept;

// ---- 477860 LCG, an explicit RNG input ----
struct WeaponRandom {
    std::uint32_t state = 0;
    void seed(std::uint32_t value) noexcept { state = value; }
    std::uint16_t next() noexcept;
};

// ---- 45C910/45C970/45CAE0/45CB40/45CDA0/45D1E0 matrix helpers (Q22) ----
struct Matrix4 { std::array<std::int32_t, 12> m{}; };
Matrix4 matrix_identity() noexcept;
Matrix4 matrix_rotation(std::uint32_t axis, std::int32_t cos_q22, std::int32_t sin_q22) noexcept;
Matrix4 matrix_translation(Fixed3 value) noexcept; // 45CAE0: identity + translation column
Matrix4 matrix_multiply(const Matrix4& left, const Matrix4& right) noexcept;
Fixed3 transform_point(const Matrix4& matrix, const Fixed3& point) noexcept;
Matrix4 compose_pose(Fixed3 position, Angle3 angles) noexcept;

// ---- 482D90 muzzle origin and aim angles ----
struct MuzzleInputs {
    Fixed3 actor_position;
    Angle3 actor_angles;
    Fixed3 view_delta;          // C239E8/C239EC/C239F0
    Fixed3 weapon_muzzle;       // weapon record +0x78/+0x7C/+0x80
    Angle3 weapon_aim;          // weapon record +0x84/+0x88/+0x8C
    std::int32_t eye_height = 0; // C23A18
};
struct MuzzleResult { Fixed3 origin; Angle3 aim; };
MuzzleResult compute_muzzle(const MuzzleInputs& inputs) noexcept;

// ---- 4847D0 barrel tip: muzzle pose matrix applied to a mesh offset ----
Fixed3 compute_barrel_tip(Fixed3 muzzle, Angle3 aim, Fixed3 mesh_offset) noexcept;

// ---- 45EA00 spread and kind plan (burst counter in/out, RNG input) ----
struct SpawnPlan {
    bool spawns = false;      // 46FB40 is called
    bool melee = false;       // weapon 1 takes the 470B00 melee path
    std::uint8_t kind = 0;
    std::int32_t burst_after = 0; // actor item definition +0x6C after the call
};
SpawnPlan plan_spawn(std::uint32_t weapon_index, std::uint8_t weapon_class,
                     std::int32_t burst_counter, std::int32_t class_spread,
                     WeaponRandom& rng, Angle3& aim) noexcept;

// ---- 46FB40 spawn record / 46EA40 record decode ----
struct SpawnRecord {
    std::uint32_t packed_x = 0, packed_y = 0;
    std::uint16_t actor_slot = 0;
    std::uint16_t aim_yaw_word = 0, aim_pitch_word = 0;
    std::uint8_t kind = 0;
};
using GroundHeightFn = std::function<std::int32_t(std::int32_t x, std::int32_t y)>;
SpawnRecord build_spawn_record(Fixed3 origin, Angle3 aim, std::uint16_t actor_slot,
                               std::uint8_t kind, const GroundHeightFn& ground);
struct DecodedSpawn { Fixed3 origin; Angle3 aim; std::uint8_t kind = 0; std::uint16_t actor_slot = 0; };
DecodedSpawn decode_spawn_record(const SpawnRecord& record, const GroundHeightFn& ground);

// ---- 462F20 projectile entity ----
struct ProjectileEntity {
    std::int32_t item_main = 0;  // +0x00 item pointer (input)
    std::int32_t item_alt = 0;   // +0x04 item pointer (input)
    std::int32_t item_field = 0; // +0x10
    Fixed3 position;             // +0x24
    std::uint32_t aim_yaw = 0;   // +0x30
    std::uint32_t aim_pitch = 0; // +0x34
    Fixed3 velocity;             // +0x3C
    std::uint32_t class_flags = 0; // +0x6C = class +0x30
    std::uint32_t field_7c = 0;  // +0x7C = class +0x14
    std::uint32_t field_80 = 0;  // +0x80 = class +0x10
    std::uint32_t field_84 = 0;  // +0x84 = class +0x18
    std::uint32_t color = 0;     // +0x88
    std::int32_t owner_slot = -1; // +0xF8 actor pointer as slot input
    std::int32_t ttl = 0;        // +0xFC
    std::array<std::uint8_t,204> reserved_source_bytes{}; // Unmodeled source offsets in increasing order.
    bool active() const noexcept { return ttl > 0 && position.z >= -65536; }
};
using ProjectileRecordImage=std::array<std::uint8_t,0x114>;
ProjectileRecordImage snapshot_projectile_record(const ProjectileEntity&) noexcept;
void apply_projectile_record(ProjectileEntity&,const ProjectileRecordImage&);

struct ClassSpawnFields { // weapon class record inputs
    std::uint32_t speed = 0;     // +0x0C
    std::uint32_t field_10 = 0;  // +0x10
    std::uint32_t field_14 = 0;  // +0x14
    std::uint32_t field_18 = 0;  // +0x18
    std::uint32_t flags = 0;     // +0x30
};
ProjectileEntity spawn_projectile(Fixed3 origin, Angle3 aim, const ClassSpawnFields& fields,
                                  const std::array<std::int32_t, 2>& item_pointers,
                                  std::int32_t item_field, std::int32_t owner_slot,
                                  std::uint32_t team_color, bool global_8cf982_bit4) noexcept;

// ---- 464EB0 per-tick motion core ----
// Collision queries keep the original call order and conventions:
//   terrain (47E390): returns negative distance on hit, 1 for none
//   actors_a/actors_b (464400): return 0 on hit with the distance in
//     `distance`, nonzero for none
//   special_a/special_b (463790, 464BA0): return 0 on hit with the distance
//     in `distance`, nonzero for none
struct ProjectileEvent;
struct ProjectileQuery {
    std::function<std::int32_t(const ProjectileEntity&, const Fixed3& step,
                               std::int32_t length, std::int32_t& distance)> terrain;
    std::function<std::int32_t(const ProjectileEntity&, const Fixed3& step,
                               std::int32_t length, std::int32_t& distance)> actors_a;
    std::function<std::int32_t(const ProjectileEntity&, const Fixed3& step,
                               std::int32_t length, std::int32_t& distance)> actors_b;
    std::function<std::int32_t(const ProjectileEntity&, const Fixed3& step,
                               std::int32_t length, std::int32_t& distance)> special_a;
    std::function<std::int32_t(const ProjectileEntity&, const Fixed3& step,
                               std::int32_t length, std::int32_t& distance)> special_b;
    bool special_b_enabled = false; // [0xBD5248]
    bool alternate_move_filter = false; // [0x67F549] & 0x10 selects mask 0x60 over 0x90
    // Execute each reached effect before later projectile state writes.
    std::function<void(ProjectileEntity&,const ProjectileEvent&)> on_event;
    // Read after impact effects, at the original move/edge call sites.
    std::function<bool()> use_alternate_move_filter;
    std::function<Fixed3()> current_world_center;
};
enum class ProjectileEventKind : std::uint8_t {
    move,       // 463350 position record (message/demo), flags filtered
    world_edge, // 464E00 edge crossing, hit_type 0x41 is the sound id
    impact,     // hit dispatch entry; effects and damage stay outside
};
struct ProjectileEvent {
    ProjectileEventKind kind = ProjectileEventKind::impact;
    std::uint8_t hit_type = 0; // 0 terrain, 1/2 actor arrays, 3/4 special
    Fixed3 position;           // impact, crossing point or new position
    Fixed3 from;               // move events: previous position (463350 argument)
};
// One 464EB0 iteration for a single record: activation test, magnitude and
// unit step, collision query order, slide, gravity 0xB2, TTL decrement and
// the impact deactivation. Impact effects, damage and sounds stay outside.
void tick_projectile(ProjectileEntity& entity, Fixed3 world_center,
                     const ProjectileQuery& query, std::vector<ProjectileEvent>& events);
} // namespace dfre
