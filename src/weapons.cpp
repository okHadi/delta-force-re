#include "dfre/weapons.hpp"
#include <cstring>
#include <stdexcept>
namespace dfre {
namespace {
// Original 4F9704, 28 rows of 0x14 bytes, words +4/+8/+C.
constexpr std::array<std::array<std::uint8_t,3>,kWeaponLoadoutCount> kLoadouts{{
    {{0,0,0}},
    {{0,0,0}},
    {{1,0,0}},
    {{2,0,0}},
    {{3,0,0}},
    {{4,0,0}},
    {{5,0,0}},
    {{8,9,0}},
    {{6,7,0}},
    {{6,10,7}},
    {{8,11,9}},
    {{12,13,0}},
    {{15,0,0}},
    {{16,0,0}},
    {{17,18,19}},
    {{20,21,22}},
    {{23,24,0}},
    {{25,0,0}},
    {{26,0,0}},
    {{27,0,0}},
    {{28,0,0}},
    {{29,30,0}},
    {{31,0,0}},
    {{32,0,0}},
    {{37,38,30}},
    {{0,0,0}},
    {{33,0,0}},
    {{34,0,0}},
}};
constexpr std::array<WeaponDefinition, kWeaponCount> kWeapons{{
    {0x0000, 0x0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 30, 0, 0x00000000, 0x00000000},
    {0x0fa5, 0x0fa4, 6, 2, 0, 41, -65, -148, 0, 0, 0, 0, 70, 0, 0x00000042, 0x00000000},
    {0x0000, 0x0000, 0, 3, 0, 118, -78, -86, 0, 0, 0, 0, 30, 0, 0x00000000, 0x00000000},
    {0x0000, 0x0000, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 30, 0, 0x00000000, 0x00000000},
    {0x0fc0, 0x0fbf, 1, 5, 17, 141, -57, -54, 0, 0, 0, 0, 10, 0, 0x00000002, 0x00000000},
    {0x0fa7, 0x0fa6, 1, 6, 11, 102, -34, -43, 0, 0, 0, 0, 10, 0, 0x00000000, 0x00000000},
    {0x0fd9, 0x0fd8, 1, 7, 7, 72, -44, -75, 0, 0, 0, 0, 6, 4, 0x00000005, 0x00000000},
    {0x0fd9, 0x0fd8, 1, 7, 7, 72, -44, -75, 0, 0, 0, 0, 10, 4, 0x00000005, 0x00000000},
    {0x0fa3, 0x0fa2, 1, 7, 7, 52, -45, -75, 0, 0, 0, 0, 6, 0, 0x00000004, 0x00000000},
    {0x0fa3, 0x0fa2, 1, 7, 7, 52, -45, -75, 0, 0, 0, 0, 6, 0, 0x00000004, 0x00000000},
    {0x0fd9, 0x0fd8, 2, 9, 13, 72, -44, -75, 0, 0, 0, 1, 150, 4, 0x00000015, 0x00000000},
    {0x0fa3, 0x0fa2, 1, 10, 19, 52, -45, -75, 0, 0, 0, 0, 30, 0, 0x00000004, 0x00000000},
    {0x0fb8, 0x0fb7, 1, 11, 14, 85, -43, -73, 0, 0, 0, 0, 6, 0, 0x00000024, 0x00000000},
    {0x0fb8, 0x0fb7, 1, 11, 14, 85, -43, -73, 0, 0, 0, 0, 6, 0, 0x00000004, 0x00000000},
    {0x0fb8, 0x0fb7, 1, 11, 14, 85, -43, -73, 0, 0, 0, 0, 5, 0, 0x00000024, 0x00000000},
    {0x0fbc, 0x0fbb, 1, 12, 15, 45, -20, -42, 0, 0, 0, 0, 55, 8, 0x00000005, 0x00000000},
    {0x0fa3, 0x0fa2, 1, 13, 16, 87, -67, -77, 0, 0, 0, 0, 60, 8, 0x00000007, 0x00000000},
    {0x0fa3, 0x0fa2, 1, 14, 4, 94, -33, -54, 0, 0, 0, 0, 6, 0, 0x00000000, 0x00000000},
    {0x0fa3, 0x0fa2, 1, 14, 4, 94, -33, -54, 0, 0, 0, 0, 6, 0, 0x00000000, 0x00000000},
    {0x0fa3, 0x0fa2, 1, 14, 4, 94, -33, -54, 0, 0, 0, 0, 10, 0, 0x00000000, 0x00000000},
    {0x0fbe, 0x0fbd, 1, 15, 4, 84, -35, -72, 0, 0, 0, 0, 6, 0, 0x00000026, 0x00000000},
    {0x0fbe, 0x0fbd, 1, 15, 4, 84, -35, -72, 0, 0, 0, 0, 6, 0, 0x00000006, 0x00000000},
    {0x0fbe, 0x0fbd, 1, 15, 4, 84, -35, -72, 0, 0, 0, 0, 10, 0, 0x00000006, 0x00000000},
    {0x0fa3, 0x0fa2, 1, 16, 3, 87, -67, -77, 0, 0, 0, 0, 4, 0, 0x00000000, 0x00000000},
    {0x0fa3, 0x0fa2, 1, 16, 3, 87, -67, -77, 0, 0, 0, 0, 35, 0, 0x00000000, 0x00000000},
    {0x0fa3, 0x0fa2, 0, 17, 20, 87, -67, -77, 0, 0, 0, 0, 5, 2, 0x0000000d, 0x00000000},
    {0x0fa3, 0x0fa2, 0, 18, 18, 87, -67, -77, 0, 0, 0, 0, 5, 0, 0x00000000, 0x00000000},
    {0x0fe9, 0x0fea, 10, 19, 0, 66, -45, -33, 0, 0, 0, 0, 180, 2, 0x00000001, 0x00000000},
    {0x0000, 0x0fc9, 2, 20, 27, 30, -67, 0, 0, 0, 0, 0, 130, 2, 0x0000000d, 0x00000000},
    {0x0fb4, 0x0000, 9, 21, 2, 87, -67, -77, 0, 0, 0, 0, 90, 0, 0x00000080, 0x00000000},
    {0x0fe7, 0x0fe8, 8, 21, 0, 73, -88, -140, 0, 0, 0, 0, 5, 0, 0x00000000, 0x00000000},
    {0x0fa3, 0x0fa2, 0, 22, 0, 87, -67, -77, 0, 0, 0, 0, 5, 0, 0x00000000, 0x00000000},
    {0x0fba, 0x0fb9, 1, 23, 22, 60, -35, -45, 0, 0, 0, 0, 70, 8, 0x00000005, 0x00000000},
    {0x0fc5, 0x0fc1, 11, 26, 12, 36, -57, -107, 0, 0, 0, 0, 90, 0, 0x00000082, 0x00000000},
    {0x0fa3, 0x0fa2, 0, 27, 1, 87, -67, -77, 0, 0, 0, 0, 30, 0, 0x00000082, 0x00000000},
    {0x0fa3, 0x0fb6, 1, 7, 23, 52, -45, -75, 0, 0, 0, 0, 5, 4, 0x00000004, 0x00000000},
    {0x0fa3, 0x0fb6, 1, 7, 25, 52, -45, -75, 0, 0, 0, 0, 5, 4, 0x00000004, 0x00000000},
    {0x0fc7, 0x0000, 7, 24, 10, 66, -68, -94, 0, 0, 0, 0, 90, 4, 0x00000080, 0x00000000},
    {0x0fc7, 0x0000, 7, 24, 10, 66, -68, -94, 0, 0, 0, 0, 90, 4, 0x00000080, 0x00000000},
}};
constexpr std::array<WeaponClassDefinition, kWeaponClassCount> kClasses{{
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    {1, 5, 0, 20, 0, 0, 100, 1, 0, 0, 0, 0, 0},
    {2, 2, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    {3, 12, 30, 295, 0, 10, 100, 5, 0, 0, 0, 0, 2},
    {4, 12, 30, 295, 1, 5, 100, 5, 98, 97, 96, 95, 130},
    {5, 12, 30, 295, 1, 5, 100, 5, 94, 0, 0, 0, 130},
    {6, 12, 30, 295, 1, 5, 100, 5, 0, 0, 0, 0, 130},
    {7, 12, 30, 1000, 1, 30, 70, 1, 82, 84, 83, 85, 2},
    {8, 12, 30, 1000, 1, 30, 70, 1, 81, 84, 83, 85, 2},
    {9, 12, 30, 1000, 1, 30, 70, 3, 0, 0, 0, 0, 2},
    {10, 2, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    {11, 3, 7, 295, 1, 10, 200, 8, 86, 89, 88, 87, 2},
    {12, 6, 1, 20, 0, 0, 1000, 6, 0, 0, 0, 0, 16},
    {13, 18, 1, 72, 0, 0, 1000, 1, 80, 113, 0, 114, 5},
    {14, 3, 200, 1000, 1, 30, 70, 4, 99, 102, 101, 100, 2},
    {15, 4, 10, 1200, 4, 50, 60, 1, 90, 93, 92, 91, 2},
    {16, 4, 5, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    {17, 3, 9, 250, 0, 10, 200, 4, 109, 112, 111, 110, 130},
    {18, 5, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    {19, 3, 10, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    {20, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    {21, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    {22, 12, 5, 950, 2, 40, 70, 1, 103, 106, 105, 104, 2},
    {23, 12, 30, 1000, 1, 30, 70, 1, 107, 0, 0, 0, 258},
    {24, 12, 30, 1000, 1, 30, 70, 3, 0, 0, 0, 0, 258},
    {25, 12, 30, 1000, 1, 30, 70, 1, 108, 0, 0, 0, 258},
    {26, 12, 30, 1000, 1, 30, 70, 3, 0, 0, 0, 0, 258},
    {27, 2, 1, 72, 3, 1000, 500, 1, 115, 117, 0, 116, 66},

}};
} // namespace

const std::array<WeaponDefinition, kWeaponCount>& weapon_definitions() noexcept { return kWeapons; }
const std::array<WeaponClassDefinition, kWeaponClassCount>& weapon_classes() noexcept { return kClasses; }

std::array<std::uint8_t,kWeaponSlotCount> weapon_loadout_slots(const std::array<std::uint32_t,3>& ids) {
    for(auto id:ids)if(id>=kLoadouts.size())throw std::out_of_range("weapon loadout id exceeds the recipe table");
    const auto& sidearm=kLoadouts[ids[0]];
    const auto& primary=kLoadouts[ids[1]];
    const auto& secondary=kLoadouts[ids[2]];
    return {27,1,sidearm[0],primary[0],primary[1],primary[2],secondary[0],secondary[1],secondary[2],33};
}
void WeaponState::loadout(const std::array<std::uint32_t,3>& ids) {
    set_slot_map(weapon_loadout_slots(ids));
    resupply(ids[2]==1); // Original 484914 compares C23A4C with 1.
}

WeaponState::WeaponState() {
    slot_map_ = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

void WeaponState::set_slot_map(const std::array<std::uint8_t, kWeaponSlotCount>& map) noexcept {
    slot_map_ = map;
}

namespace {
std::int32_t gun_flags_refire(std::uint32_t flags) noexcept { return (flags & 0x20u) ? 1 : 0; }
std::int32_t signed_word(std::uint32_t word) noexcept {std::int32_t result;std::memcpy(&result,&word,4);return result;}
std::int32_t decrement(std::int32_t value) noexcept {return signed_word(std::uint32_t(value)-1);}
}

const WeaponClassDefinition& WeaponState::class_of(const WeaponDefinition& definition) const noexcept {
    return kClasses[definition.weapon_class];
}

std::uint32_t& WeaponState::burst_cell() {return burst_storage?burst_storage():burst_counter_;}
const std::uint32_t& WeaponState::burst_cell() const {return burst_storage?burst_storage():burst_counter_;}
std::int32_t WeaponState::burst_counter() const {return signed_word(burst_cell());}
std::uint32_t WeaponState::aim_block_raw() const noexcept {return std::uint32_t(aim_block_);}
void WeaponState::set_aim_block_raw(std::uint32_t value) noexcept {aim_block_=signed_word(value);}

void WeaponState::run_view_body(WeaponViewKind kind,std::uint32_t weapon){
    const auto body=view_body;
    if(!body)throw std::runtime_error("weapon: live view body missing");
    body(kind,weapon);
}

void WeaponState::emit_shot() {
    const auto index=current_weapon();
    if(shot_mode==WeaponShotMode::live_body)
        throw std::runtime_error("weapon: live shot requires its view body");
    if(index==6){
        if(burst_cell()==0)burst_cell()=2;else --burst_cell();
    }
    emit({WeaponEventKind::shot,std::int32_t(index)});
}

void WeaponState::finish_action(int silent) {
    // 4831B0, 483580 and 4837D0 share one body: refuse while the action
    // timer runs, set C23830 = weapon +0x9C, play the class action sound
    // only when the argument is zero.
    if (fire_timer_ > 0) return;
    fire_timer_ = current_definition().fire_delay;
    if (silent == 0)
        emit({WeaponEventKind::sound, std::int32_t(class_of(current_definition()).action_sound)});
}

void WeaponState::start_reload() { // 4832F0
    if (reload_timer_ > 0) return;
    const auto& definition = current_definition();
    const auto weapon_class = definition.weapon_class;
    if (reserve_[weapon_class] == 0) return;
    if (magazine_[weapon_class] == std::int32_t(kClasses[weapon_class].magazine_size)) return;
    emit({WeaponEventKind::sound, std::int32_t(kClasses[weapon_class].reload_sound)});
    magazine_[weapon_class] = 0;
    reload_timer_ = kReloadTicks;
}

void WeaponState::complete_reload() { // 483390
    const auto weapon_class = current_definition().weapon_class;
    if (reserve_[weapon_class] == 0) return;
    reserve_[weapon_class] -= 1;
    magazine_[weapon_class] = std::int32_t(kClasses[weapon_class].magazine_size);
    if (pending_[weapon_class] == 0) finish_action(0);
}

void WeaponState::expire_gun() { // 483240
    const auto& definition = current_definition();
    const auto weapon_class = definition.weapon_class;
    if (magazine_[weapon_class] == 0) { start_reload(); return; }
    pending_[weapon_class] = 1;
    if (!unlimited_ammo_) {
        magazine_[weapon_class] -= 1;
        if (magazine_[weapon_class] == 0) { start_reload(); return; }
    }
    if (gun_flags_refire(definition.flags) != 0 &&
        (trigger_query?trigger_query():trigger_held_) && aim_block_ == 0) fan_dispatch();
}

void WeaponState::expire_launcher() { // 483610
    const auto weapon_class = current_definition().weapon_class;
    if (reserve_[weapon_class] == 0) return;
    pending_[weapon_class] = 1;
    if (!unlimited_ammo_) reserve_[weapon_class] -= 1;
}

void WeaponState::expire_charge() { // 483860 (use classes 7, 9, 11)
    expire_launcher();
}

void WeaponState::expire_laser() { // 483AA0
    const auto weapon_class=current_definition().weapon_class;
    emit({WeaponEventKind::queue, 0x2C});
    pending_[weapon_class] = 1;
}

void WeaponState::expire_action() { // 483B70
    switch (current_definition().use_class) {
        case 1: expire_gun(); break;
        case 2: expire_launcher(); break;
        case 6: pending_[current_definition().weapon_class] = 1; break; // 4839F0
        case 7: case 9: case 11: expire_charge(); break;
        case 10: expire_laser(); break;
        default: break; // use class 8 (detonator) and gaps: nothing
    }
}

void WeaponState::expire_reload_timer() { // 483C30
    switch (current_definition().use_class) {
        case 1: complete_reload(); break;
        case 2: expire_action(); break;
        default: break;
    }
}

void WeaponState::fire_gun() { // 482F90
    if (fire_timer_ > 0) return;
    const auto& definition = current_definition();
    const auto weapon_class = definition.weapon_class;
    if (pending_[weapon_class] == 0) {
        emit({WeaponEventKind::sound, std::int32_t(kClasses[weapon_class].empty_sound)});
        finish_action(1);
        burst_cell() = 0;
        return;
    }
    if(shot_mode==WeaponShotMode::live_body){
        run_view_body(WeaponViewKind::gun,current_weapon());
    } else emit_shot();
    pending_[weapon_class] = 0;
    finish_action(1);
}

void WeaponState::fire_launcher() { // 483410
    if (fire_timer_ > 0) return;
    const auto& definition = current_definition();
    const auto weapon_class = definition.weapon_class;
    if (pending_[weapon_class] == 0) {
        emit({WeaponEventKind::sound, std::int32_t(kClasses[weapon_class].empty_sound)});
        burst_cell() = 0;
        return; // 4834E9 leaves the action timer untouched
    }
    if(shot_mode==WeaponShotMode::live_body)run_view_body(WeaponViewKind::launcher,current_weapon());else emit_shot();
    pending_[weapon_class] = 0;
    finish_action(0);
}

void WeaponState::fire_melee() { // 4838C0
    if (fire_timer_ > 0) return;
    const auto index=current_weapon();
    const auto& definition = kWeapons[index];
    if(shot_mode==WeaponShotMode::live_body)run_view_body(WeaponViewKind::melee,index);else emit_shot();
    pending_[definition.weapon_class] = 0;
    fire_timer_ = definition.fire_delay; // inline, silent
}

void WeaponState::fire_claymore() { // 4840B0
    if (fire_timer_ > 0) return;
    const auto& definition = current_definition();
    const auto weapon_class = definition.weapon_class;
    if (pending_[weapon_class] == 0) return;
    const auto index = current_weapon();
    if(shot_mode==WeaponShotMode::live_body)run_view_body(WeaponViewKind::claymore,index);
    else emit({WeaponEventKind::place, index == 0x25 ? 0xFAE : 0xFC8});
    pending_[weapon_class] = 0;
    finish_action(0);
}

void WeaponState::fire_detonator() { // 470F40
    emit({WeaponEventKind::queue, 0x28});
}

void WeaponState::fire_satchel() { // 4841F0
    if (fire_timer_ > 0) return;
    const auto& definition = current_definition();
    const auto weapon_class = definition.weapon_class;
    if (pending_[weapon_class] == 0) return;
    if(shot_mode==WeaponShotMode::live_body)run_view_body(WeaponViewKind::satchel,current_weapon());
    else emit({WeaponEventKind::place, 0xFB4});
    pending_[weapon_class] = 0;
    finish_action(0);
}

void WeaponState::fire_laser() { // 483A30
    if (fire_timer_ > 0) return;
    const auto index=current_weapon();
    emit({WeaponEventKind::sound2, 0x1AE});
    emit({WeaponEventKind::message, 0});
    fire_timer_ = kWeapons[index].fire_delay;
}

void WeaponState::fire_grenade() { // 483670
    if (fire_timer_ > 0) return;
    const auto& definition = current_definition();
    const auto weapon_class = definition.weapon_class;
    if (pending_[weapon_class] == 0) {
        emit({WeaponEventKind::sound, std::int32_t(kClasses[weapon_class].empty_sound)});
        burst_cell() = 0;
        return;
    }
    if(shot_mode==WeaponShotMode::live_body)run_view_body(WeaponViewKind::grenade,current_weapon());else emit_shot();
    pending_[weapon_class] = 0;
    finish_action(0);
}

void WeaponState::fan_dispatch() { // 483B10
    switch (current_definition().use_class) {
        case 1: fire_gun(); break;
        case 2: fire_launcher(); break;
        case 6: fire_melee(); break;
        case 7: fire_claymore(); break;
        case 8: fire_detonator(); break;
        case 9: fire_satchel(); break;
        case 10: fire_laser(); break;
        case 11: fire_grenade(); break;
        default: break;
    }
}

bool WeaponState::fire() { // 483C70
    if (aim_block_ != 0) return false;
    const auto index = current_weapon();
    if (index == 0) return false;
    const auto& definition = current_definition();
    if (definition.weapon_class == 0) {
        const auto use_class = definition.use_class;
        if (use_class != 6 && use_class != 8 && use_class != 10) return false;
    }
    if (fire_timer_ > 0) return false;
    fan_dispatch();
    return true;
}

bool WeaponState::reload_press() { // 483BD0
    switch (current_definition().use_class) {
        case 1: start_reload(); return true;
        case 2: case 7: case 9: case 11: finish_action(0); return true;
        default: return false;
    }
}

bool WeaponState::select_slot(int slot) { // 483DD0
    if (slot < 0 || slot >= kWeaponSlotCount) return false;
    const auto& definition = kWeapons[slot_map_[std::size_t(slot)]];
    const auto weapon_class = definition.weapon_class;
    slot_ = slot;
    fire_timer_ = 0;
    reload_timer_ = 0;
    switch (definition.use_class) {
        case 1:
            if (magazine_[weapon_class] == 0 && reserve_[weapon_class] != 0) start_reload();
            break;
        case 2: case 7: case 9: case 11:
            if (pending_[weapon_class] == 0 && reserve_[weapon_class] != 0) finish_action(0);
            break;
        default: break;
    }
    burst_cell() = 0;
    return true;
}

bool WeaponState::cycle_slot(int direction) { // 484B70
    if (direction > 0) {
        for (int slot = slot_ + 1; slot < kWeaponSlotCount; ++slot)
            if (slot_map_[std::size_t(slot)] != 0) return select_slot(slot);
        for (int slot = 0; slot <= slot_; ++slot) // original wrap scan includes the current slot
            if (slot_map_[std::size_t(slot)] != 0) return select_slot(slot);
        return false;
    }
    for (int slot = slot_ - 1; slot >= 0; --slot)
        if (slot_map_[std::size_t(slot)] != 0) return select_slot(slot);
    for (int slot = kWeaponSlotCount - 1; slot >= slot_; --slot) // includes the current slot
        if (slot_map_[std::size_t(slot)] != 0) return select_slot(slot);
    return false;
}

void WeaponState::toggle_aim_block() noexcept { // 484C20 toggles C23B64
    // Blocker primitive. toggle_player_aim owns the full RNG and sway body.
    aim_block_ = aim_block_ != 0 ? 0 : 1;
}

void WeaponState::tick(bool trigger_held) {
    // 482B00 weapon section. Actor eye height (C23A18) and the C23B7C
    // target cycling in the same function belong to camera/mission state.
    trigger_held_ = trigger_held;
    if (raise_counter_ > 1) raise_counter_ -= 2;
    else raise_counter_ = 0;
    if (fire_timer_ != 0) {
        fire_timer_ = decrement(fire_timer_);
        if (fire_timer_ <= 0) {
            fire_timer_ = 0;
            expire_action();
        }
    }
    if (reload_timer_ != 0) {
        reload_timer_ = decrement(reload_timer_);
        if (reload_timer_ <= 0) {
            reload_timer_ = 0;
            expire_reload_timer();
        }
    }
    if (burst_cell() != 0 && fire_timer_ <= 0) fire();
}

void WeaponState::resupply(bool double_primary_slots) { // 4848C0
    for (int slot = 0; slot < kWeaponSlotCount; ++slot) {
        const auto weapon_class = kWeapons[slot_map_[std::size_t(slot)]].weapon_class;
        reserve_[weapon_class] = std::int32_t(kClasses[weapon_class].magazine_count) - 1;
        magazine_[weapon_class] = std::int32_t(kClasses[weapon_class].magazine_size);
        pending_[weapon_class] = 1;
    }
    if (double_primary_slots) // C23A4C == 1 covers slots 3, 4, 5
        for (int slot = 3; slot <= 5; ++slot) {
            const auto weapon_class = kWeapons[slot_map_[std::size_t(slot)]].weapon_class;
            reserve_[weapon_class] = std::int32_t(kClasses[weapon_class].magazine_count) * 2 - 1;
        }
}

void WeaponState::emit(WeaponEvent event) {
    events_.push_back(event);
    const auto sink=on_event;
    if(sink)sink(event);
}

std::vector<WeaponEvent> WeaponState::take_events() {
    auto events = std::move(events_);
    events_.clear();
    return events;
}

} // namespace dfre
