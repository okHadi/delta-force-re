#include "dfre/projectiles.hpp"
#include <cstdlib>
#include <cmath>
namespace dfre {
namespace {
std::int32_t rol32(std::uint32_t value, unsigned bits) noexcept {
    return std::int32_t((value << bits) | (value >> (32 - bits)));
}
std::int32_t isqrt32(std::int32_t value) noexcept { // 477620
    if (value <= 1) return value;
    std::uint32_t ebx = std::uint32_t(value), edx = 0, eax = 0, ecx = 0;
    do {
        edx = (edx << 2) | ((ebx >> 30) & 3);
        eax <<= 1;
        const std::uint32_t probe = eax + eax + 1;
        ebx <<= 2;
        if (edx >= probe) {
            eax += 1;
            edx -= probe;
        }
        ++ecx;
    } while (ecx < 0x10);
    const std::uint32_t remainder = std::uint32_t(value) - eax * eax;
    if (remainder >= std::uint32_t(eax - 1)) eax += 1;
    return std::int32_t(eax);
}
std::int32_t mul_shr22(std::int32_t left, std::int32_t right) noexcept {
    return std::int32_t((std::int64_t(left) * std::int64_t(right)) >> 22);
}
std::int32_t mul_shr16(std::int32_t left, std::int32_t right) noexcept {
    return std::int32_t((std::int64_t(left) * std::int64_t(right)) >> 16);
}
std::int32_t dot_shr16(std::int32_t left, std::int32_t right) noexcept {
    return std::int32_t((std::int64_t(left) * std::int64_t(right)) >> 16);
}
std::int32_t wrap_add(std::int32_t left, std::int32_t right) noexcept {
    return std::int32_t(std::uint32_t(left) + std::uint32_t(right));
}
} // namespace

const std::array<std::int32_t, 2561> kSineTableQ22=[] {
    std::array<std::int32_t,2561> values{};
    for(std::size_t i=0;i<values.size();++i)
        values[i]=std::int32_t(std::sin(double(i)*0.00306796162109375)*4194304.0);
    return values;
}();

void sincos_q22(std::uint32_t angle, std::int32_t& sin_q22, std::int32_t& cos_q22) noexcept {
    // 4776C0: linear interpolation over the 2561-entry Q22 table; the cosine
    // base is the pointer at [0x4FB2F8] = 0xBD0868 (table index + 512).
    const std::uint32_t index = angle >> 21;
    const std::int32_t fraction = std::int32_t(angle & 0x1FFFFF);
    const auto interpolate = [index, fraction](const std::int32_t* base) noexcept {
        const std::int32_t first = base[index];
        const std::int32_t delta = base[index + 1] - first;
        return first + std::int32_t((std::int64_t(delta) * fraction) >> 21);
    };
    sin_q22 = interpolate(kSineTableQ22.data());
    cos_q22 = interpolate(kSineTableQ22.data() + 512);
}

std::uint16_t WeaponRandom::next() noexcept { // 477860
    const std::int32_t previous = std::int32_t(state);
    std::int32_t value = rol32(std::uint32_t(previous), 11);
    value = wrap_add(value, previous);
    value = rol32(std::uint32_t(value), 4) ^ 1;
    state = std::uint32_t(value);
    return std::uint16_t(value & 0xFFFF);
}

Matrix4 matrix_identity() noexcept { // 45C910
    Matrix4 out;
    out.m[0] = out.m[5] = out.m[10] = 0x400000;
    return out;
}

Matrix4 matrix_rotation(std::uint32_t axis, std::int32_t first, std::int32_t second) noexcept {
    // 45C970 with the caller convention: `first` is the first sincos output
    // (table 0xBD0068) and `second` the second output (0xBD0868).
    Matrix4 out;
    if (axis == 0) {
        out.m[0] = 0x400000;
        out.m[5] = first;
        out.m[6] = -second;
        out.m[9] = second;
        out.m[10] = first;
    } else if (axis == 1) {
        out.m[0] = first;
        out.m[2] = -second;
        out.m[5] = 0x400000;
        out.m[8] = second;
        out.m[10] = first;
    } else {
        out.m[0] = first;
        out.m[1] = -second;
        out.m[4] = second;
        out.m[5] = first;
        out.m[10] = 0x400000;
    }
    return out;
}

Matrix4 matrix_translation(Fixed3 value) noexcept { // 45CAE0
    Matrix4 out;
    out.m[0] = out.m[5] = out.m[10] = 0x400000;
    out.m[3] = value.x;
    out.m[7] = value.y;
    out.m[11] = value.z;
    return out;
}

Matrix4 matrix_multiply(const Matrix4& left, const Matrix4& right) noexcept { // 45CB40
    Matrix4 out;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 4; ++column) {
            std::int32_t value = column == 3 ? left.m[row * 4 + 3] : 0;
            for (int k = 0; k < 3; ++k)
                value = wrap_add(value, mul_shr22(left.m[row * 4 + k], right.m[k * 4 + column]));
            out.m[row * 4 + column] = value;
        }
    }
    return out;
}

Fixed3 transform_point(const Matrix4& matrix, const Fixed3& point) noexcept { // 45CDA0
    const std::int32_t source[3] = {point.x, point.y, point.z};
    Fixed3 out;
    std::int32_t* target[3] = {&out.x, &out.y, &out.z};
    for (int row = 0; row < 3; ++row) {
        std::int32_t value = matrix.m[row * 4 + 3];
        for (int k = 0; k < 3; ++k)
            value = wrap_add(value, mul_shr22(matrix.m[row * 4 + k], source[k]));
        *target[row] = value;
    }
    return out;
}

Matrix4 compose_pose(Fixed3 position, Angle3 angles) noexcept { // 45D1E0
    // param_2 = {x, y, z, yaw, pitch, roll}: roll -> axis 0, pitch -> axis 1,
    // yaw -> axis 2, then diagonal(position) * accumulated rotation.
    Matrix4 accumulated = matrix_identity();
    const auto apply = [&accumulated](std::uint32_t axis, std::uint32_t angle) noexcept {
        if (angle == 0) return;
        std::int32_t sin_value = 0, cos_value = 0;
        sincos_q22(angle, sin_value, cos_value);
        // 45D1E0 passes the second sincos output (cos) in the ecx slot and the
        // first (sin) in the edx slot of 45C970.
        accumulated = matrix_multiply(matrix_rotation(axis, cos_value, sin_value), accumulated);
    };
    apply(0, angles.roll);
    apply(1, angles.pitch);
    apply(2, angles.yaw);
    return matrix_multiply(matrix_translation(position), accumulated);
}

MuzzleResult compute_muzzle(const MuzzleInputs& inputs) noexcept { // 482D90
    MuzzleResult out;
    out.aim.yaw = inputs.weapon_aim.yaw + inputs.actor_angles.yaw;
    out.aim.pitch = inputs.weapon_aim.pitch + inputs.actor_angles.pitch;
    out.aim.roll = inputs.weapon_aim.roll + inputs.actor_angles.roll;
    Fixed3 offset;
    // 482D90 shifts the weapon offsets by 8 as bit patterns (wrap, sign free).
    offset.x = std::int32_t(std::uint32_t(inputs.weapon_muzzle.x) << 8) + inputs.view_delta.x;
    offset.y = std::int32_t(std::uint32_t(inputs.weapon_muzzle.y) << 8) + inputs.view_delta.y;
    offset.z = std::int32_t(std::uint32_t(inputs.weapon_muzzle.z) << 8) + inputs.view_delta.z;
    Matrix4 matrix = matrix_identity();
    std::int32_t sin_value = 0, cos_value = 0;
    sincos_q22(inputs.actor_angles.pitch, sin_value, cos_value);
    matrix = matrix_multiply(matrix_rotation(1, cos_value, sin_value), matrix);
    sincos_q22(inputs.actor_angles.yaw, sin_value, cos_value);
    matrix = matrix_multiply(matrix_rotation(2, cos_value, sin_value), matrix);
    const Fixed3 local = transform_point(matrix, offset);
    out.origin.x = inputs.actor_position.x + local.x;
    out.origin.y = inputs.actor_position.y + local.y;
    out.origin.z = inputs.actor_position.z + inputs.eye_height + local.z;
    return out;
}

Fixed3 compute_barrel_tip(Fixed3 muzzle, Angle3 aim, Fixed3 mesh_offset) noexcept { // 4847D0
    return transform_point(compose_pose(muzzle, aim), mesh_offset);
}

SpawnPlan plan_spawn(std::uint32_t weapon_index, std::uint8_t weapon_class,
                     std::int32_t burst_counter, std::int32_t class_spread,
                     WeaponRandom& rng, Angle3& aim) noexcept {
    // 45EA00: spread then the per-weapon kind switch. RNG and the actor item
    // definition burst counter (+0x6C) are explicit inputs.
    if (class_spread != 1) {
        const std::int32_t first = rng.next();
        const std::int32_t second = rng.next();
        const std::int32_t coefficient = (class_spread - 1) * 0x1E;
        aim.yaw = std::uint32_t(aim.yaw + std::uint32_t(coefficient * (0x8000 - first)));
        aim.pitch = std::uint32_t(aim.pitch + std::uint32_t(coefficient * (0x8000 - second)));
    }
    SpawnPlan plan;
    plan.burst_after = burst_counter;
    switch (weapon_index) {
        case 1: plan.melee = true; break;
        case 4: case 5: case 10: case 0x21: case 0x1C:
            plan.kind = weapon_class;
            plan.spawns = true;
            break;
        case 6: case 7:
            plan.spawns = true;
            if (burst_counter != 0) { plan.kind = 9; plan.burst_after = burst_counter - 1; }
            else if (weapon_index == 6) { plan.kind = 8; plan.burst_after = 2; }
            else plan.kind = 7;
            break;
        case 8: case 9:
            plan.spawns = true;
            if (burst_counter != 0) { plan.kind = 9; plan.burst_after = burst_counter - 1; }
            else if (weapon_index == 8) { plan.kind = 8; plan.burst_after = 2; }
            else plan.kind = 7;
            break;
        case 0xC: case 0xD: plan.kind = 0x0E; plan.spawns = true; break;
        case 0xF: plan.kind = 0x0F; plan.spawns = true; break;
        case 0x14: case 0x16: plan.kind = 4; plan.spawns = true; break;
        case 0x15:
            plan.spawns = true;
            if (burst_counter != 0) { plan.kind = 6; plan.burst_after = burst_counter - 1; }
            else { plan.kind = 5; plan.burst_after = 2; }
            break;
        case 0x20: plan.kind = 0x16; plan.spawns = true; break;
        case 0x23:
            plan.spawns = true;
            if (burst_counter != 0) { plan.kind = 0x18; plan.burst_after = burst_counter - 1; }
            else { plan.kind = 0x17; plan.burst_after = 2; }
            break;
        case 0x24:
            plan.spawns = true;
            if (burst_counter != 0) { plan.kind = 0x1A; plan.burst_after = burst_counter - 1; }
            else { plan.kind = 0x19; plan.burst_after = 2; }
            break;
        default: break;
    }
    return plan;
}

SpawnRecord build_spawn_record(Fixed3 origin, Angle3 aim, std::uint16_t actor_slot,
                               std::uint8_t kind, const GroundHeightFn& ground) {
    // 46FB40: quantize x/y to 256 units, pack the height above ground in the
    // freed low bytes, keep the aim high words and the kind byte.
    const std::int32_t x = origin.x & ~0xFF;
    const std::int32_t y = origin.y & ~0xFF;
    const std::int32_t z = origin.z & ~0xFF;
    const std::int32_t delta = z - ground(x, y);
    SpawnRecord record;
    record.packed_x = std::uint32_t(x) | (std::uint32_t(delta >> 16) & 0xFF);
    record.packed_y = std::uint32_t(y) | (std::uint32_t(delta >> 8) & 0xFF);
    record.actor_slot = actor_slot;
    record.aim_yaw_word = std::uint16_t(aim.yaw >> 16);
    record.aim_pitch_word = std::uint16_t(aim.pitch >> 16);
    record.kind = kind;
    return record;
}

DecodedSpawn decode_spawn_record(const SpawnRecord& record, const GroundHeightFn& ground) {
    // 46EA40: inverse of build_spawn_record; the z low byte is lost (256-unit
    // quantization) and is restored from the ground height.
    std::int32_t delta = std::int32_t((record.packed_x & 0xFF) << 16) |
                         std::int32_t((record.packed_y & 0xFF) << 8);
    if ((delta & 0x800000) != 0) delta |= std::int32_t(0xFF000000);
    DecodedSpawn out;
    out.origin.x = std::int32_t(record.packed_x & ~std::uint32_t(0xFF));
    out.origin.y = std::int32_t(record.packed_y & ~std::uint32_t(0xFF));
    out.origin.z = ground(out.origin.x, out.origin.y) + delta;
    out.aim.yaw = std::uint32_t(record.aim_yaw_word) << 16;
    out.aim.pitch = std::uint32_t(record.aim_pitch_word) << 16;
    out.aim.roll = 0;
    out.kind = record.kind;
    out.actor_slot = record.actor_slot;
    return out;
}

ProjectileEntity spawn_projectile(Fixed3 origin, Angle3 aim, const ClassSpawnFields& fields,
                                  const std::array<std::int32_t, 2>& item_pointers,
                                  std::int32_t item_field, std::int32_t owner_slot,
                                  std::uint32_t team_color, bool global_8cf982_bit4) noexcept {
    // 462F20: initial ballistic vector from the two aim angle words, class
    // speed / 62 per tick, entity bookkeeping. Item pointers, the item field,
    // owner slot, team color and the 0x8CF982 global bit are explicit inputs.
    ProjectileEntity entity;
    std::int32_t sin_yaw = 0, cos_yaw = 0, sin_pitch = 0, cos_pitch = 0;
    sincos_q22(aim.yaw & ~std::uint32_t(0xFFFF), sin_yaw, cos_yaw);
    sincos_q22(aim.pitch & ~std::uint32_t(0xFFFF), sin_pitch, cos_pitch);
    const std::int32_t direction_x = mul_shr22(cos_yaw, cos_pitch);
    const std::int32_t direction_y = mul_shr22(sin_yaw, cos_pitch);
    const std::int32_t direction_z = sin_pitch;
    const std::int32_t step = std::int32_t(std::uint32_t(fields.speed) << 16) / 0x3E;
    entity.position = origin;
    entity.aim_yaw = aim.yaw;
    entity.aim_pitch = aim.pitch;
    entity.velocity.x = mul_shr22(direction_x, step);
    entity.velocity.y = mul_shr22(direction_y, step);
    entity.velocity.z = mul_shr22(direction_z, step);
    entity.class_flags = fields.flags;
    entity.field_7c = fields.field_14;
    entity.field_80 = fields.field_10;
    entity.field_84 = fields.field_18;
    entity.owner_slot = owner_slot;
    entity.ttl = std::int32_t(fields.field_18);
    if (owner_slot >= 0)
        entity.color = global_8cf982_bit4 ? 0x8000FF00 : team_color;
    const std::uint32_t flags = fields.flags;
    if ((flags & 0x01) != 0 || (flags & 0x40) != 0) {
        entity.item_main = item_pointers[0];
        entity.item_alt = item_pointers[1];
        entity.item_field = item_field;
        entity.color = 0xFFFFFFFF;
    } else if ((flags & 0x10) != 0) {
        entity.item_main = item_pointers[0];
        entity.item_alt = item_pointers[1];
        entity.item_field = item_field;
    }
    return entity;
}

void tick_projectile(ProjectileEntity& entity, Fixed3 world_center,
                     const ProjectileQuery& query, std::vector<ProjectileEvent>& events) {
    // 464EB0 record pass for one entity. Effects, damage and sounds at the
    // impact stay outside; only the record state and the dispatch entry are
    // reproduced.
    if (entity.ttl <= 0) { // 464ED2: clear the item pointer only
        entity.item_main = 0;
        return;
    }
    if (entity.position.z < -65536) { // 464EE4: clear ttl and item
        entity.ttl = 0;
        entity.item_main = 0;
        return;
    }
    const std::int64_t square_sum = ((std::int64_t(entity.velocity.x) * entity.velocity.x) >> 16) +
                                    ((std::int64_t(entity.velocity.y) * entity.velocity.y) >> 16) +
                                    ((std::int64_t(entity.velocity.z) * entity.velocity.z) >> 16);
    const std::int32_t square = std::int32_t(std::uint32_t(std::int32_t(square_sum)));
    const std::int32_t length = isqrt32(square) << 8;
    if (length == 0) {
        entity.velocity.z -= 0xB2;
        entity.ttl -= 1;
        return;
    }
    const std::int32_t factor = std::int32_t(0x100000000LL / std::int64_t(length));
    const Fixed3 step{mul_shr16(entity.velocity.x, factor), mul_shr16(entity.velocity.y, factor),
                      mul_shr16(entity.velocity.z, factor)};
    const Fixed3 start = entity.position;
    Fixed3 next{wrap_add(start.x, entity.velocity.x), wrap_add(start.y, entity.velocity.y),
                wrap_add(start.z, entity.velocity.z)};
    std::int32_t distance = length;
    std::uint8_t hit_type = 0;
    std::int32_t reported = 0;
    const auto accept = [&distance, &hit_type](std::int32_t candidate, std::uint8_t type) noexcept {
        if (candidate < distance) {
            distance = candidate;
            hit_type = type;
        }
    };
    const std::int32_t terrain = query.terrain ? query.terrain(entity, step, length, reported) : 1;
    if (terrain != 1) distance = -terrain;
    if (query.actors_a && query.actors_a(entity, step, length, reported = 0) == 0) accept(reported, 1);
    if (query.actors_b && query.actors_b(entity, step, length, reported = 0) == 0) accept(reported, 2);
    if (query.special_a && query.special_a(entity, step, length, reported = 0) == 0) accept(reported, 3);
    if (query.special_b_enabled && query.special_b &&
        query.special_b(entity, step, length, reported = 0) == 0)
        accept(reported, 4);
    const auto emit_event = [&](const ProjectileEvent& event) {
        events.push_back(event);
        if(query.on_event)query.on_event(entity,event);
    };
    const auto record_move = [&](const Fixed3& to) {
        bool emit = false;
        if (query.alternate_move_filter) emit = (entity.class_flags & 0x60) != 0;
        else emit = (entity.class_flags & 0x90) == 0;
        if (emit) emit_event({ProjectileEventKind::move, 0, to, start});
    };
    const auto edge_check = [&]() {
        const std::int32_t delta_x = std::abs(start.x - world_center.x);
        const std::int32_t delta_y = std::abs(start.y - world_center.y);
        if (delta_x >= 0x280000 || delta_y >= 0x280000) return;
        const std::int32_t dot = dot_shr16(wrap_add(world_center.x, -start.x), step.x) +
                                 dot_shr16(wrap_add(world_center.y, -start.y), step.y) +
                                 dot_shr16(wrap_add(world_center.z, -start.z), step.z);
        if (dot >= 0 && dot < length) {
            const Fixed3 crossing{wrap_add(start.x, mul_shr16(step.x, dot)),
                                  wrap_add(start.y, mul_shr16(step.y, dot)),
                                  wrap_add(start.z, mul_shr16(step.z, dot))};
            emit_event({ProjectileEventKind::world_edge, 0x41, crossing, {}});
        }
    };
    Fixed3 final_position = next;
    if (distance < length) {
        const std::int32_t slide = distance - 0x4000;
        final_position = Fixed3{wrap_add(start.x, mul_shr16(step.x, slide)),
                                wrap_add(start.y, mul_shr16(step.y, slide)),
                                wrap_add(start.z, mul_shr16(step.z, slide))};
        emit_event({ProjectileEventKind::impact, hit_type, final_position, {}});
        entity.ttl = 0;
        entity.item_main = 0;
    }
    // shared tail: record move, world edge check, store, gravity, ttl
    record_move(final_position);
    edge_check();
    entity.position = final_position;
    if ((entity.class_flags & 0x40) == 0) entity.velocity.z -= 0xB2;
    entity.ttl -= 1;
}
} // namespace dfre
