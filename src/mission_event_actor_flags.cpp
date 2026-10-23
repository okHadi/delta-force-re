#include "dfre/mission_event_actor_flags.hpp"
#include <cstring>
#include <limits>
namespace dfre {namespace {
using U=std::uint32_t;
U side_bits(std::uint16_t n){return n<0x8000u?U(n):U(n)|0xffff0000u;}
std::int32_t si(U n){std::int32_t s;std::memcpy(&s,&n,4);return s;}
struct Access {
 MissionTickState& state;const MissionEventActorFlagBindings& b;
 void read(U at){if(b.before_read)b.before_read(at);}
 U address(const AiActorRef& ref,U offset=0){return mission_actor_source_address(ref.group,ref.index)+offset;}
 AiActorState& actor(const AiActorRef& ref){auto* a=state.actor_at(ref);if(!a)throw MissionTickError("Event actor exceeds its current table");return *a;}
 U side(const AiActorRef& ref){read(address(ref,0x70));return side_bits(actor(ref).field_72);}
 std::int32_t count(unsigned group){read(group==1?0x8cf904:group==3?0x8cfa18:0x8cfa1c);if(b.current_count)return b.current_count(group);const auto n=state.tables.groups[group].size();if(n>std::size_t(std::numeric_limits<std::int32_t>::max()))throw MissionTickError("Event actor count exceeds signed range");return std::int32_t(n);}
 void writing(U at,U width,U value){if(b.before_write)b.before_write(at,width,value);}
 template<class F>void each(U wanted,F body){if(!wanted)return;for(unsigned group:{1u,3u,0u}){if(count(group)<=0)continue;U index=0;for(;;){const AiActorRef ref{std::uint8_t(group),index};const auto current=side(ref);if(current==wanted)body(ref);const auto bound=count(group);++index;if(si(index)>=bound)break;}}}
};
}
void mission_event_actor_flags(MissionTickState& state,U side,U& damage,const MissionEventActorFlagBindings& b){
 Access access{state,b};access.each(side,[&](const AiActorRef& ref){
  access.writing(access.address(ref,0x74),2,0xffce);access.actor(ref);state.extra_for(ref).field_74=0xffce;
  access.writing(0xa158bc,4,0);damage=0;
  access.read(access.address(ref,0x110));const auto callback=access.actor(ref).field_110;
  if(!callback || !b.update)throw MissionTickError("Event actor requires its current update body");
  b.update(ref,callback,1);
 });
}
void mission_event_actor_remove_one(MissionTickState& state,const AiActorRef& ref,const MissionEventActorFlagBindings& b){
 Access access{state,b};access.read(0x67f528);if(!state.globals.mission_67f528)return;
 const auto index=(access.address(ref)-0x7cca80u)/0x114u;
 std::array<std::uint8_t,4> payload{};for(unsigned i=0;i<4;++i)payload[i]=std::uint8_t(index>>(i*8));
 if(!b.remove_45da30)throw MissionTickError("Event actor requires45DA30");b.remove_45da30(ref);
 if(!b.broadcast_442630)throw MissionTickError("Event actor requires442630");b.broadcast_442630(0x10,payload.data(),2);
}
void mission_event_actor_remove(MissionTickState& state,U side,const MissionEventActorFlagBindings& b){Access access{state,b};access.each(side,[&](const AiActorRef& ref){mission_event_actor_remove_one(state,ref,b);});}
MissionEventActorFlagBindings mission_event_actor_flag_bindings(MissionActorLifecycle& lifecycle,MissionClientOutbox& outbox,std::function<void(const AiActorRef&,U,U)> update){
 MissionEventActorFlagBindings b;b.update=std::move(update);b.remove_45da30=[&lifecycle](const AiActorRef& ref){lifecycle.remove(ref);};b.broadcast_442630=[&outbox](U type,std::uint8_t* bytes,U length){outbox.broadcast(std::int32_t(type),bytes,length);};return b;
}
}
