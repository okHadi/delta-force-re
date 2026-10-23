#include "dfre/mission_client_live.hpp"
#include <limits>

namespace dfre {
namespace {
constexpr std::uint64_t begin=mission_client_source_base;
constexpr std::uint64_t end=begin+mission_client_source_bytes;
static_assert(mission_client_source_bytes==0xa53c4);
}
MissionClientLive::MissionClientLive():bytes_(new std::uint8_t[mission_client_source_bytes]{}){}
MissionClientLive::~MissionClientLive()=default;
bool MissionClientLive::claims(std::uint32_t address,std::uint32_t width) const {
    if(width!=1&&width!=2&&width!=4)throw MissionClientLiveError("Client access width must be 1, 2 or 4");
    const std::uint64_t first=address,last=first+width;
    if(last>std::uint64_t(std::numeric_limits<std::uint32_t>::max())+1)
        throw MissionClientLiveError("Client source access wraps its address");
    if(first>=begin&&last<=end)return true;
    if(first<end&&last>begin)throw MissionClientLiveError("Client source access crosses the pool boundary");
    return false;
}
std::uint32_t MissionClientLive::read(std::uint32_t address,std::uint32_t width) const {
    if(!claims(address,width))throw MissionClientLiveError("Client source address is outside the pool");
    const auto offset=std::size_t(address-mission_client_source_base);std::uint32_t value=0;
    for(std::uint32_t i=0;i<width;++i)value|=std::uint32_t(bytes_[offset+i])<<(8*i);
    return value;
}
void MissionClientLive::write(std::uint32_t address,std::uint32_t width,std::uint32_t value) {
    if(!claims(address,width))throw MissionClientLiveError("Client source address is outside the pool");
    const auto offset=std::size_t(address-mission_client_source_base);
    for(std::uint32_t i=0;i<width;++i)bytes_[offset+i]=std::uint8_t(value>>(8*i));
}
MissionClientRecordView MissionClientLive::client(std::size_t slot) {
    if(slot>=mission_client_count)throw MissionClientLiveError("Client slot is outside the pool");
    return {bytes_.get()+slot*mission_client_record_size,mission_client_record_size};
}
std::uint32_t MissionClientLive::source_address(std::size_t slot,std::size_t offset) {
    if(slot>=mission_client_count||offset>=mission_client_record_size)
        throw MissionClientLiveError("Client field is outside its record");
    return mission_client_source_base+std::uint32_t(slot*mission_client_record_size+offset);
}
} // namespace dfre
