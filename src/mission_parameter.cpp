#include "dfre/mission_parameter.hpp"
#include <stdexcept>
namespace dfre {namespace {
template<class T> const T& need(const T& f){if(!f)throw std::runtime_error("Parameter service reached a missing provider");return f;}
}
std::uint32_t mission_parameter_set(const MissionDebriefBindings& b,
    std::uint32_t index,std::uint32_t value){
    const auto table=need(b.read)(0x4b691c,4);
    if(table)need(b.write)(table+index*4u,4,value);
    return value;
}
std::uint32_t mission_parameter_get(const MissionDebriefBindings& b,
    std::uint32_t index){
    const auto table=need(b.read)(0x4b691c,4);
    if(table){
        const auto value=need(b.read)(table+index*4u,4);
        need(b.write)(0x4b69f0,4,value);
    }
    return need(b.read)(0x4b69f0,4);
}
}
