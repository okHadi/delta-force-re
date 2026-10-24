#include "dfre/mission_profile_start.hpp"
#include <stdexcept>
namespace dfre {namespace {
template<class T> const T& need(const T& f){if(!f)throw std::runtime_error("Profile start reached a missing provider");return f;}
}
void mission_profile_start_mask(const MissionDebriefBindings& b){
    const auto setting=need(b.read)(0x4cee90,4);
    const auto profile=need(b.read)(0x4cf2e4,4);
    need(b.write)(0x67f54c,4,setting);
    const auto campaign=need(b.read)(profile+0x28u,4);
    const auto mask=need(b.read)(profile+0x70u+campaign*0x44u,4);
    need(b.write)(0x67f70c,4,mask);
}
std::uint32_t mission_profile_start_result(const MissionDebriefBindings& b){
    const auto mask=need(b.read)(0x67f70c,4);
    need(b.write)(0x67dbec,4,mask);
    need(b.call)(0x49a620,{0x67f714,0,0x514});
    const auto rules=need(b.read)(0x67f548,4);
    const auto branch=need(b.read)(0x503328,4);
    need(b.write)(0x67dbe4,4,rules);
    return branch;
}
}
