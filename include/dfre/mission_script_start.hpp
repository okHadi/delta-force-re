#pragma once
#include "dfre/mission_script_runtime.hpp"
#include <string_view>
#include <variant>
namespace dfre {
struct MissionScriptQueueBindings {
    std::function<std::uint32_t(std::uint32_t address)> read;
    std::function<void(std::uint32_t address,std::uint32_t value)> write;
    std::function<std::int32_t(std::uint32_t left,std::uint32_t right)> compare;
    //49A620(address,0,16): clear the complete next queue cell now.
    std::function<void(std::uint32_t address)> zero16;
};
std::uint32_t mission_script_queue_start(const MissionScriptQueueBindings&,std::uint32_t resource); //455F10
void mission_script_queue_compact(const MissionScriptQueueBindings&); //455FA0
std::uint32_t mission_script_queue_remove(const MissionScriptQueueBindings&,std::uint32_t name); //456000
std::uint32_t mission_script_registry_start(const MissionScriptQueueBindings&,std::uint32_t name); //455F60
using MissionScriptStartName=std::variant<std::uint32_t,std::string_view>;
struct MissionScriptStartBindings {
    std::function<std::uint32_t()> registry_head; //8E3D58
    std::function<std::uint32_t(std::uint32_t node,std::uint32_t offset)> node_word;
    std::function<std::int32_t(std::uint32_t left,const MissionScriptStartName& right)> compare;
};
class MissionScriptStart {
public:
    // Queue comes from zeroed455C90 state. Its vector is the counted nonzero prefix.
    // Keep both owners alive; name text views remain valid through their call only.
    MissionScriptStart(MissionScriptQueueCells&,const MissionScriptStartBindings&);
    MissionScriptStart(MissionScriptQueueCells&,MissionScriptStartBindings&&)=delete;
    std::uint32_t start(std::uint32_t nonzero_resource); //455F10, valid initialized queue
    std::uint32_t start_name(std::uint32_t source_name); //455F60
    void event(std::int32_t id); //43F500, dlg%03i
private:
    std::uint32_t named(const MissionScriptStartName&);
    MissionScriptQueueCells& queue_;
    const MissionScriptStartBindings& bindings_;
};
}
