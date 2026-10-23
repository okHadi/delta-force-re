#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace dfre {
struct MissionGameTransportBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint32_t(std::uint32_t target,const std::vector<std::uint32_t>&)> call;
    // Map a current source frame. Keep it mapped until release.
    std::function<std::uint32_t(std::uint32_t site,std::uint32_t bytes)> frame;
    // Complete release without an error, including while another call fails.
    std::function<void(std::uint32_t site,std::uint32_t address,std::uint32_t bytes)> release;
    // Optional synchronous call observer, before the selected body runs.
    std::function<void(std::uint32_t target,const std::vector<std::uint32_t>&)> before_call;
};
// The wrappers read the current501398 table and its current function pointer.
std::uint32_t mission_game_transport_receive(const MissionGameTransportBindings&); //497790
std::uint32_t mission_game_transport_send_server(const MissionGameTransportBindings&,
    std::uint32_t data,std::uint32_t length); //4977A0
std::uint32_t mission_game_transport_send_client(const MissionGameTransportBindings&,
    std::uint32_t data,std::uint32_t length,std::uint32_t peer); //4977E0
// Direct Netsock bodies use the same mapped cells and required DLL services.
std::uint32_t mission_game_transport_receive_netsock(const MissionGameTransportBindings&); //4B9530
std::uint32_t mission_game_transport_send_server_netsock(const MissionGameTransportBindings&,
    std::uint32_t data,std::uint32_t length); //4B95D0
std::uint32_t mission_game_transport_send_client_netsock(const MissionGameTransportBindings&,
    std::uint32_t data,std::uint32_t length,std::uint32_t peer); //4B96B0
std::uint32_t mission_game_transport_initialize_netsock(const MissionGameTransportBindings&); //4B8EC0
void mission_game_transport_close_netsock(const MissionGameTransportBindings&); //4B8EF0/4B8E10
} // namespace dfre
