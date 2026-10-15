#include "dfre/actor_runtime.hpp"

#include <cstring>
#include <stdexcept>

namespace dfre {
namespace {

constexpr std::uint32_t kClassIds[4][2] = {
    {0x138Bu, 0x138Cu},  // normal
    {0x1392u, 0x1393u},  // original mode 2
    {0x13B4u, 0x13B5u},  // night
    {0x13B6u, 0x13B7u},  // original mode 2 + night
};

constexpr std::uint32_t kMarker6001 = 0x1771u;
constexpr std::uint32_t kMarker6002 = 0x1772u;
constexpr std::uint32_t kMarker6003 = 0x1773u;
constexpr std::uint32_t kMarker6004 = 0x1774u;

std::uint32_t read_le32(const std::uint8_t* data) {
    std::uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

void write_le32(std::uint8_t* data, std::uint32_t value) {
    std::memcpy(data, &value, sizeof(value));
}

void write_le16(std::uint8_t* data, std::uint16_t value) {
    std::memcpy(data, &value, sizeof(value));
}

// Signed remainder like the original `idiv`: both operands are 32-bit.
std::uint32_t signed_remainder(std::uint32_t value, std::uint32_t modulus) {
    const std::int32_t dividend = static_cast<std::int32_t>(value);
    const std::int32_t divisor = static_cast<std::int32_t>(modulus);
    return static_cast<std::uint32_t>(dividend % divisor);
}

} // namespace

std::uint32_t actor_class_item_id(std::uint32_t mode, bool night, std::uint32_t side) noexcept {
    const unsigned row = (mode == 2 ? 1u : 0u) + (night ? 2u : 0u);
    return kClassIds[row][side != 0 ? 1 : 0];
}

ActorDefinitionTable::ActorDefinitionTable(std::vector<ActorDefinition> entries)
    : entries_(std::move(entries)) {}

std::size_t ActorDefinitionTable::find_index(std::uint32_t item_id) const noexcept {
    // 0x4828C0: first record whose +0x20 matches; index 0 when absent.
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].item_id == item_id) {
            return i;
        }
    }
    return 0;
}

const ActorDefinition& ActorDefinitionTable::at(std::size_t index) const {
    if (index >= entries_.size()) {
        throw std::out_of_range("actor definition index is out of range");
    }
    return entries_[index];
}

ActorImage actor_image(const RuntimeActor& actor) {
    ActorImage image{};
    write_le32(image.data() + 0x00, actor.definition_index);
    write_le32(image.data() + 0x04, actor.definition_index);  // normalized pointer
    write_le32(image.data() + 0x08, actor.object_id);
    write_le32(image.data() + 0x0C, actor.person);
    write_le32(image.data() + 0x10, actor.graphic);
    write_le32(image.data() + 0x14, actor.husk);
    write_le32(image.data() + 0x18, actor.character);
    for (std::size_t i = 0; i < actor.pose.size(); ++i) {
        write_le32(image.data() + 0x24 + i * 4, actor.pose[i]);
    }
    write_le32(image.data() + 0x6C, actor.flags);
    write_le16(image.data() + 0x70, actor.field_70);
    write_le16(image.data() + 0x72, actor.field_72);
    write_le16(image.data() + 0x74, actor.health);
    write_le32(image.data() + 0x94, actor.weapon_slot);
    write_le32(image.data() + 0x98, actor.argument);
    write_le32(image.data() + 0x10C, actor.move_function);
    write_le32(image.data() + 0x110, actor.ai_function);
    return image;
}

RuntimeActorFactory::RuntimeActorFactory(ActorDefinitionTable definitions, ActorFactoryConfig config)
    : definitions_(std::move(definitions)), config_(config), actors_(kActorSlotCount) {}

std::optional<std::size_t> RuntimeActorFactory::create(std::uint32_t argument1, std::uint32_t argument2,
                                                       const ActorCreateHandles& handles) {
    if (definitions_.empty()) {
        return std::nullopt;
    }
    const std::uint32_t class_id = actor_class_item_id(config_.mode, config_.night, argument2);
    const std::size_t definition_index = definitions_.find_index(class_id);
    const ActorDefinition& definition = definitions_.at(definition_index);
    // Original allocation: first slot whose +0x00 dword is zero.
    std::size_t slot = kActorSlotCount;
    for (std::size_t i = 0; i < kActorSlotCount; ++i) {
        if (actors_[i].definition_index == 0) {
            slot = i;
            break;
        }
    }
    if (slot >= kActorSlotCount) {
        return std::nullopt;
    }
    if (slot >= count_) {
        count_ = slot + 1;
    }
    RuntimeActor actor;
    actor.definition_index = definition_index;
    actor.person = handles.person;
    actor.graphic = definition.graphic;
    actor.character = handles.character;
    actor.flags = 3;
    actor.field_72 = 1;
    const std::uint16_t hp16 = static_cast<std::uint16_t>(definition.hp);
    actor.health = config_.scale_health
                       ? static_cast<std::uint16_t>(static_cast<std::uint32_t>(hp16) << 2)
                       : hp16;
    actor.argument = argument1;
    actor.move_function = definition.move_function;
    actor.ai_function = definition.ai_function;
    actors_[slot] = actor;
    return slot;
}

const RuntimeActor& RuntimeActorFactory::actor(std::size_t index) const {
    if (index >= actors_.size()) {
        throw std::out_of_range("actor slot is out of range");
    }
    return actors_[index];
}

RuntimeActor& RuntimeActorFactory::actor(std::size_t index) {
    if (index >= actors_.size()) {
        throw std::out_of_range("actor slot is out of range");
    }
    return actors_[index];
}

MarkerPoseTable::MarkerPoseTable(std::vector<MarkerImage> records) : records_(std::move(records)) {}

std::uint32_t MarkerPoseTable::definition_index(std::size_t index) const {
    if (index >= records_.size()) {
        throw std::out_of_range("marker record is out of range");
    }
    return read_le32(records_[index].data());
}

std::array<std::uint32_t, 6> MarkerPoseTable::pose(std::size_t index) const {
    if (index >= records_.size()) {
        throw std::out_of_range("marker record is out of range");
    }
    std::array<std::uint32_t, 6> pose{};
    for (std::size_t i = 0; i < pose.size(); ++i) {
        pose[i] = read_le32(records_[index].data() + 0x24 + i * 4);
    }
    return pose;
}

const MarkerImage& MarkerPoseTable::image(std::size_t index) const {
    if (index >= records_.size()) {
        throw std::out_of_range("marker record is out of range");
    }
    return records_[index];
}

void bind_actor_pose(std::uint32_t& counter, const ActorBindConfig& config,
                     const MarkerPoseTable& markers, const ActorDefinitionTable& definitions,
                     std::size_t placement_index, RuntimeActor& actor) {
    std::uint32_t scan_definition = 0;
    std::uint32_t scan_ordinal = 0;
    switch (config.game_type) {
    case 0:
        scan_definition = definitions.find_index(kMarker6001);
        scan_ordinal = config.count_6001 == 0 ? 0
                                              : signed_remainder(static_cast<std::uint32_t>(placement_index),
                                                                 config.count_6001);
        break;
    case 1:
    case 3:
        scan_definition = definitions.find_index(kMarker6002);
        scan_ordinal = config.count_6002 == 0 ? 0 : signed_remainder(counter, config.count_6002);
        ++counter;
        break;
    case 2:
    case 4:
    case 5:
        if (actor.field_70 == 1) {
            scan_definition = definitions.find_index(kMarker6003);
            scan_ordinal = config.count_6003 == 0 ? 0 : signed_remainder(counter, config.count_6003);
        } else {
            scan_definition = definitions.find_index(kMarker6004);
            scan_ordinal = config.count_6004 == 0 ? 0 : signed_remainder(counter, config.count_6004);
        }
        ++counter;
        break;
    default:
        break;  // original game types above 5 scan for definition index 0
    }
    std::uint32_t match = 0;
    for (std::size_t i = 0; i < markers.size(); ++i) {
        if (markers.definition_index(i) != scan_definition) {
            continue;
        }
        if (match == scan_ordinal) {
            actor.pose = markers.pose(i);
            return;
        }
        ++match;
    }
}

std::vector<ActorSpawn> spawn_placement_actors(RuntimeActorFactory& factory,
                                               std::vector<ActorPlacement>& placements,
                                               const std::vector<ActorCreateHandles>& handles,
                                               std::uint32_t& bind_counter,
                                               const ActorBindConfig& bind_config,
                                               const MarkerPoseTable& markers,
                                               bool enabled) {
    std::vector<ActorSpawn> spawns;
    if (!enabled) {
        return spawns;
    }
    for (std::size_t i = 0; i < placements.size(); ++i) {
        ActorPlacement& placement = placements[i];
        if (!placement.active) {
            continue;
        }
        const ActorCreateHandles slot_handles = i < handles.size() ? handles[i] : ActorCreateHandles{};
        const std::optional<std::size_t> slot =
            factory.create(placement.argument1, placement.argument2, slot_handles);
        placement.actor = slot;
        if (!slot) {
            // The original stores the null pointer and would write through it
            // in the finalize pass; the port skips the actor instead.
            continue;
        }
        RuntimeActor& actor = factory.actor(*slot);
        actor.object_id = placement.object_id;
        actor.field_70 = placement.field_9c;
        bind_actor_pose(bind_counter, bind_config, markers, factory.definitions(), i, actor);
        spawns.push_back({i, *slot, slot_handles.person, placement.name});
    }
    return spawns;
}

std::optional<LocalPlayerActor> bind_local_player_actor(RuntimeActorFactory& factory,
                                                        std::uint32_t player_object_id,
                                                        std::uint32_t weapon_slot_value,
                                                        std::uint32_t field_98,
                                                        std::string_view player_name) {
    for (std::size_t i = 0; i < factory.actors().size(); ++i) {
        RuntimeActor& actor = factory.actor(i);
        if ((actor.flags & 1u) == 0 || actor.object_id != player_object_id) {
            continue;
        }
        actor.weapon_slot = weapon_slot_value;
        actor.argument = field_98;
        LocalPlayerActor result;
        result.actor = i;
        result.field_94 = weapon_slot_value;
        result.field_98 = field_98;
        result.person_name = std::string(player_name);
        return result;
    }
    return std::nullopt;
}

} // namespace dfre
