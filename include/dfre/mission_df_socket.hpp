#pragma once
#include "dfre/mission_client_startup.hpp"
namespace dfre {
struct MissionDfSocketBindings : MissionClientStartupBindings {
    // Map current caller stack bytes for this source frame. Keep them through release.
    std::function<std::uint32_t(std::uint32_t site,std::uint32_t bytes,
        const std::vector<std::uint32_t>& arguments)> frame;
    // Frame release completes without an error, including during a body error.
    std::function<void(std::uint32_t site,std::uint32_t address,std::uint32_t bytes)> release;
};
std::uint32_t mission_df_socket_start(const MissionDfSocketBindings&,std::uint32_t mode,
    std::uint32_t address,std::uint32_t port); //454BB0
std::uint32_t mission_df_socket_buffers(const MissionDfSocketBindings&); //454AB0
std::uint32_t mission_df_socket_open_send(const MissionDfSocketBindings&,std::uint32_t port); //454A10
std::uint32_t mission_df_socket_open_receive(const MissionDfSocketBindings&,std::uint32_t mode,
    std::uint32_t port_address); //454930
std::uint32_t mission_df_socket_set_buffer(const MissionDfSocketBindings&,bool receive,
    std::uint32_t bytes); //454890/454900
std::uint32_t mission_df_socket_get_buffer(const MissionDfSocketBindings&,bool receive); //454850/4548C0
std::uint32_t mission_df_socket_resolve(const MissionDfSocketBindings&,std::uint32_t name); //454DF0
}
