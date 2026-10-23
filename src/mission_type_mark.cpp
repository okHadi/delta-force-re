#include "dfre/mission_type_mark.hpp"
namespace dfre {
void mission_type_mark(const MissionTypeCountBindings& b,std::uint32_t index){
    if(!b.write)throw MissionTickError("Type mark memory store is not bound");
    b.write(0x7936a4u+(index<<6),4,2);
}
}
