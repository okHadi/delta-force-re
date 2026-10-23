#pragma once
#include "dfre/mission_client_outbox.hpp"
#include <array>
#include <functional>
namespace dfre {
struct MissionPeerParserCall {
    std::uint32_t address=0;
    std::array<std::uint32_t,5> args{};
    std::uint8_t count=0;
    // A view of the pointer argument. Stack-local views have args[index]==0.
    std::int8_t payload_argument=-1;
    std::uint8_t* payload=nullptr;
    std::size_t length=0;
};
struct MissionPeerParserBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint8_t*(std::uint32_t,std::size_t)> resolve;
    std::function<MissionClientRecordView(std::size_t)> client;
    std::function<std::uint32_t(const MissionPeerParserCall&)> call;
};
// 442AE0. Client spans and source views stay stable through synchronous callbacks.
void mission_peer_parser_parse(MissionClientOutbox&,GameMessageTable&,const MissionPeerParserBindings&);
}
