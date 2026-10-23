#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
namespace dfre {
class MissionCharacterAllocateError : public std::runtime_error {public:using std::runtime_error::runtime_error;};
inline constexpr std::uint32_t mission_character_instance_base=0x42fc60;
inline constexpr std::uint32_t mission_character_occupied_base=0x439860;
inline constexpr std::size_t mission_character_instance_count=256;
inline constexpr std::size_t mission_character_instance_stride=0x9c;
inline constexpr std::size_t mission_character_source_bytes=256*0x9c+256;
struct MissionCharacterInstanceView {std::uint8_t* bytes;std::size_t length;std::uint32_t source_address;};
class MissionCharacterAllocateState {
public:
    MissionCharacterAllocateState(const std::uint8_t* cold_image,std::size_t bytes);
    MissionCharacterAllocateState(const MissionCharacterAllocateState&)=delete;
    MissionCharacterAllocateState& operator=(const MissionCharacterAllocateState&)=delete;
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width) const;
    void write(std::uint32_t address,std::uint32_t width,std::uint32_t value);
    MissionCharacterInstanceView instance(std::size_t slot);
private:std::array<std::uint8_t,mission_character_source_bytes> bytes_{};
};
struct MissionCharacterAllocateRegisters {std::uint32_t eax=0,ebx=0,ecx=0,edx=0,esi=0,edi=0,ebp=0;};
struct MissionCharacterAllocateBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<void(std::uint32_t site,MissionCharacterAllocateRegisters&)> call;
};
//439C70: EDI is the loaded CHR payload. EAX/EBX return instance/slot.
void mission_character_allocate_raw(const MissionCharacterAllocateBindings&,MissionCharacterAllocateRegisters&);
//42CCE0: frame_base is source EBP; frame_base-8..frame_base needs shared backing.
//ESP, EFLAGS and saved-stack slots are outside this register contract.
std::uint32_t mission_character_allocate(const MissionCharacterAllocateBindings&,
    std::uint32_t resource,MissionCharacterAllocateRegisters&,std::uint32_t frame_base);
} // namespace dfre
