#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace dfre {
class MissionAudioCacheError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
struct MissionAudioCacheBindings {
    // Width is1,2,or4 bytes. Source addresses identify shared storage.
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    // Required reached bodies:497F60 allocator,498D60/498D30 loose seek/read,
    //49E6E0/49E6B0 archive seek/read,and import slot4C6854 (_lclose).
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>&)> call;
};
void mission_audio_cache_load(const MissionAudioCacheBindings&,std::uint32_t asset,std::uint32_t alternate); //49D050
std::uint32_t mission_audio_cache_convert(const MissionAudioCacheBindings&,std::uint32_t record); //494C50
std::uint32_t mission_audio_cache_allocate_alternate(const MissionAudioCacheBindings&,std::uint32_t size); //498120
std::uint32_t mission_audio_cache_tag(const MissionAudioCacheBindings&,std::uint32_t pointer,std::uint32_t tag); //498230
std::uint32_t mission_audio_cache_seek(const MissionAudioCacheBindings&,std::uint32_t offset,std::uint32_t origin); //4992D0
std::uint32_t mission_audio_cache_read(const MissionAudioCacheBindings&,std::uint32_t destination,std::uint32_t size); //499280
std::uint32_t mission_audio_cache_close(const MissionAudioCacheBindings&); //499020
std::uint32_t mission_audio_cache_close_loose(const MissionAudioCacheBindings&); //498D50
} // namespace dfre
