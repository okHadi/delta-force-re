#include "dfre/mission_actor_resources.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
std::int64_t signed_value(U n){return n<=0x7fffffffu?std::int64_t(n):std::int64_t(n)-0x100000000ll;}
struct Memory {
 const MissionActorResourceBindings& b;
 U read(U at,unsigned width=4)const{if(!b.read)throw std::runtime_error("actor resource read provider required");return b.read(at,width);}
 void write(U at,U value,unsigned width=4)const{if(!b.write)throw std::runtime_error("actor resource write provider required");b.write(at,width,value);}
 void call(U site,std::vector<U> args={})const{if(!b.call)throw std::runtime_error("actor resource call provider required");b.call(site,args);}
 void clear(U at,U count)const{call(0x49a620,{at,0,count});}
};
}
void mission_character_release(const MissionActorResourceBindings& b,U instance){if(instance)mission_character_release_raw(b,instance);}
void mission_character_release_raw(const MissionActorResourceBindings& b,U instance){
 const Memory m{b};m.call(0x4980d0,{m.read(instance+4)});
 const auto slot=signed_value(instance-0x42fc60u)/0x9c;
 m.write(0x439860u+U(slot),0,1);
}
void mission_character_pool_clear(const MissionActorResourceBindings& b){
 const Memory m{b};for(U offset=0;offset!=0x100;offset+=4)m.write(0x439860+offset,0);
 for(U offset=0;offset!=0x9c00;offset+=4)m.write(0x42fc60+offset,0);
 for(U offset=0;offset!=0x9ed4;offset+=4)m.write(0x42fc60+offset,0);
}
void mission_character_work_clear(const MissionActorResourceBindings& b){Memory{b}.clear(0x6bbd48,0x1c104);}
void mission_character_resources_clear(const MissionActorResourceBindings& b){
 const Memory m{b};for(U offset=0;offset!=0x1900;offset+=0x64){const U allocation=m.read(0x6ba4a0+offset);if(allocation)m.call(0x4980d0,{allocation});}
 m.clear(0x6ba440,0x1900);mission_character_pool_clear(b);m.write(0x6bbd40,0);mission_character_work_clear(b);
}
void mission_actor_resources_clear(const MissionActorResourceBindings& b){
 const Memory m{b};m.clear(0x8cf908,0x10c);m.clear(0x7cca80,0x11400);m.clear(0x7dde80,0x45000);m.clear(0x867e80,0x45000);m.clear(0x822e80,0x45000);m.clear(0x8ace80,0x22800);m.clear(0x78f280,0x4400);m.clear(0x793680,0x1000);m.clear(0x794680,0x38400);
 m.call(0x42ebe0);U source=0x4cc990,target=0x8cf90c;
 for(;;){const U first=m.read(source,1);m.write(target,first,1);if(!first)return;const U second=m.read(source+1,1);source+=2;m.write(target+1,second,1);target+=2;if(!second)return;}
}
void mission_auxiliary_resources_clear(const MissionActorResourceBindings& b){
 const Memory m{b};for(U offset=0;offset!=0x3cc8;offset+=0x614){m.call(0x4980d0,{m.read(0xbfc100+offset)});m.call(0x4980d0,{m.read(0xbfc104+offset)});m.call(0x4980d0,{m.read(0xbfc10c+offset)});m.call(0x4980d0,{m.read(0xbfc110+offset)});for(U field=0;field!=0x14;field+=4)m.write(0xbfc100+offset+field,0);}
 m.write(0x500afc,0);
}
void mission_item_resource_links_clear(const MissionActorResourceBindings& b){
 const Memory m{b};if(signed_value(m.read(0xc23814))<=0)return;U row=0,offset=0;
 do{m.write(m.read(0xc23810)+offset+0x6c,0);m.write(m.read(0xc23810)+offset+0x70,0);m.write(m.read(0xc23810)+offset+0x74,0);++row;const U count=m.read(0xc23814);offset+=0x13c;if(signed_value(row)>=signed_value(count))return;}while(true);
}
}
