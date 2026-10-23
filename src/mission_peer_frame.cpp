#include "dfre/mission_peer_frame.hpp"
#include "dfre/game_messages.hpp"
#include <stdexcept>
namespace dfre {
namespace {
template<class F> const F& need(const F& f,const char* message){if(!f)throw std::runtime_error(message);return f;}
}
std::uint32_t mission_peer_frame_receive(GameMessagePeerBuffer& buffer,const MissionPeerFrameBindings& b){
    const auto read=[&](std::uint32_t address){return need(b.cell,"Peer receive cells are not bound")(address);};
    const auto write=[&](std::uint32_t address,std::uint32_t value){need(b.store,"Peer receive cell writer is not bound")(address,value);};
    const auto count=buffer.count_8ee1cc;
    if(count){
        const auto peer=read(0x50133c);
        write(0x8e61b8,count);write(0x8e61bc,peer);write(0x8e61b4,0x8ec1cc);
        buffer.count_8ee1cc=0;
        return 0x8ec1cc;
    }
    if(!read(0x67f520))return 0;
    const auto result=need(b.receive_497790,"Peer receive transport is not bound")();
    write(0x8e61b4,result);
    if(!result){write(0x8e61bc,0);write(0x8e61b8,0);return 0;}
    auto value=read(0x501344);write(0x8e61bc,value);
    value=read(0x50135c);write(0x8e61b4,value);
    value=read(0x501360);write(0x8e61b8,value);
    return read(0x8e61b4);
}
}
