#include "dfre/mission_actor_message.hpp"
#include <cstring>
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t value;std::memcpy(&value,&n,4);return value;}
}
void mission_actor_message_send_index(const MissionActorMessageBindings& b,U index,U message){
    std::array<std::uint8_t,2> payload{std::uint8_t(index),std::uint8_t(message)};
    if(!b.broadcast)throw std::runtime_error("Actor message4423F0 producer is not bound");
    b.broadcast(0x25,payload);
}
void mission_actor_message_send(const MissionActorMessageBindings& b,U actor,U message){mission_actor_message_send_index(b,(actor-0x7cca80u)/0x114u,message);}
void mission_actor_message_receive(const MissionActorMessageBindings& b,U actor,const U& message){
    const auto read=[&](U at){if(!b.read)throw std::runtime_error("Actor message current memory read is not bound");return b.read(at,4);};
    const auto z=read(actor+0x2c)+0x10000u;
    const auto y=read(actor+0x28);
    const auto definition=read(actor+4);
    const auto phrase=read(definition+0xd4);
    const auto x=read(actor+0x24);
    const auto current_message=message;
    const auto sound=read(0x4f7eacu+((phrase*12u+current_message)<<2));
    if(!b.positional)throw std::runtime_error("Actor message43E7D0 sound is not bound");
    b.positional(sound,{si(x),si(y),si(z)});
}
}
