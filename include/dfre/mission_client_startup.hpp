#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
namespace dfre {
class MissionClientStartupError : public std::runtime_error {public:using std::runtime_error::runtime_error;};
struct MissionClientStartupBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>& args)> call;
    std::function<void(std::uint32_t site,const std::vector<std::uint32_t>& args)> before_call;
};
void mission_client_startup(const MissionClientStartupBindings&); //43FF30
void mission_client_setup_local(const MissionClientStartupBindings&,std::uint32_t name,
    std::uint32_t face,std::uint32_t side); //444C50
void mission_client_assign_team(const MissionClientStartupBindings&,std::uint32_t client,
    std::uint32_t requested); //4406E0
void mission_client_register_input(const MissionClientStartupBindings&,std::uint32_t source); //4715B0
void mission_client_reset_banned(const MissionClientStartupBindings&); //4487E0
//4974C0/4974D0/4974E0/497500. Store the current source callback address.
void mission_client_register_callback(const MissionClientStartupBindings&,std::uint32_t site,
    std::uint32_t callback);
}
