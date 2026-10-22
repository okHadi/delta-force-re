// Native projectile checks. The expected values come from
// tools/check_projectiles_original.py, which replays the same cases on the
// original Df.exe code (SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff).
#include "dfre/projectiles.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void require(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }
}
int main() { try {
    // 477510 table + 4776C0 interpolation: sin/cos of 45 degrees.
    std::int32_t sin = 0, cos = 0;
    dfre::sincos_q22(0x20000000, sin, cos);
    require(sin == 2965820 && cos == 2965820, "sincos 45 degrees");
    dfre::sincos_q22(0, sin, cos);
    require(sin == 0 && cos == 4194303, "sincos zero");

    // 477860 LCG sequence from seed 1.
    dfre::WeaponRandom rng;
    rng.seed(1);
    require(rng.next() == 0x8011 && rng.next() == 0x8111, "LCG sequence");

    // Matrix helpers.
    const auto identity = dfre::matrix_identity();
    require(identity.m[0] == 0x400000 && identity.m[5] == 0x400000 && identity.m[10] == 0x400000, "identity");
    const auto translation = dfre::matrix_translation({123, -456, 789});
    require(translation.m[3] == 123 && translation.m[7] == -456 && translation.m[11] == 789, "translation column");
    const auto yaw_only = dfre::compose_pose({0, 0, 0}, {0x20000000, 0, 0});
    require(yaw_only.m[0] == 2965820 && yaw_only.m[1] == -2965820 && yaw_only.m[4] == 2965820, "pose yaw-only");
    const dfre::Fixed3 point = dfre::transform_point(translation, {10, 20, 30});
    require(point.x == 133 && point.y == -436 && point.z == 819, "transform point");

    // Original 482D90 muzzle with zero actor angles.
    dfre::MuzzleInputs inputs;
    inputs.actor_position = {1000, 2000, 3000};
    inputs.weapon_muzzle = {2, -3, 4};
    inputs.view_delta = {10, -20, 30};
    inputs.eye_height = 0xB333;
    const auto muzzle = dfre::compute_muzzle(inputs);
    require(muzzle.origin.x == 1521 && muzzle.origin.y == 1212, "muzzle offset");
    require(muzzle.origin.z == 49928, "muzzle eye height");
    require(muzzle.aim.yaw == 0 && muzzle.aim.pitch == 0 && muzzle.aim.roll == 0, "muzzle aim");

    // 4847D0 barrel tip with zero angles = muzzle + offset.
    const auto tip = dfre::compute_barrel_tip({100, 200, 300}, {0, 0, 0}, {5, 6, 7});
    require(tip.x == 105 && tip.y == 206 && tip.z == 307, "barrel tip");

    // 45EA00 spread and kind plan.
    dfre::Angle3 aim{0x10000000, 0x20000000, 0};
    dfre::WeaponRandom plan_rng;
    plan_rng.seed(1);
    const auto plan = dfre::plan_spawn(6, 7, 0, 1, plan_rng, aim);
    require(plan.spawns && !plan.melee && plan.kind == 8 && plan.burst_after == 2, "burst first round");
    require(aim.yaw == 0x10000000 && aim.pitch == 0x20000000, "no spread for class field 1");
    aim = {0x10000000, 0x20000000, 0};
    plan_rng.seed(1);
    const auto spread_plan = dfre::plan_spawn(6, 7, 0, 5, plan_rng, aim);
    require(spread_plan.kind == 8 && aim.yaw == 268433416u && aim.pitch == 536838152u, "spread angles");
    dfre::Angle3 knife_aim{};
    dfre::WeaponRandom knife_rng;
    const auto knife = dfre::plan_spawn(1, 0, 0, 1, knife_rng, knife_aim);
    require(knife.melee && !knife.spawns, "knife melee path");

    // 46FB40 record build and 46EA40 decode (256-unit quantization).
    const auto ground = [](std::int32_t, std::int32_t) { return 1000; };
    const auto record = dfre::build_spawn_record({16777216, 33554432, 50331648}, {0x10000000, 0x20000000, 0},
                                                 3, 8, ground);
    require(record.packed_x == 16777471 && record.packed_y == 33554684, "record packing");
    require(record.actor_slot == 3 && record.aim_yaw_word == 4096 && record.aim_pitch_word == 8192, "record words");
    // Roundtrip case: height 0x10000 above ground survives the quantization.
    const auto ground_low = [](std::int32_t, std::int32_t) { return 6488064; };
    const auto roundtrip = dfre::build_spawn_record({16777216, 33554432, 6553600}, {0x10000000, 0x20000000, 0},
                                                    3, 8, ground_low);
    require(roundtrip.packed_x == 16777217 && roundtrip.packed_y == 33554432, "roundtrip packing");
    const auto decoded = dfre::decode_spawn_record(roundtrip, ground_low);
    require(decoded.origin.x == 16777216 && decoded.origin.y == 33554432 && decoded.origin.z == 6553600, "decode origin");
    require(decoded.aim.yaw == 0x10000000 && decoded.aim.pitch == 0x20000000, "decode aim");

    // 462F20 entity: M4-class speed 1000 at 45 degrees.
    dfre::ClassSpawnFields fields;
    fields.speed = 1000;
    fields.field_10 = 30;
    fields.field_14 = 70;
    fields.field_18 = 100;
    fields.flags = 2;
    const auto entity = dfre::spawn_projectile({16777216, 33554432, 50331648}, {0x20000000, 0, 0}, fields,
                                               {4001, 4020}, 4660, 3, 0xB9A0A0FF, false);
    require(entity.velocity.x == 747434 && entity.velocity.y == 747434 && entity.velocity.z == 0, "velocity");
    require(entity.ttl == 100 && entity.field_7c == 70 && entity.field_80 == 30 && entity.field_84 == 100, "fields");
    require(entity.color == 0xB9A0A0FF && entity.item_main == 0, "no items for flags 2");

    // 464EB0 motion: gravity and ttl per tick.
    dfre::ProjectileEntity flying;
    flying.position = {16777216, 0, 50331648};
    flying.velocity = {1057032, 0, 0};
    flying.ttl = 100;
    dfre::ProjectileQuery empty;
    std::vector<dfre::ProjectileEvent> events;
    dfre::tick_projectile(flying, {0x4000000, 0x4000000, 0}, empty, events);
    require(flying.position.x == 17834248 && flying.velocity.z == -178 && flying.ttl == 99, "tick one");
    require(events.size() == 1 && events[0].kind == dfre::ProjectileEventKind::move, "tick move event");
    dfre::tick_projectile(flying, {0x4000000, 0x4000000, 0}, empty, events);
    require(flying.position.x == 18891280 && flying.position.z == 50331470 && flying.ttl == 98, "tick two");

    // Scripted terrain hit: slide position, deactivation, gravity.
    dfre::ProjectileEntity hitting;
    hitting.position = {16777216, 0, 50331648};
    hitting.velocity = {1057032, 0, 0};
    hitting.ttl = 10;
    hitting.item_main = 0x1000;
    dfre::ProjectileQuery terrain_hit;
    terrain_hit.terrain = [](const dfre::ProjectileEntity&, const dfre::Fixed3&, std::int32_t,
                             std::int32_t&) { return std::int32_t(-100000); };
    unsigned effects=0;
    terrain_hit.on_event=[&](dfre::ProjectileEntity& live,const dfre::ProjectileEvent& event){
        require(live.position.x==16777216,"effect sees original projectile position");
        if(event.kind==dfre::ProjectileEventKind::impact){
            require(effects==0 && live.ttl==10 && live.item_main==0x1000,"impact runs before removal");
            live.velocity.z=1000;
        }else{
            require(event.kind==dfre::ProjectileEventKind::move && effects==1 && live.ttl==0 && live.item_main==0,
                "move runs after impact removal and before position store");
        }
        ++effects;
    };
    events.clear();
    dfre::tick_projectile(hitting, {0x4000000, 0x4000000, 0}, terrain_hit, events);
    require(hitting.position.x == 16860826 && hitting.ttl == -1 && hitting.item_main == 0, "terrain hit state");
    require(hitting.velocity.z == 822 && effects==2, "gravity uses the current effect state");

    std::cout << "projectile checks passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
} }
