#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>
namespace dfre {
// Original weapon runtime state for Delta Force (1998).
// Addresses refer to Df.exe SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// The tables below are static copies of the original data at 0x4F9934
// (weapons, 39 x 0xA8) and 0x4F9154 (classes, 28 x 0x34).

struct WeaponDefinition {
    std::uint16_t primary_item = 0;   // +0x64 runtime item id
    std::uint16_t secondary_item = 0; // +0x68 runtime item id
    std::uint8_t use_class = 0;       // +0x6C action class 1..11, 0 none
    std::uint8_t ui_index = 0;        // +0x70
    std::uint8_t weapon_class = 0;    // +0x74 ammo/behaviour class 0..27
    std::int32_t muzzle_x = 0;        // +0x78
    std::int32_t muzzle_y = 0;        // +0x7C
    std::int32_t muzzle_z = 0;        // +0x80
    std::int32_t aim_x = 0;           // +0x84
    std::int32_t aim_y = 0;           // +0x88
    std::int32_t aim_z = 0;           // +0x8C
    std::int32_t aim_field = 0;       // +0x90
    std::int32_t fire_delay = 0;      // +0x9C ticks between shots
    std::int32_t burst_field = 0;     // +0xA0
    std::uint32_t flags = 0;          // +0xA4, bit 0x20 = refire while held
};

struct WeaponClassDefinition {       // 0x4F9154 record
    std::uint32_t id = 0;            // +0x00
    std::uint32_t magazine_count = 0;// +0x04 spare magazines
    std::uint32_t magazine_size = 0; // +0x08 rounds per magazine
    std::uint32_t field_0c = 0;      // +0x0C
    std::uint32_t field_10 = 0;      // +0x10
    std::uint32_t field_14 = 0;      // +0x14
    std::uint32_t field_18 = 0;      // +0x18
    std::uint32_t field_1c = 0;      // +0x1C
    std::uint32_t field_20 = 0;      // +0x20
    std::uint32_t action_sound = 0;  // +0x24 used by 4831B0/483580/4837D0
    std::uint32_t reload_sound = 0;  // +0x28 used by 4832F0
    std::uint32_t empty_sound = 0;   // +0x2C used by dry fire paths
    std::uint32_t field_30 = 0;      // +0x30
};

constexpr std::size_t kWeaponCount = 39;
constexpr std::size_t kWeaponClassCount = 28;
constexpr int kWeaponSlotCount = 10;
constexpr std::size_t kWeaponLoadoutCount = 28;
// Original 484970: sidearm, primary, secondary recipe ids.
std::array<std::uint8_t,kWeaponSlotCount> weapon_loadout_slots(
    const std::array<std::uint32_t,3>& ids);
constexpr std::int32_t kReloadTicks = 120; // 4832F0 sets 0x78

const std::array<WeaponDefinition, kWeaponCount>& weapon_definitions() noexcept;
const std::array<WeaponClassDefinition, kWeaponClassCount>& weapon_classes() noexcept;

enum class WeaponEventKind : std::uint8_t {
    shot,      // 45EA00 projectile burst entry, value = weapon index
    sound,     // 43E7D0 positional sound, value = sound id
    sound2,    // 43E720 global sound/asset, value = id
    place,     // 470F60 entity placement, value = item id
    queue,     // 46F3E0 message queue, value = message type
    message,   // 479F00 HUD text (laser "Fire Missions")
};
struct WeaponEvent {
    WeaponEventKind kind = WeaponEventKind::sound;
    std::int32_t value = 0;
};
inline bool operator==(const WeaponEvent& left, const WeaponEvent& right) noexcept {
    return left.kind == right.kind && left.value == right.value;
}
inline bool operator!=(const WeaponEvent& left, const WeaponEvent& right) noexcept {
    return !(left == right);
}

// Deterministic local-player weapon stage: 482B00 (tick), 483C70 (fire),
// 483BD0 (change magazine), 483DD0 (slot select), 484B70 (cycle),
// 484C20 (aim toggle), 4848C0 (resupply). Ballistic calls stay inputs.
class WeaponState {
public:
    WeaponState();
    // C239C0 slot map, 10 entries, 0 = empty slot.
    void set_slot_map(const std::array<std::uint8_t, kWeaponSlotCount>& map) noexcept;
    const std::array<std::uint8_t, kWeaponSlotCount>& slot_map() const noexcept { return slot_map_; }
    // 67DBC9 bit 0: original "double ammo" toggle skips ammo cost.
    void set_unlimited_ammo(bool enabled) noexcept { unlimited_ammo_ = enabled; }
    bool unlimited_ammo() const noexcept { return unlimited_ammo_; }

    void loadout(const std::array<std::uint32_t,3>& ids); // 484970
    void resupply(bool double_primary_slots); // 4848C0
    bool select_slot(int slot);               // 483DD0
    bool cycle_slot(int direction);           // 484B70
    bool fire();                              // 483C70 -> 483B10
    bool reload_press();                      // 483BD0
    void toggle_aim_block() noexcept;         // 484C20 (blocker only)
    void tick(bool trigger_held);             // 482B00 weapon part

    int slot() const noexcept { return slot_; }                     // C239B8
    std::uint8_t current_weapon() const noexcept { return slot_map_[std::size_t(slot_)]; }
    const WeaponDefinition& current_definition() const noexcept { return weapon_definitions()[current_weapon()]; }
    std::int32_t fire_timer() const noexcept { return fire_timer_; }   // C23830
    std::int32_t reload_timer() const noexcept { return reload_timer_; } // C23834
    std::int32_t magazine(int weapon_class) const noexcept { return magazine_[std::size_t(weapon_class)]; } // C238B8
    std::int32_t reserve(int weapon_class) const noexcept { return reserve_[std::size_t(weapon_class)]; }   // C23938
    std::int32_t pending(int weapon_class) const noexcept { return pending_[std::size_t(weapon_class)]; }   // C23838
    bool aim_blocked() const noexcept { return aim_block_ != 0; }   // C23B64
    std::int32_t burst_counter() const noexcept { return burst_counter_; } // actor item def +0x6C
    std::int32_t raise_counter() const noexcept { return raise_counter_; } // C23A3C
    std::vector<WeaponEvent> take_events();

private:
    const WeaponClassDefinition& class_of(const WeaponDefinition&) const noexcept;
    void fan_dispatch();          // 483B10 by use class
    void expire_action();         // 483B70 by use class
    void expire_reload_timer();   // 483C30 by use class
    void finish_action(int silent);// 4831B0 / 483580 / 4837D0
    void fire_gun();              // 482F90
    void fire_launcher();         // 483410
    void fire_melee();            // 4838C0
    void fire_claymore();         // 4840B0
    void fire_detonator();        // 470F40
    void fire_satchel();          // 4841F0
    void fire_laser();            // 483A30
    void fire_grenade();          // 483670
    void expire_gun();            // 483240
    void expire_launcher();       // 483610
    void expire_charge();         // 483860
    void expire_laser();          // 483AA0
    void start_reload();          // 4832F0
    void complete_reload();       // 483390
    void emit_shot();             // 45EA00 counter part + event

    std::array<std::uint8_t, kWeaponSlotCount> slot_map_{};
    int slot_ = 0;
    std::int32_t fire_timer_ = 0;
    std::int32_t reload_timer_ = 0;
    std::array<std::int32_t, kWeaponClassCount> pending_{};
    std::array<std::int32_t, kWeaponClassCount> magazine_{};
    std::array<std::int32_t, kWeaponClassCount> reserve_{};
    std::int32_t aim_block_ = 0;
    std::int32_t burst_counter_ = 0;
    std::int32_t raise_counter_ = 0;
    bool unlimited_ammo_ = false;
    bool trigger_held_ = false;
    std::vector<WeaponEvent> events_;
};
} // namespace dfre
