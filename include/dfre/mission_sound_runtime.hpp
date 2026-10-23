#pragma once
#include "dfre/assets.hpp"
#include "dfre/mission_audio_frame.hpp"
#include "dfre/mission_sound_assets.hpp"
#include "dfre/mission_sound_bank.hpp"
#include "dfre/terrain.hpp"
#include <memory>
#include <optional>
namespace dfre {
struct MissionSoundRuntimeBindings {
    std::function<std::uint32_t(std::uint32_t actor,std::uint32_t offset)> actor_word;
    std::function<std::optional<std::uint32_t>(std::uint32_t address,std::uint32_t width)> record_base;
    std::function<std::uint32_t(std::uint32_t base,std::uint32_t offset,std::uint32_t width)> record_read;
    std::function<void(std::uint32_t base,std::uint32_t offset,std::uint32_t width,std::uint32_t value)> record_store;
    std::function<std::uint32_t()> script_key; // current503228
    std::function<void(std::uint32_t address,std::uint32_t token,std::uint32_t sample)> completed;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>&)> service;
};
// Keep the asset store, frame cells and current terrain pointer alive.
// The software voice owner stays enabled; this owner opens no output device.
class MissionSoundRuntime {
public:
    MissionSoundRuntime(const AssetStore&,const AssetBytes& executable,
        MissionAudioFrameCells&,const TerrainGeometry*&,MissionSoundRuntimeBindings);
    MissionSoundRuntime(const AssetStore&&,const AssetBytes&,
        MissionAudioFrameCells&,const TerrainGeometry*&,MissionSoundRuntimeBindings)=delete;
    ~MissionSoundRuntime();
    MissionSoundRuntime(const MissionSoundRuntime&)=delete;
    MissionSoundRuntime& operator=(const MissionSoundRuntime&)=delete;
    std::uint32_t load_context(std::string_view name,std::uint32_t context,std::uint32_t tag=0xffffffffu);
    void initialize_definitions(std::string_view name); //43E340
    void resolve_definitions(std::string_view primary,std::string_view alternate); //43E4A0
    void global(std::uint32_t id); //43E720
    void positional(std::uint32_t id,Fixed3);
    void attach(std::uint32_t raw_actor,std::uint32_t id,std::uint32_t scale,std::uint32_t ceiling);
    void stop(std::uint32_t raw_actor,std::uint32_t scale); //43F020
    std::uint32_t loudness(std::uint32_t id);
    std::uint32_t definition_id(std::string_view name); //43F490
    void frame(); //43F0B0
    void publish();
    void advance_frames(std::uint32_t actual_sample_frames);
    const MissionSoundAssetBindings& asset_bindings() const;
    const MissionSoundBankBindings& bank_bindings() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
