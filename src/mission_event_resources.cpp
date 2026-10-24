#include "dfre/mission_event_resources.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
std::int64_t signed_value(U n){return n<=0x7fffffffu?std::int64_t(n):std::int64_t(n)-0x100000000ll;}
struct Memory {
 const MissionEventResourceBindings& b;
 U read(U at,unsigned width=4)const{if(!b.read)throw std::runtime_error("event resource read provider required");return b.read(at,width);}
 void write(U at,U value,unsigned width=4)const{if(!b.write)throw std::runtime_error("event resource write provider required");b.write(at,width,value);}
 U call(U site,std::vector<U> args={})const{if(!b.call)throw std::runtime_error("event resource call provider required");return b.call(site,args);}
};
}
void mission_event_resources_reset(const MissionEventResourceBindings& b){
 const Memory m{b};for(U at:{0x4f2bccu,0x4f2bd4u,0x4f2bdcu}){const U pointer=m.read(at);if(pointer)m.call(0x4980d0,{pointer});}
 m.write(0x4f2bc8,0);m.write(0x4f2bd0,0);m.write(0x4f2bd8,0);
 m.call(0x49a620,{0x6e0050,0,0x200});m.call(0x49a620,{0x6d7e50,0,0x200});m.call(0x49a620,{0x6d8050,0,0x8000});m.write(0x6e0254,0);m.write(0x6e0250,0);
}
void mission_event_resources_load(const MissionEventResourceBindings& b){
 const Memory m{b};mission_event_resources_reset(b);
 m.call(0x499280,{0x4f2bc8,4});m.call(0x499280,{0x4f2bd0,4});m.call(0x499280,{0x4f2bd8,4});
 const U event_size=m.read(0x4f2bc8)*24u;const U trigger_size=m.read(0x4f2bd0)<<5;const U action_size=m.read(0x4f2bd8)<<5;
 const U events=m.call(0x497f60,{event_size,0x10,0,0x4cbf5c});m.write(0x4f2bcc,events);
 const U triggers=m.call(0x497f60,{trigger_size,0x10,0,0x4cbf64});m.write(0x4f2bd4,triggers);
 const U actions=m.call(0x497f60,{action_size,0x10,0,0x4cbf70});const U current_events=m.read(0x4f2bcc);m.write(0x4f2bdc,actions);
 m.call(0x499280,{current_events,event_size});m.call(0x499280,{m.read(0x4f2bd4),trigger_size});m.call(0x499280,{m.read(0x4f2bdc),action_size});
 if(signed_value(m.read(0x4f2bc8))>0){U row=0,offset=0;do{
  const U first=m.read(0x4f2bcc);const U trigger_index=m.read(first+offset+4);const U trigger_base=m.read(0x4f2bd4);m.write(first+offset+4,trigger_base+(trigger_index<<5));
  const U second=m.read(0x4f2bcc);const U action_index=m.read(second+offset+8);const U action_base=m.read(0x4f2bdc);m.write(second+offset+8,action_base+(action_index<<5));
  ++row;const U count=m.read(0x4f2bc8);offset+=24;if(signed_value(row)>=signed_value(count))break;
 }while(true);}
 m.call(0x42eea0);m.call(0x42f020);m.write(0x4f2be0,m.read(0x4f2be0,1)^1u,1);
}
}
