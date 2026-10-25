#include "dfre/mission_event_region_person.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
std::int64_t si(U value){return value<0x80000000u?std::int64_t(value):std::int64_t(value)-0x100000000ll;}
U read(const MissionEventPersonBindings& b,U at){if(!b.read)throw MissionTickError("Region person read is not bound");return b.read(at,4);}
}
void mission_event_region_person(const MissionEventPersonBindings& b,U action){
 const U kind=read(b,action+4);const U classification=kind==12?2:kind==13?1:0;if(!classification)return;
 if(!read(b,action+8))return;
 const U min_x=read(b,action+0xc),max_x=read(b,action+0x14),min_y=read(b,action+0x18),max_y=read(b,action+0x1c);
 U count=read(b,0x8cfa18),index=0,actor=0x7cca80;if(si(count)<=0)return;
 for(;;){const U word=read(b,actor+0x6e);const U current=word>>16;const auto signed_class=current<0x8000u?std::int64_t(current):std::int64_t(current)-0x10000ll;
  if(signed_class==classification){const U x=read(b,actor+0x24);if(si(x)>=si(min_x)&&si(x)<=si(max_x)){const U y=read(b,actor+0x28);if(si(y)>=si(min_y)&&si(y)<=si(max_y))mission_event_person_edit(b,action,actor);}}
  count=read(b,0x8cfa18);++index;actor+=0x114;if(si(index)>=si(count))break;
 }
}
}
