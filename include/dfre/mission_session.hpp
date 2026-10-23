#pragma once
#include "dfre/mission_client_live.hpp"
#include "dfre/mission_crt_thread.hpp"
#include "dfre/mission_geometry.hpp"
#include "dfre/mission_muzzle.hpp"
#include "dfre/mission_script_registry.hpp"
#include "dfre/mission_sound_runtime.hpp"
#include "dfre/input_bridge.hpp"
#include "dfre/mission_actor_insert_live.hpp"
#include "dfre/events.hpp"
#include "dfre/terrain.hpp"
#include "dfre/mission_logic.hpp"
#include <memory>

namespace dfre {
// Bind the loaded input row through the current executable memory owner.
std::optional<CommandBinding> mission_session_input_query_row(const MissionSoundRuntime&,std::size_t row);

struct MissionSessionContext {
    MissionTickState& state;
    MissionRuntimeState& runtime;
    const MissionData& mission;
    const MissionAssetCache& cache;
    const TerrainRuntime& terrain;
    MissionAnimations& animations;
    MissionGeometry& geometry;
    MissionMuzzle& muzzle;
    InputBridge& input;
    MissionSoundRuntime& sound;
    MissionScriptRegistry& scripts;
    MissionAudioFrameCells& audio;
    MissionScriptQueueCells& script_queue;
    MissionCrtThreadCells& crt_thread;
    MissionClientLive& clients;
    const MissionActorInsertBindings& extra_memory;
    std::uint32_t& rules_67f548;
    std::uint32_t& requested_8ee1d4;
    Fixed3& camera_position_6ba3d0;
};
struct MissionSessionHost {
    bool primary_source_records=false;
    std::function<bool()> pump_490880;
    std::function<std::uint64_t()> rdtsc;
    std::function<std::uint32_t()> elapsed_audio_frames;
    std::function<std::uint32_t()> camera_actor_6ba3f0;
    std::function<std::uint32_t()> camera_mode_6ba3ec;
    EventReferencePolicy event_references=EventReferencePolicy::strict;
    EventReferenceDiagnostic event_diagnostic;
};
// Construct after mission initialization. Keep all context owners alive. Destroy
// this session before resetting the mission, input, sound or script owners.
class MissionSession {
public:
    MissionSession(MissionSessionContext,MissionSessionHost);
    ~MissionSession();
    MissionSession(const MissionSession&)=delete;
    MissionSession& operator=(const MissionSession&)=delete;
    void install();
    void uninstall() noexcept;
    bool installed() const noexcept;
    MissionTickReport step();
    std::optional<std::uint32_t> hud_service(std::uint32_t site,const std::vector<std::uint32_t>& args);
    void script_message(std::uint32_t id,const std::uint8_t* bytes,std::size_t length);
    void request_result(std::uint32_t mode,MissionLogicResultPacket& caller_packet);
    std::uint32_t camera_yaw() const;
    // Route these owned cells before the sound memory fallback. Construction
    // captures initial source values before the host installs that routing.
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width) const;
    void write(std::uint32_t address,std::uint32_t width,std::uint32_t value);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace dfre
