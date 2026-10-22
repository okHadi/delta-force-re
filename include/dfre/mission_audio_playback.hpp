#pragma once
#include "dfre/audio.hpp"
#include <functional>
namespace dfre {
inline constexpr std::uint32_t mission_audio_voice_base=0x4a16d2;
struct MissionAudioCompiledVoice {
    std::uint32_t frequency=0x12345678,end=0xfedcba98;
    std::array<std::uint8_t,8> right_kernel{},left_kernel{};
    std::uint32_t right_shift_code=0x0302e0c1,left_shift_code=0x0302e0c1;
};
struct MissionAudioPlaybackCells {
    std::uint32_t& enabled_d08940;
    std::array<std::uint8_t,0x1038> records_4a16d2{};
    std::uint32_t master_4a2716=255,generation_4a2ae0=1,disabled_4a2712=0;
    std::uint32_t scan_4b4f5a=0,frames_4a35b0=0,completion_4a2ae8=0x4a2aec;
    std::array<MissionAudioCompiledVoice,16> compiled{};
    // Source4A4ADA filter initialization cells; sample mixing owns their use.
    std::uint32_t filter_4a4bcc=0,filter_4a4bd0=0,filter_4a4bb4=0,filter_4a4bbc=0;
    std::uint32_t filter_4a4bb8=0,filter_4a4bc0=0,filter_4a4bc4=0,filter_4a4bc8=0;
    std::uint8_t filter_4b4ca2=0,filter_4b4ca5=0;
};
struct MissionAudioSampleView {
    std::uint8_t* bytes=nullptr;
    std::size_t length=0;
    std::uint32_t source_address=0;
};
struct MissionAudioPlaybackBindings {
    std::function<MissionAudioSampleView(std::uint32_t)> sample;
    std::function<std::uint8_t(std::uint32_t)> pcm_byte;
    std::function<std::uint8_t(std::uint32_t)> static_byte; // reached table reads outside embedded windows
    std::function<void(std::uint32_t address,std::uint32_t token,std::uint32_t sample_address)> completed;
};
// Keep one shared cells/bindings owner. The class opens no device.
class MissionAudioPlayback {
public:
    MissionAudioPlayback(MissionAudioPlaybackCells&,const MissionAudioPlaybackBindings&);
    MissionAudioPlayback(MissionAudioPlaybackCells&,MissionAudioPlaybackBindings&&)=delete;
    void initialize(); //4A2AED queue initialization and4A4ADA
    std::uint32_t play(std::uint32_t sample,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate); //494DD0
    std::uint32_t play_channel(std::uint32_t channel,std::uint32_t sample,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate); //494E80
    std::uint32_t start_ambient(std::uint32_t sample,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate,std::uint32_t channel); //494F40
    std::uint32_t update(std::uint32_t handle,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate); //494FA0
    std::uint32_t playing(std::uint32_t handle) const; //494FF0
    void stop(std::uint32_t handle); //495070
    void register_completion(std::uint32_t address); //495160
    void publish(); //4A48FC record publication and current frequency/end operands
    // Queue/position/PCM-read slice of4A35BC. Supply actual elapsed sample frames.
    // PCM accumulation and output filters have a separate owner.
    void advance_frames(std::uint32_t);
private:
    std::uint32_t read(std::uint32_t,unsigned=4) const;
    void write(std::uint32_t,std::uint32_t,unsigned=4);
    std::uint32_t start(std::uint32_t record,std::uint32_t sample,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate,std::uint32_t callback);
    void change(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t);
    void silence(std::uint32_t);
    void compile(std::size_t,bool);
    std::uint8_t pcm(std::uint32_t) const;
    MissionAudioPlaybackCells& cells_;
    const MissionAudioPlaybackBindings& bindings_;
};
} // namespace dfre
