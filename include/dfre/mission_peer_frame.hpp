#pragma once
#include <cstdint>
#include <functional>
namespace dfre {
struct GameMessagePeerBuffer;
struct MissionPeerFrameBindings {
    std::function<std::uint32_t(std::uint32_t)> cell;
    std::function<void(std::uint32_t,std::uint32_t)> store;
    // Returns original EAX; the provider updates current 501344/50135C/501360.
    std::function<std::uint32_t()> receive_497790;
};
// 460E40. Returns a source address. The parser reads the published shared cells.
std::uint32_t mission_peer_frame_receive(GameMessagePeerBuffer&,const MissionPeerFrameBindings&);
}
