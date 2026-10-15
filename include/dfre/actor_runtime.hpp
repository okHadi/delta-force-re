#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dfre {

// Runtime actor factory and initialization stage, rebuilt from Df.exe
// (sha256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff).
//   actor factory       0x45D520  (definition -> 0x114-byte runtime actor)
//   instance bind       0x43F610  (marker pose copy into the actor)
//   placement finalize  0x440640  (17 mission placement slots -> actors)
//   definition init     0x4829F0  (actor side: local player actor binding)
//
// The stage is data-only. Person slots (0x44A0C0) and resource handles
// (0x42CCE0, item loader 0x45D6F0) are caller inputs; this module never
// allocates and never resolves models. Movement, animation, AI, weapons and
// resource planning stay in their own modules.

inline constexpr std::size_t kActorImageBytes = 0x114;
inline constexpr std::size_t kActorSlotCount = 0x100;    // table 0x7CCA80
inline constexpr std::size_t kMarkerRecordBytes = 0x114; // table 0x8ACE80
inline constexpr std::size_t kPlacementStride = 0x9B84;  // table 0x6E9A90
inline constexpr std::size_t kPlacementCount = 0x11;     // 17 slots

using ActorImage = std::array<std::uint8_t, kActorImageBytes>;
using MarkerImage = std::array<std::uint8_t, kMarkerRecordBytes>;

// Original class item ids selected by 0x45D520. `mode` is 0x8CF97C, `night`
// is bit 0x10 of 0x8CF982, `side` is the factory's second argument.
std::uint32_t actor_class_item_id(std::uint32_t mode, bool night, std::uint32_t side) noexcept;

// Loaded item-definition fields the stage reads. The original record is
// 0x13C bytes at [0xC23810] + index*0x13C. `graphic`, `ai_function` and
// `move_function` are the loader handles (0x45D6F0) and are copied straight
// into the actor. `character` is the raw +0x74 record field; the original
// resolves it through 0x42CCE0 and the result is ActorCreateHandles::character.
struct ActorDefinition {
    std::uint32_t item_id = 0;        // +0x20
    std::uint32_t graphic = 0;        // +0x6C graphic model handle
    std::uint32_t character = 0;      // +0x74 character record field (raw)
    std::uint32_t ai_function = 0;    // +0x84
    std::uint32_t move_function = 0;  // +0x90
    std::uint32_t hp = 0;             // +0x94 dword; the actor keeps u16
};

// 0x4828C0: first record whose +0x20 equals the id; index 0 when absent.
class ActorDefinitionTable {
public:
    ActorDefinitionTable() = default;
    explicit ActorDefinitionTable(std::vector<ActorDefinition> entries);
    std::size_t find_index(std::uint32_t item_id) const noexcept;
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t size() const noexcept { return entries_.size(); }
    const ActorDefinition& at(std::size_t index) const;
    const std::vector<ActorDefinition>& entries() const noexcept { return entries_; }
private:
    std::vector<ActorDefinition> entries_;
};

struct ActorFactoryConfig {
    std::uint32_t mode = 0;     // 0x8CF97C
    bool night = false;         // (0x8CF982 & 0x10) != 0
    bool scale_health = false;  // 0x67F549 bit 0: hp << 2, wrapped to u16
};

// Handles the original obtains from allocators. The caller supplies both.
struct ActorCreateHandles {
    std::uint32_t person = 0;     // 0x44A0C0 result stored at actor +0x0C
    std::uint32_t character = 0;  // 0x42CCE0(def +0x74) result at actor +0x18
};

// Fields 0x45D520 writes into the zeroed 0x114-byte slot. Every other byte
// stays zero. The serialized image keeps the definition index at +0x04 where
// the native image stores the definition pointer.
struct RuntimeActor {
    std::uint32_t definition_index = 0;  // +0x00
    std::uint32_t object_id = 0;         // +0x08 placement +0x00 (finalize)
    std::uint32_t person = 0;            // +0x0C
    std::uint32_t graphic = 0;           // +0x10 definition +0x6C
    std::uint32_t husk = 0;              // +0x14; factory writes 0
    std::uint32_t character = 0;         // +0x18 create handle
    std::array<std::uint32_t, 6> pose{}; // +0x24..+0x3B bind copy
    std::uint32_t flags = 0;             // +0x6C = 3
    std::uint16_t field_70 = 0;          // +0x70 placement +0x9C (finalize)
    std::uint16_t field_72 = 0;          // +0x72 = 1
    std::uint16_t health = 0;            // +0x74 definition hp, optional << 2
    std::uint32_t weapon_slot = 0;       // +0x94 0x4829F0 weapon slot map value
    std::uint32_t argument = 0;          // +0x98 factory argument 1
    std::uint32_t move_function = 0;     // +0x10C definition +0x90
    std::uint32_t ai_function = 0;       // +0x110 definition +0x84
};

// Exact 0x114-byte image of a RuntimeActor. Normalized fields: +0x04 keeps
// the definition index, so a native pointer is reconstructed as
// `definition_base + index*0x13C` by the caller.
ActorImage actor_image(const RuntimeActor& actor);

// Port of 0x45D520. Slots fill front to back; the high-water count is
// 0x8CFA18. Returns nullopt when all 256 slots are used.
class RuntimeActorFactory {
public:
    RuntimeActorFactory(ActorDefinitionTable definitions, ActorFactoryConfig config);
    std::optional<std::size_t> create(std::uint32_t argument1, std::uint32_t argument2,
                                      const ActorCreateHandles& handles);
    std::size_t count() const noexcept { return count_; }
    const std::vector<RuntimeActor>& actors() const noexcept { return actors_; }
    std::vector<RuntimeActor>& actors() noexcept { return actors_; }
    const RuntimeActor& actor(std::size_t index) const;
    RuntimeActor& actor(std::size_t index);
    const ActorDefinitionTable& definitions() const noexcept { return definitions_; }
    const ActorFactoryConfig& config() const noexcept { return config_; }
private:
    ActorDefinitionTable definitions_;
    ActorFactoryConfig config_;
    std::vector<RuntimeActor> actors_;
    std::size_t count_ = 0;
};

// One record of the original 0x8ACE80 marker table (0x114 bytes). The bind
// reads +0x00 (definition index) and +0x24..+0x3B (six pose words).
class MarkerPoseTable {
public:
    MarkerPoseTable() = default;
    explicit MarkerPoseTable(std::vector<MarkerImage> records);
    std::size_t size() const noexcept { return records_.size(); }
    std::uint32_t definition_index(std::size_t index) const;
    std::array<std::uint32_t, 6> pose(std::size_t index) const;
    const MarkerImage& image(std::size_t index) const;
private:
    std::vector<MarkerImage> records_;
};

// Counts and mode the bind reads. The original stores them in globals
// (0x8CF9F4 6001, 0x8CFA00 6002, 0x8CF9F8 6003, 0x8CF9FC 6004); their writer
// is not proven, so the caller supplies them. The rotating counter is
// 0x4F820C.
struct ActorBindConfig {
    std::uint32_t game_type = 0;   // 0x67F53C, 0..5
    std::uint32_t count_6001 = 0;  // 0x8CF9F4
    std::uint32_t count_6002 = 0;  // 0x8CFA00
    std::uint32_t count_6003 = 0;  // 0x8CF9F8
    std::uint32_t count_6004 = 0;  // 0x8CF9FC
};

// Port of 0x43F610. Selects a marker record for the actor and copies its six
// pose words into actor +0x24..+0x3B. `counter` is the original 0x4F820C.
void bind_actor_pose(std::uint32_t& counter, const ActorBindConfig& config,
                     const MarkerPoseTable& markers, const ActorDefinitionTable& definitions,
                     std::size_t placement_index, RuntimeActor& actor);

// One of the 17 original placement slots (table 0x6E9A90). Slots 1..16 are
// filled by the network/lobby code, slot 0 by 0x444C50; both stay caller data.
struct ActorPlacement {
    std::uint32_t object_id = 0;     // +0x00
    std::string name;                // +0x0C C string copied to person +4
    std::uint16_t field_9c = 0;      // +0x9C -> actor +0x70
    std::uint32_t argument1 = 0;     // +0xA0 factory argument 1
    std::uint32_t argument2 = 0;     // +0xA4 factory argument 2
    std::uint32_t team_code = 0;     // +0x1C4 u16; not read by this stage
    bool active = false;             // +0x18C
    std::optional<std::size_t> actor; // +0x08 factory slot (output)
};

// Result of one 0x440640 creation. `person` and `name` belong to the person
// record the caller allocated (0x44A0C0); the original writes the name to
// person +4.
struct ActorSpawn {
    std::size_t placement = 0;
    std::size_t actor = 0;
    std::uint32_t person = 0;
    std::string name;
};

// Port of 0x440640. `enabled` mirrors 0x67F528 != 0. `handles` holds one
// 0x44A0C0/0x42CCE0 pair per placement (used when the slot is active).
// Updates each active placement's `actor` and returns the creation order.
std::vector<ActorSpawn> spawn_placement_actors(RuntimeActorFactory& factory,
                                               std::vector<ActorPlacement>& placements,
                                               const std::vector<ActorCreateHandles>& handles,
                                               std::uint32_t& bind_counter,
                                               const ActorBindConfig& bind_config,
                                               const MarkerPoseTable& markers,
                                               bool enabled);

// Actor-side result of 0x4829F0. The original also writes the 0xC23820
// runtime record (camera height 0xB333, weapon slot map); that record belongs
// to the player/camera modules and stays out of this stage.
struct LocalPlayerActor {
    std::size_t actor = 0;        // original [0xC23820] and [0xC23A38]
    std::uint32_t field_94 = 0;   // original weapon slot map value -> actor +0x94
    std::uint32_t field_98 = 0;   // original [0x67F430] -> actor +0x98
    std::string person_name;      // original [0x67F420] -> person +4
};

// Port of the actor scan and writes of 0x4829F0. Finds the first actor with
// +0x6C bit 0 set and +0x08 equal to `player_object_id` (original 0x50133C).
// Returns nullopt when no actor matches.
std::optional<LocalPlayerActor> bind_local_player_actor(RuntimeActorFactory& factory,
                                                        std::uint32_t player_object_id,
                                                        std::uint32_t weapon_slot_value,
                                                        std::uint32_t field_98,
                                                        std::string_view player_name);

} // namespace dfre
