#pragma once
#include "dfre/mission_client_outbox.hpp"
#include <memory>
#include <stdexcept>

namespace dfre {
inline constexpr std::uint32_t mission_client_source_base=0x6e9a90;
inline constexpr std::size_t mission_client_count=17;
inline constexpr std::size_t mission_client_source_bytes=mission_client_count*mission_client_record_size;
class MissionClientLiveError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
// One current physical client pool. Views stay valid until this owner is destroyed.
class MissionClientLive {
public:
    MissionClientLive();
    ~MissionClientLive();
    MissionClientLive(const MissionClientLive&)=delete;
    MissionClientLive& operator=(const MissionClientLive&)=delete;
    MissionClientLive(MissionClientLive&&)=delete;
    MissionClientLive& operator=(MissionClientLive&&)=delete;
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width) const;
    void write(std::uint32_t address,std::uint32_t width,std::uint32_t value);
    MissionClientRecordView client(std::size_t slot);
    static std::uint32_t source_address(std::size_t slot,std::size_t offset=0);
private:
    std::unique_ptr<std::uint8_t[]> bytes_;
};
} // namespace dfre
