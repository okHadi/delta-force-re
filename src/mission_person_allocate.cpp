#include "dfre/mission_person_allocate.hpp"
namespace dfre {
std::uint32_t mission_person_allocate(const MissionActorCreateBindings& b,std::uint32_t owner){
    if(!b.read)throw MissionActorCreateError("Person allocation needs current memory reads");
    std::uint32_t slot=0,person=0x794680,occupied=b.read(person,4);
    while(occupied){++slot;person+=0xb4;if(slot>=0x500)return 0;occupied=b.read(person,4);}
    if(!b.call)throw MissionActorCreateError("Person allocation needs its clear service");
    b.call(0x49a620,{person,0,0xb4});
    if(!b.write)throw MissionActorCreateError("Person allocation needs current memory writes");
    b.write(person,4,owner);return person;
}
} // namespace dfre
