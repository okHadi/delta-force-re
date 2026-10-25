#pragma once
#include "dfre/mission_character_identity.hpp"
namespace dfre {
struct MissionCharacterMemoryEntry {
    std::uint32_t token,physical,payload;
    std::uint64_t incarnation;
};
struct MissionCharacterMemoryBindings {
    //Visit current publication metadata. Keep owners stable during each call.
    std::function<void(const std::function<void(MissionCharacterMemoryEntry)>&)> entries;
    std::function<AnimationController&(std::uint32_t token)> controller;
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::optional<MissionCharacterAllocationIdentity>(std::uint32_t payload)> allocation;
};
//Borrow the published owners. Each access gets a fresh controller view.
//Use raw backing providers here, without routing them through this codec.
class MissionCharacterMemory {
public:
    MissionCharacterMemory(MissionCharacterIdentity&,MissionCharacterMemoryBindings);
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width) const;
    void write(std::uint32_t address,std::uint32_t width,std::uint32_t value);
    //Write final reviewed state to raw backing before real source release.
    //Keep controller/publication state stable. Backing writes may commit a
    //prefix and throw; they must not change these owners or typed state.
    void handoff(std::uint32_t token);
private:
    MissionCharacterIdentity& identity_;
    MissionCharacterMemoryBindings bindings_;
};
}
