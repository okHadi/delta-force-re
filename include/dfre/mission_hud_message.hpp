#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionHudMessageBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>&)> call;
};
// Caller owns current argument storage through callbacks. Metadata is read last.
struct MissionHudMessageCall {std::uint32_t text=0,metadata=0;};
void mission_hud_message(const MissionHudMessageBindings&,const MissionHudMessageCall&); //479F00
void mission_hud_message_clear(const MissionHudMessageBindings&); //479E90
std::uint32_t mission_hud_triggered_lookup(const MissionHudMessageBindings&,std::uint32_t group,std::uint32_t key); //404D80
void mission_hud_triggered_text(const MissionHudMessageBindings&,std::uint32_t id,std::uint32_t frame); //404DD0;32-byte caller frame
// Bounded493A80 producer for current literal ID%03i at4C82FC.
std::uint32_t mission_hud_message_format_id(const MissionHudMessageBindings&,std::uint32_t destination,std::uint32_t format,std::uint32_t value);
} // namespace dfre
