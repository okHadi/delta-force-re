#include "dfre/object_live_callbacks.hpp"
#include "dfre/world_hits.hpp"

#include <algorithm>
#include <cstring>
#include <string>

namespace dfre {
namespace {

using Image = ObjectBehaviorActorImage;

std::uint32_t read_u32(const std::uint8_t* bytes, std::size_t off) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes + off, sizeof(value));
    return value;
}

std::uint16_t read_u16(const std::uint8_t* bytes, std::size_t off) {
    std::uint16_t value = 0;
    std::memcpy(&value, bytes + off, sizeof(value));
    return value;
}

std::uint8_t read_u8(const std::uint8_t* bytes, std::size_t off) {
    return bytes[off];
}

void write_u32(std::uint8_t* bytes, std::size_t off, std::uint32_t value) {
    std::memcpy(bytes + off, &value, sizeof(value));
}

void write_u16(std::uint8_t* bytes, std::size_t off, std::uint16_t value) {
    std::memcpy(bytes + off, &value, sizeof(value));
}

void write_u8(std::uint8_t* bytes, std::size_t off, std::uint8_t value) {
    bytes[off] = value;
}

std::int32_t as_i32(std::uint32_t value) {
    return static_cast<std::int32_t>(value);
}

std::uint32_t abs_i32(std::uint32_t value) {
    const std::uint32_t sign = (value & 0x80000000u) != 0 ? 0xFFFFFFFFu : 0u;
    return (value ^ sign) - sign;
}

// Low 32 bits of (signed a * signed b) >> shift, the original
// IMUL + SHRD EAX,EDX,imm pattern (fixed point 22 for sincos products).
std::uint32_t mul_shr(std::uint32_t a, std::uint32_t b, unsigned shift) {
    const std::int64_t product =
        static_cast<std::int64_t>(as_i32(a)) * static_cast<std::int64_t>(as_i32(b));
    return static_cast<std::uint32_t>(product >> shift);
}

std::int32_t sar(std::uint32_t value, unsigned amount) {
    return as_i32(value) >> amount;
}

// ---- actor image fields at their original offsets ----

std::uint32_t a_u32(const ObjectBehaviorActor& self, std::size_t off) {
    return read_u32(self.image.data(), off);
}

std::uint16_t a_u16(const ObjectBehaviorActor& self, std::size_t off) {
    return read_u16(self.image.data(), off);
}

// The original compares +0x74 (health) as a signed 16-bit value (JLE/JG).
std::int16_t a_i16(const ObjectBehaviorActor& self, std::size_t off) {
    return static_cast<std::int16_t>(read_u16(self.image.data(), off));
}

std::uint8_t a_u8(const ObjectBehaviorActor& self, std::size_t off) {
    return read_u8(self.image.data(), off);
}

void a_w32(ObjectBehaviorActor& self, std::size_t off, std::uint32_t value) {
    write_u32(self.image.data(), off, value);
}

void a_w16(ObjectBehaviorActor& self, std::size_t off, std::uint16_t value) {
    write_u16(self.image.data(), off, value);
}

void a_w8(ObjectBehaviorActor& self, std::size_t off, std::uint8_t value) {
    write_u8(self.image.data(), off, value);
}

// ---- foreign data ----

std::uint8_t* person_bytes(ObjectBehaviorActor& self, const ObjectBehaviorWorld& world) {
    if (!world.person_image)
        throw ObjectBehaviorError("object_behavior: person resolver is missing");
    std::uint8_t* bytes = world.person_image(a_u32(self, 0x0C));
    if (!bytes)
        throw ObjectBehaviorError("object_behavior: person resolver did not find the record");
    return bytes;
}



std::uint32_t p_u32(ObjectBehaviorActor& self, const ObjectBehaviorWorld& world, std::size_t off) {
    return read_u32(person_bytes(self, world), off);
}

void p_w32(ObjectBehaviorActor& self, const ObjectBehaviorWorld& world, std::size_t off,
           std::uint32_t value) {
    write_u32(person_bytes(self, world), off, value);
}



std::uint32_t definition_field(const ObjectBehaviorActor& self, const ObjectBehaviorWorld& world,
                               std::uint32_t offset) {
    if (!world.definition_dword)
        throw ObjectBehaviorError("object_behavior: definition resolver is missing");
    return world.definition_dword(a_u32(self, 0x04), offset);
}

std::uint32_t type_slot_field(const ObjectBehaviorActor& self, const ObjectBehaviorWorld& world,
                              std::uint32_t offset) {
    if (!world.type_slot_dword)
        throw ObjectBehaviorError("object_behavior: type slot resolver is missing");
    return world.type_slot_dword(std::uint32_t(sar(a_u32(self,0x70),16)), offset);
}

std::uint32_t global_field(const ObjectBehaviorWorld& world, std::uint32_t address) {
    if (!world.global_dword)
        throw ObjectBehaviorError("object_behavior: global resolver is missing");
    return world.global_dword(address);
}

const std::uint8_t* other_actor(const ObjectBehaviorWorld& world, std::uint32_t raw) {
    if (!world.actor_image)
        throw ObjectBehaviorError("object_behavior: actor resolver is missing");
    const std::uint8_t* bytes = world.actor_image(raw);
    if (!bytes)
        throw ObjectBehaviorError("object_behavior: actor resolver did not find the record");
    return bytes;
}

void apply_patches(ObjectBehaviorActor& self, ObjectBehaviorState& state,
                   const ObjectBehaviorWorld& world, const ObjectBehaviorInput& input) {
    for (const ObjectBehaviorPatch& patch : input.patches) {
        std::uint8_t* base = nullptr;
        std::size_t offset = 0;
        switch (patch.target) {
        case ObjectBehaviorPatch::Target::actor:
            base = self.image.data();
            offset = patch.address;
            break;
        case ObjectBehaviorPatch::Target::person:
            base = person_bytes(self, world);
            offset = patch.address;
            break;
        case ObjectBehaviorPatch::Target::global:
            if (patch.width != 4)
                throw ObjectBehaviorError("object_behavior: global patch must be 4 bytes");
            if (patch.address == 0xA158C0u)
                state.global_a158c0 = patch.value;
            else if (patch.address == 0xA158BCu)
                state.global_a158bc = patch.value;
            else if (patch.address == 0xBD5248u)
                state.ground_level_bd5248 = patch.value;
            else
                throw ObjectBehaviorError("object_behavior: unknown global patch address");
            continue;
        }
        const std::size_t limit = patch.target == ObjectBehaviorPatch::Target::actor
                                      ? self.image.size()
                                      : kAiPersonRecordBytes;
        if (offset + patch.width > limit)
            throw ObjectBehaviorError("object_behavior: patch is outside the record");
        if (patch.width == 1)
            write_u8(base, offset, static_cast<std::uint8_t>(patch.value));
        else if (patch.width == 2)
            write_u16(base, offset, static_cast<std::uint16_t>(patch.value));
        else if (patch.width == 4)
            write_u32(base, offset, patch.value);
        else
            throw ObjectBehaviorError("object_behavior: unsupported patch width");
    }
}

} // namespace

std::uint32_t object_behavior_rng_next(ObjectBehaviorState& state) {
    const std::uint32_t old = state.rng;
    std::uint32_t value = old;
    value = (value << 11) | (value >> 21);  // rol eax, 0xb
    value += old;
    value = (value << 4) | (value >> 28);  // rol eax, 4
    value ^= 1;
    state.rng = value;
    return value & 0xFFFF;
}

ObjectBehaviorInput object_behavior_call(ObjectBehaviorActor& self,
                                         ObjectBehaviorState& state,
                                         const ObjectBehaviorWorld& world,
                                         ObjectBehaviorCallKind kind,
                                         std::uint32_t site,
                                         const std::array<std::uint32_t, 6>& args,
                                         std::vector<std::uint8_t> payload) {
    if(state.call){
        const ObjectBehaviorAction action{kind,site,args,std::move(payload)};
        state.actions.push_back(action);
        const auto input=state.call(self,state,action);
        if(input.site!=site || !input.patches.empty())
            throw ObjectBehaviorError("object_behavior: live call needs current state and matching outputs");
        return input;
    }
    if (state.cursor >= state.inputs.size())
        throw ObjectBehaviorError("object_behavior: input stream is exhausted");
    if (state.inputs[state.cursor].site != site)
        throw ObjectBehaviorError("object_behavior: input site does not match the original call order");
    state.actions.push_back({kind, site, args, std::move(payload)});
    const ObjectBehaviorInput input = state.inputs[state.cursor++];
    apply_patches(self, state, world, input);
    return input;
}

namespace object_behavior {
namespace {


void veh0_ai_impl(ObjectBehaviorActor& self, ObjectBehaviorState& state,
                  const ObjectBehaviorWorld& world) {
    // 429BD1 keeps ESI across every foreign call. Actor+0C is read again only
    // for the three destination stores at429DCD/429E54/429E5D.
    const auto person_token=a_u32(self,0x0c);
    const auto person=[&](){
        if(!world.person_image)throw ObjectBehaviorError("object_behavior: person resolver is missing");
        auto* bytes=world.person_image(person_token);
        if(!bytes)throw ObjectBehaviorError("object_behavior: cached person is missing");
        return bytes;
    };
    if(state.single_player){
        if(as_i32(a_u32(self,0x7c))==0){
            if(a_u32(self,0x1c)!=0)object_behavior_call(self,state,world,ObjectBehaviorCallKind::effect,0x43f020,{self.token,0u});
        }else if(a_u32(self,0x1c)==0)object_behavior_call(self,state,world,ObjectBehaviorCallKind::effect,0x43ef80,{self.token,0x1fau,0u,0u});
        a_w32(self,0xfc,0x10);return;
    }
    if(a_u8(self,0x6c)&4){
        a_w16(self,0x6c,std::uint16_t(a_u16(self,0x6c)|0x108));a_w32(self,0xfc,0x400);
        if(a_u32(self,0x1c)!=0)object_behavior_call(self,state,world,ObjectBehaviorCallKind::effect,0x43f020,{self.token,0u});
        return;
    }
    if(a_i16(self,0x74)<1){
        a_w16(self,0x6c,std::uint16_t(a_u16(self,0x6c)|0x104));a_w32(self,0x88,0);
        std::array<std::uint8_t,0x18> block{};std::memcpy(block.data(),self.image.data()+0x24,block.size());
        write_u16(block.data(),0x0a,std::uint16_t(read_u32(block.data(),0x0a)+1));
        object_behavior_call(self,state,world,ObjectBehaviorCallKind::effect,0x446fb0,{a_u32(self,0xf8),0u,0x3cu},{block.begin(),block.end()});
        object_behavior_call(self,state,world,ObjectBehaviorCallKind::effect,0x447610,{self.token,0u,0u,0u});
        std::fill_n(self.image.begin()+0x3c,0x18,0);std::fill_n(self.image.begin()+0x54,0x18,0);a_w32(self,0xfc,0x10);return;
    }
    std::uint32_t angle=0,distance=0,radius=0;
    bool failed=false;
    if(read_u32(person(),0xa4)!=0){
        const auto marker=object_behavior_call(self,state,world,ObjectBehaviorCallKind::query,0x45c6a0,{self.token,0u,0u,0u,0u,0u});
        if(marker.output_mask&2)angle=marker.values[1];
        if(marker.output_mask&4)distance=marker.values[2];
        if(marker.output_mask&8)radius=marker.values[3];
        if(marker.values[0]==0){write_u32(person(),0xa4,0);failed=true;}
    }
    if(!failed && read_u32(person(),0xa4)!=0){
        if(as_i32(distance)<=as_i32(radius) || as_i32(distance)<as_i32(abs_i32(a_u32(self,0x7c))*2)){
            object_behavior_call(self,state,world,ObjectBehaviorCallKind::effect,0x42ee30,
                {std::uint32_t(sar(a_u32(self,0x70),16)),read_u32(person(),0xac),read_u32(person(),0xb0)});
            const auto list_index=read_u32(person(),0xac);
            const auto entry=read_u32(person(),0xb0)+1u;write_u32(person(),0xb0,entry);
            if(as_i32(entry)>=as_i32(global_field(world,list_index*0x88u+0x78f284u)))p_w32(self,world,0xb0,0);
            if(read_u32(person(),0xb0)==0 && (global_field(world,read_u32(person(),0xac)*0x88u+0x78f280u)&1)){
                p_w32(self,world,0xa4,0);p_w32(self,world,0xac,0);a_w32(self,0xfc,0x10);return;
            }
            const auto marker=object_behavior_call(self,state,world,ObjectBehaviorCallKind::query,0x45c6a0,{self.token,0u,0u,0u,0u,0u});
            if(marker.output_mask&2)angle=marker.values[1];
        }
        if(angle!=a_u32(self,0x30))a_w32(self,0x8c,angle);
        const auto speed=type_slot_field(self,world,0x3c);
        a_w32(self,0xfc,0x40);a_w32(self,0x88,speed);a_w8(self,0x6d,std::uint8_t(a_u8(self,0x6d)|1));
    }else{a_w32(self,0x88,0);a_w32(self,0xfc,0x40);a_w8(self,0x6d,std::uint8_t(a_u8(self,0x6d)|1));}
    if(a_u32(self,0x7c)==0){
        if(a_u32(self,0x1c)!=0)object_behavior_call(self,state,world,ObjectBehaviorCallKind::effect,0x43f020,{self.token,0u});
    }else if(a_u32(self,0x1c)==0)object_behavior_call(self,state,world,ObjectBehaviorCallKind::effect,0x43ef80,{self.token,0x1fau,0u,0u});
}

void veh0_move_impl(ObjectBehaviorActor& self, ObjectBehaviorState& state,
                    const ObjectBehaviorWorld& world) {
    if (as_i32(a_u32(self, 0x7C)) != 0) {
        const std::uint32_t yaw = a_u32(self, 0x30);
        const std::uint32_t target = a_u32(self, 0x8C);
        const std::uint32_t step = static_cast<std::uint32_t>(sar(target - yaw, 3));
        const ObjectBehaviorInput turn = object_behavior_call(
            self, state, world, ObjectBehaviorCallKind::query, 0x477820, {step, 0u, 0u});
        const std::uint32_t out_a = turn.values[1];
        const std::uint32_t out_b = turn.values[2];
        const std::int32_t def_e4 = as_i32(definition_field(self, world, 0xE4));
        if (def_e4 == 0)
            throw ObjectBehaviorError("object_behavior: veh0 turn rate is zero");
        const std::uint32_t rate = static_cast<std::uint32_t>(0x20000000 / def_e4);
        const std::uint32_t scale = a_u32(self, 0x7C);
        const std::uint32_t delta =
            static_cast<std::uint32_t>(sar(static_cast<std::uint32_t>(
                                               (mul_shr(scale, out_a, 22) * rate)), 6));
        const std::uint32_t scaled_b = mul_shr(scale, out_b, 22);
        const std::uint32_t table_index = delta >> 21;
        const auto current_def_e4=definition_field(self,world,0xe4);
        const std::uint32_t table_value = global_field(world, table_index * 4u + 0xBD0068u);
        std::uint32_t extra =
            mul_shr(table_value, current_def_e4, 22);
        extra = static_cast<std::uint32_t>(sar(extra, 1));
        a_w32(self, 0x30, a_u32(self, 0x30) + delta);
        const ObjectBehaviorInput forward = object_behavior_call(
            self, state, world, ObjectBehaviorCallKind::query, 0x477820,
            {a_u32(self, 0x30), 0u, 0u});
        const std::uint32_t out_c = forward.values[1];
        const std::uint32_t out_d = forward.values[2];
        a_w32(self, 0x3C, static_cast<std::uint32_t>(sar(mul_shr(scaled_b, out_d, 22), 6)));
        a_w32(self, 0x40, static_cast<std::uint32_t>(sar(mul_shr(scaled_b, out_c, 22), 6)));
        a_w32(self, 0x3C, a_u32(self, 0x3C) +
                              static_cast<std::uint32_t>(sar(mul_shr(extra, out_c, 22), 6)));
        a_w32(self, 0x40, a_u32(self, 0x40) +
                              static_cast<std::uint32_t>(sar(mul_shr(extra, out_d, 22), 6)));
        a_w32(self, 0x24, a_u32(self, 0x24) + a_u32(self, 0x3C));
        a_w32(self, 0x28, a_u32(self, 0x28) + a_u32(self, 0x40));
        a_w32(self, 0x94, step);
        a_w32(self, 0x90, delta);
    }
    if (a_u32(self, 0x7C) != a_u32(self, 0x88)) {
        const std::uint32_t difference = a_u32(self, 0x88) - a_u32(self, 0x7C);
        const auto flags=a_u8(self,0x6c);
        std::uint32_t limit = definition_field(self, world, 0xDC);
        if ((flags & 4) != 0)
            limit *= 2;
        if (as_i32(abs_i32(difference)) < as_i32(limit)) {
            a_w32(self, 0x7C, a_u32(self, 0x88));
        } else if (as_i32(difference) < 1) {
            a_w32(self, 0x7C, a_u32(self, 0x7C) - limit);
        } else {
            a_w32(self, 0x7C, a_u32(self, 0x7C) + limit);
        }
    }
    const ObjectBehaviorInput facing = object_behavior_call(
        self, state, world, ObjectBehaviorCallKind::query, 0x477820,
        {a_u32(self, 0x30), 0u, 0u});
    const std::uint32_t s1 = mul_shr(facing.values[1], 0x2C000, 22);
    const std::uint32_t s2 = mul_shr(facing.values[2], 0x2C000, 22);
    const ObjectBehaviorInput front = object_behavior_call(
        self, state, world, ObjectBehaviorCallKind::query, 0x47DF40,
        {a_u32(self,0x24) + s2, a_u32(self,0x28) + s1, 0u, 0u, 0u, 0u});
    const ObjectBehaviorInput back = object_behavior_call(
        self, state, world, ObjectBehaviorCallKind::query, 0x47DF40,
        {a_u32(self,0x24) - s2, a_u32(self,0x28) - s1, 0u, 0u, 0u, 0u});
    a_w32(self, 0x34, (front.values[0] - back.values[0]) << 11);
    const std::uint32_t s1_quarter = static_cast<std::uint32_t>(sar(s1, 2));
    const std::uint32_t s2_quarter = static_cast<std::uint32_t>(sar(s2, 2));
    const ObjectBehaviorInput left = object_behavior_call(
        self, state, world, ObjectBehaviorCallKind::query, 0x47DF40,
        {a_u32(self,0x24) - s1_quarter, a_u32(self,0x28) + s2_quarter, 0u, 0u, 0u, 0u});
    const ObjectBehaviorInput right = object_behavior_call(
        self, state, world, ObjectBehaviorCallKind::query, 0x47DF40,
        {a_u32(self,0x24) + s1_quarter, a_u32(self,0x28) - s2_quarter, 0u, 0u, 0u, 0u});
    a_w32(self, 0x38, (left.values[0] - right.values[0]) << 13);
    a_w32(self, 0x2C, static_cast<std::uint32_t>(sar(front.values[0] + back.values[0], 1)));
    const std::uint32_t slot_value = type_slot_field(self,world,0x3c);
    if (slot_value == 0)
        return;
    const std::int64_t numerator = static_cast<std::int64_t>(as_i32(a_u32(self, 0x7C))) * 65536;
    const auto quotient=numerator/as_i32(slot_value);
    if(quotient<-2147483648LL || quotient>2147483647LL)throw ObjectBehaviorError("object_behavior: veh0 ratio division overflows");
    a_w32(self, 0x20, static_cast<std::uint32_t>(quotient) + 0x10000);
}

void slgt_ai_impl(ObjectBehaviorActor& self, ObjectBehaviorState& state,
                  const ObjectBehaviorWorld& world) {
    auto* const person=person_bytes(self,world);
    const auto target=read_u32(person,0x44);
    if (state.single_player) {
        a_w32(self, 0xFC, 0x400);
        return;
    }
    if ((a_u8(self, 0x6C) & 4) != 0) {
        a_w32(self, 0xFC, 0x400);
        a_w8(self, 0x6D, static_cast<std::uint8_t>(a_u8(self, 0x6D) | 1));
        return;
    }
    if (a_i16(self, 0x74) < 1) {
        a_w16(self, 0x6C, static_cast<std::uint16_t>(a_u16(self, 0x6C) | 0x010C));
        a_w32(self, 0x8C, a_u32(self, 0x30));
        std::array<std::uint8_t, 0x18> block{};
        std::memcpy(block.data(), self.image.data() + 0x24, block.size());
        const std::uint32_t bumped = read_u32(block.data(), 0x0A) + 1;
        write_u16(block.data(), 0x0A, static_cast<std::uint16_t>(bumped));
        object_behavior_call(self, state, world, ObjectBehaviorCallKind::effect, 0x446FB0,
                             {a_u32(self, 0xF8), 0u, 0u}, {block.begin(), block.end()});
        std::fill_n(self.image.begin() + 0x3C, 0x18, static_cast<std::uint8_t>(0));
        std::fill_n(self.image.begin() + 0x54, 0x18, static_cast<std::uint8_t>(0));
        return;
    }
    if (a_u32(self, 0xE0) == 0) {
        a_w32(self, 0xE8, 0x20000000);
        a_w32(self, 0xE0, a_u32(self, 0x30) | 1);
        a_w32(self, 0xE4, a_u32(self, 0x34));
        a_w32(self, 0x8C, a_u32(self, 0xE0) + a_u32(self, 0xE8));
        a_w32(self, 0xF0, a_u32(self, 0x34));
    }
    if (target != 0) {
        const std::uint32_t remaining = read_u32(person, 0x38) - 1;
        write_u32(person, 0x38, remaining);
        if (as_i32(remaining) > 0) {
            if ((read_u32(person, 0x24) & 0x40) == 0) {
                const std::uint8_t* other = other_actor(world, target);
                if ((read_u8(other, 0x6C) & 4) != 0)
                    write_u8(person,0x24,std::uint8_t(read_u8(person,0x24)|0x40));
                write_u32(person, 0x48, read_u32(other, 0x24));
                write_u32(person, 0x4C, read_u32(other, 0x28));
                write_u32(person, 0x50, read_u32(other, 0x2C));
                const std::uint32_t dx = read_u32(person, 0x48) - a_u32(self, 0x24);
                const std::uint32_t dy = read_u32(person, 0x4C) - a_u32(self, 0x28);
                const std::uint32_t dz = read_u32(person, 0x50) - a_u32(self, 0x2C);
                const auto hx=std::uint32_t(sar(dx,16)),hy=std::uint32_t(sar(dy,16));
                const auto squared=hx*hx+hy*hy;
                const std::uint32_t horizontal=std::uint32_t(collision_sqrt(as_i32(squared)))<<16;
                const ObjectBehaviorInput heading = object_behavior_call(
                    self, state, world, ObjectBehaviorCallKind::query, 0x477770,
                    {dx, dy, 0u});
                std::uint32_t turn = heading.values[0] - a_u32(self, 0xE0);
                turn += (object_behavior_rng_next(state) & 0xFFFF) * 0x40;
                turn -= 0x200000;
                if (as_i32(turn) > 0x20000000)
                    turn = 0x20000000;
                if (as_i32(turn) < -0x20000000)
                    turn = static_cast<std::uint32_t>(-0x20000000);
                a_w32(self, 0x8C, turn + a_u32(self, 0xE0));
                const ObjectBehaviorInput depression = object_behavior_call(
                    self, state, world, ObjectBehaviorCallKind::query, 0x477770,
                    {horizontal, dz, 0u});
                std::uint32_t pitch = depression.values[0] - a_u32(self, 0xE4);
                pitch += (object_behavior_rng_next(state) & 0xFFFF) * 0x40;
                pitch -= 0x200000;
                if (as_i32(pitch) > 0x8000000)
                    pitch = 0x8000000;
                if (as_i32(pitch) < -0x8000000)
                    pitch = static_cast<std::uint32_t>(-0x8000000);
                a_w32(self, 0xFC, 0x20);
                a_w32(self, 0xF0, pitch + a_u32(self, 0xE4));
                a_w8(self, 0x6D, static_cast<std::uint8_t>(a_u8(self, 0x6D) | 1));
                return;
            }
            write_u32(person, 0x44, 0);
        }
    }
    {
        const ObjectBehaviorInput found = object_behavior_call(
            self, state, world, ObjectBehaviorCallKind::query, 0x44F540,
            {self.token, 0u, 0u});
        const std::uint32_t candidate = found.values[0];
        if (candidate != 0) {
            const std::uint32_t draw = object_behavior_rng_next(state);
            if (as_i32(draw) >= as_i32(found.values[1])) {
                a_w32(self, 0xFC, 0x10);
                return;
            }
            const std::uint8_t* other = other_actor(world, candidate);
            object_behavior_call(self, state, world, ObjectBehaviorCallKind::effect, 0x42EDD0,
                                 {std::uint32_t(sar(a_u32(self,0x70),16)), std::uint32_t(sar(read_u32(other,0x70),16))});
            write_u32(person, 0x38, 8);
            write_u32(person, 0x44, candidate);
            write_u8(person,0x24,std::uint8_t(read_u8(person,0x24)&0xbfu));
            a_w32(self, 0xFC, 0x10);
            return;
        }
    }
    write_u32(person, 0x44, 0);
    a_w32(self, 0xFC, 0x10);
}

void slgt_sweep_impl(ObjectBehaviorActor& self,const ObjectBehaviorWorld& world){
    const auto yaw=a_u32(self,0x30),target=a_u32(self,0x8c);a_w32(self,0x90,0);
    if(yaw!=target){
        const auto difference=std::uint32_t(sar(target-yaw,16));
        const auto speed=a_u32(self,0xec);
        const auto first_rate=definition_field(self,world,0xe0)<<3;
        if(as_i32(abs_i32(difference))>as_i32(first_rate)){
            const auto current_speed=a_u32(self,0xec);
            const auto rate=definition_field(self,world,0xe0);
            if(as_i32(current_speed)<as_i32(rate))a_w32(self,0xec,current_speed+std::uint32_t(sar(rate,3)));
        }else{
            const auto step=std::uint32_t(sar(definition_field(self,world,0xe0),3));
            const auto current_speed=a_u32(self,0xec);
            if(as_i32(current_speed)>as_i32(step))a_w32(self,0xec,current_speed-step);
        }
        if(as_i32(abs_i32(difference))<as_i32(speed))a_w32(self,0x30,a_u32(self,0x8c));
        else if(as_i32(difference)>0){const auto step=speed<<16;a_w32(self,0x90,step);a_w32(self,0x30,a_u32(self,0x30)+step);}
        else{const auto step=speed<<16;a_w32(self,0x30,a_u32(self,0x30)-step);a_w32(self,0x90,(0u-speed)<<16);}
    }
    const auto pitch=a_u32(self,0x34);
    if(pitch==a_u32(self,0xf0))return;
    const auto difference=std::uint32_t(sar(a_u32(self,0xf0)-pitch,16));
    const auto rate=definition_field(self,world,0xe0);
    if(as_i32(abs_i32(difference))<as_i32(rate))a_w32(self,0x34,a_u32(self,0xf0));
    else if(as_i32(difference)>0)a_w32(self,0x34,a_u32(self,0x34)+(rate<<16));
    else a_w32(self,0x34,a_u32(self,0x34)-(rate<<16));
}

void slgt_move_impl(ObjectBehaviorActor& self,ObjectBehaviorState& state,const ObjectBehaviorWorld& world){
    const auto target=p_u32(self,world,0x44);
    if(a_u8(self,0x6c)&4)return;
    if(state.single_player){slgt_sweep_impl(self,world);return;}
    if(a_u32(self,0xe0)==0)return;
    if(target!=0){slgt_sweep_impl(self,world);return;}
    slgt_sweep_impl(self,world);
    if(a_u32(self,0x90)!=0)return;
    const auto sweep=a_u32(self,0xe8),base=a_u32(self,0xe0);
    const auto flags=a_u8(self,0x6d);
    a_w32(self,0xe8,0u-sweep);a_w8(self,0x6d,std::uint8_t(flags|1));
    a_w32(self,0x8c,base-sweep);a_w32(self,0xf0,a_u32(self,0xe4));
}

} // namespace

void veh0_ai(ObjectBehaviorActor& self, ObjectBehaviorState& state,
             const ObjectBehaviorWorld& world, std::uint32_t param2) {
    (void)param2;
    veh0_ai_impl(self, state, world);
}

void veh0_move(ObjectBehaviorActor& self, ObjectBehaviorState& state,
               const ObjectBehaviorWorld& world) {
    veh0_move_impl(self, state, world);
}

void slgt_ai(ObjectBehaviorActor& self, ObjectBehaviorState& state,
             const ObjectBehaviorWorld& world, std::uint32_t param2) {
    (void)param2;
    slgt_ai_impl(self, state, world);
}

void slgt_move(ObjectBehaviorActor& self, ObjectBehaviorState& state,
               const ObjectBehaviorWorld& world) {
    slgt_move_impl(self, state, world);
}

} // namespace object_behavior
} // namespace dfre
