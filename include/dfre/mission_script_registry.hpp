#pragma once
#include "dfre/mission_script_start.hpp"
#include "dfre/mission_sound_bank.hpp"
#include <memory>
namespace dfre {
std::uint32_t mission_script_registry_node(const MissionSoundBankBindings&); //455C30
void mission_script_registry_clear_queue(const MissionSoundBankBindings&); //4560A0
void mission_script_registry_destroy(const MissionSoundBankBindings&); //455EA0
// Scratch holds the source function's0x158-byte local area through nested calls.
std::uint32_t mission_script_registry_load(const MissionSoundBankBindings&,
    std::uint32_t name,std::uint32_t scratch); //455C90
std::uint32_t mission_script_registry_audio(const MissionSoundBankBindings&,
    std::uint32_t name,std::uint32_t context); //49D400
void mission_script_registry_audio_destroy(const MissionSoundBankBindings&,
    std::uint32_t context); //49CF70
std::int32_t mission_script_registry_compare(const MissionSoundBankBindings&,std::uint32_t left,std::uint32_t right); //49AD30
std::uint32_t mission_script_registry_token(const MissionSoundBankBindings&,std::uint32_t text,
    std::uint32_t delimiters,std::uint32_t scratch32); //49BA00/4BAD90
std::uint32_t mission_script_registry_id(const MissionSoundBankBindings&,std::uint32_t text); //49DFF0
bool mission_script_registry_step(const MissionSoundBankBindings&,std::uint32_t record); //43E430
void mission_script_registry_text(const MissionSoundBankBindings&,std::uint32_t text); //43E470
struct MissionScriptRegistryBindings {
    std::function<bool(std::uint32_t callback,MissionScriptStep&)> step;
    std::function<void(std::uint32_t callback,std::uint32_t text)> show_text;
    std::function<void(std::uint32_t name)> missing_audio;
    std::function<void(std::uint32_t,const std::array<std::uint8_t,10>&)> message;
    // Current CRT thread block for the filename tokenizer's+10 cell.
    std::function<std::uint32_t()> crt_thread;
    std::function<std::uint32_t()> voice_volume; //current67F51C, read at each source site
};
class MissionScriptRegistry {
public:
    // Keep shared cells/backend/scratch alive. Scratch is caller-owned shared memory.
    MissionScriptRegistry(MissionScriptRuntimeCells&,const MissionSoundBankBindings&,
        std::uint32_t scratch_address,std::uint32_t scratch_bytes,MissionScriptRegistryBindings);
    MissionScriptRegistry(MissionScriptRuntimeCells&,const MissionSoundBankBindings&&,
        std::uint32_t,std::uint32_t,MissionScriptRegistryBindings)=delete;
    ~MissionScriptRegistry();
    MissionScriptRegistry(const MissionScriptRegistry&)=delete;
    MissionScriptRegistry& operator=(const MissionScriptRegistry&)=delete;
    std::optional<std::uint32_t> service(std::uint32_t site,const std::vector<std::uint32_t>& args);
    MissionScriptStart& start();
    MissionFrameServiceBindings frame_bindings();
    const MissionSoundBankBindings& memory() const;
    // Reject partial access to a current typed cell.
    bool owns(std::uint32_t address,std::uint32_t width) const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
