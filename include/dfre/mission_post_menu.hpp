#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
namespace dfre {
class MissionPostMenuError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
struct MissionPostMenuBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
void mission_post_menu_enter(const MissionPostMenuBindings&); //424540
std::uint32_t mission_post_menu_update(const MissionPostMenuBindings&); //4245B0
void mission_post_menu_render(const MissionPostMenuBindings&); //424730
void mission_post_menu_leave(const MissionPostMenuBindings&); //424780
}
