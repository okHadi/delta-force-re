#include "dfre/mission_control_gate.hpp"
#include <cstring>
namespace dfre {
std::int32_t mission_control_gate(const MissionControlGateBindings& bindings){
    if(!bindings.read)throw MissionControlGateError("Control gate needs current shared cells");
    if(bindings.read(0x67f520,4)==0)return 0;
    if(bindings.read(0x67f528,4)!=0)return 0;
    const std::uint32_t shifted=bindings.read(0xbc5d20,4)<<4;
    std::int32_t numerator;std::memcpy(&numerator,&shifted,4);
    return numerator/1000;
}
}
