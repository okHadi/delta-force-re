#include "dfre/mission_primary_actor_live.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/mission_object_effects.hpp"
#include "dfre/mission_client_outbox.hpp"
#include "dfre/mission_animation.hpp"
#include <cassert>
#include <map>
using namespace dfre;using U=std::uint32_t;
namespace {void put(AiActorImage& image,U at,U n){for(U i=0;i<4;++i)image.at(at+i)=std::uint8_t(n>>(i*8));}}
int main(){
 const auto definitions=[](){std::vector<AiItemDefinition> result(3);result[1].field_28=6;result[2].field_28=6;return result;};
 MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions()),std::vector<AiPersonState>(2),0);
 constexpr U A=0x7dde80,D=0x18000000,S=0x1d000000;std::map<U,std::uint8_t> bytes;for(U at:{D,S})for(U i=0;i<0x400;++i)bytes[at+i]=0;auto put_raw=[&](U at,U n){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};put_raw(D+0x13c+0x28,6);put_raw(D+0x13c+0x24,4);put_raw(S,1);put_raw(S+4,D+0x13c);put_raw(S+0x24,11);
 unsigned person_called=0;MissionPrimaryActorLive* owner=nullptr;MissionPrimaryActorLiveBindings b;b.initial_definition=[](std::size_t){return 0u;};b.definition_index=[=](U raw){if(raw<D||raw>=D+3*0x13c||(raw-D)%0x13c)throw MissionTickError("Definition source is not current");return (raw-D)/0x13c;};b.definition_address=[=](U index){return D+index*0x13c;};b.delegated.read=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(i*8);return n;};b.delegated.write=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};b.delegated.person=[&](U actor){const auto slot=(actor-A)/0x114;assert(slot<2&&owner->read(0x8cfa1c,4)==slot+1&&state.tables.groups[0][slot].definition_index==1);state.tables.groups[0][slot].person_slot=slot;state.people[slot].owner=actor_handle(0,slot);++person_called;return 0x794680u+slot*0xb4;};
 auto invalid=[&](auto body){bool failed=false;try{body();}catch(const std::exception&){failed=true;}assert(failed);};
 invalid([&]{MissionPrimaryActorLive wrong(state,b,std::vector<AiActorImage>(1022));});std::vector<AiActorImage> tail(1023);put(tail.back(),0x110,0xaabbccdd);MissionPrimaryActorLive live(state,b,std::move(tail));owner=&live;
 assert(live.read(A+1023*0x114+0x110,4)==0xaabbccdd);assert(live.record_base(A+0x113,1)==A);invalid([&]{live.claims(A-1,2);});invalid([&]{live.read(A+0x113,2);});invalid([&]{live.write(0x8cfa1d,1,1);});invalid([&]{live.write(0x8cfa1c,4,0);});
 live.write(A,4,1);assert(live.read(A,4)==1&&live.read(A+4,4)==0&&state.tables.groups[0][0].active);invalid([&]{live.write(A+4,2,1);});assert(live.read(A+4,4)==0);live.write(A+4,4,D+0x13c);assert(state.tables.groups[0][0].definition_index==1);live.write(A,4,0);live.write(A+4,4,0);
 auto insertion=live.insertion_bindings();assert(mission_actor_insert(insertion,S,0)==A);assert(person_called&&live.read(A+0xc,4)==0x794680&&state.tables.groups[0][0].position[0]==11);state.tables.groups[0][0].position[0]=22;assert(live.read(A+0x24,4)==22);invalid([&]{live.write(A+0xc,4,0);});assert(live.read(A+0xc,4)==0x794680);
 put_raw(S+0x24,33);const auto* stable=&state.tables.groups[0][0];assert(mission_actor_insert(insertion,S,0)==A+0x114);assert(live.read(0x8cfa1c,4)==2&&state.tables.groups[0].size()==2&&state.tables.groups[0][1].position[0]==33&&stable==&state.tables.groups[0][0]);assert(person_called==2&&live.read(A+0x114+0xc,4)==0x794734);assert(state.stored_actor_images[0].size()==2&&state.actor_extras[0].size()==2&&state.runtimes[0].size()==2&&state.person_encoding[0].size()==2);
 state.tables.groups[0][1].position[1]=44;assert(live.read(A+0x114+0x28,4)==44);auto image=snapshot_mission_actor(state,{0,1});put(image,0,2);put(image,4,2);apply_mission_actor_image(state,{0,1},image);assert(live.read(A+0x114+4,4)==D+2*0x13c);state.tables.groups[0][1].person_slot.reset();apply_mission_actor_image(state,{0,1},AiActorImage{});assert(live.read(A+0x114,4)==0&&live.read(A+0x114+4,4)==0);
 invalid([&]{live.write(0x8cfa1c,4,1);});assert(state.tables.groups[0].size()==2);invalid([&]{live.write(0x8cfa1c,4,1025);});assert(live.read(0x8cfa1c,4)==2);
 put_raw(D+0x13c+0x24,0);MissionTickState full_state;init_mission_tick(full_state,{},AiActorTables{},AiItemDefinitionTable(definitions()),{},0);std::vector<AiActorImage> full_tail(1024);for(U slot=0;slot<1023;++slot){put(full_tail[slot],0,1);put(full_tail[slot],4,D+0x13c);}MissionPrimaryActorLive full(full_state,b,std::move(full_tail));auto full_memory=full.insertion_bindings();assert(mission_actor_insert(full_memory,S,0)==A+1023*0x114&&full_state.tables.groups[0].size()==1024);assert(mission_actor_insert(full_memory,S,0)==0);
 // Real effect image allocation, canonical primary publication, and shared outbox.
 for(U i=0x400;i<0x1000;++i)bytes[S+i]=0;put_raw(D+0x13c+0x20,0xfce);MissionTickState allocated;init_mission_tick(allocated,{},AiActorTables{},AiItemDefinitionTable(definitions()),{},0);MissionPrimaryActorLive allocation(allocated,b,std::vector<AiActorImage>(1024));auto allocation_memory=allocation.insertion_bindings();GameMessageStage messages;MissionServerMessageBuffer server;std::vector<std::array<std::uint8_t,mission_client_record_size>> clients(17);clients[0][0x18c]=1;clients[0][0x1c4]=5;MissionClientOutboxBindings ob;ob.client=[&](std::size_t slot){return MissionClientRecordView{clients.at(slot).data(),clients.at(slot).size()};};ob.cell=[](U){return 0u;};MissionClientOutbox outbox(messages,server,ob);
 MissionObjectEffectBindings effect;effect.read=[&](U at,U width){if(at==0xc23810)return D;return allocation_memory.read(at,width);};effect.write=allocation_memory.write;effect.call=[&](U site,const std::vector<U>& args)->U{if(site==0x4828c0){assert(args.at(0)==0xfce);return 1;}if(site==0x49a620){for(U i=0;i<args.at(2);++i)effect.write(args.at(0)+i,1,args.at(1));return args.at(0);}if(site==0x45d970)return mission_actor_insert(allocation_memory,args.at(0),args.at(1));if(site==0x460d20){for(U i=0;i<4;++i)put_raw(S+0x120+i*4,args.at(i));return mission_object_effect_allocate(effect,S);}if(site==0x442630){std::vector<std::uint8_t> packet(args.at(2));for(U i=0;i<args.at(2);++i)packet[i]=std::uint8_t(effect.read(args.at(1)+i,1));outbox.broadcast(std::int32_t(args.at(0)),packet.data(),U(packet.size()));return 0;}throw MissionTickError("Unowned primary effect service");};
 const U packet=S+0x800;put_raw(packet+0x38,0);put_raw(packet+0x3c,111);put_raw(packet+0x40,222);put_raw(packet+0x44,333);mission_object_effect_publish(effect,packet);assert(allocated.tables.groups[0].size()==1&&allocated.tables.groups[0][0].position[0]==111&&allocated.tables.groups[0][0].position[1]==222&&allocated.tables.groups[0][0].position[2]==333);assert(allocation.read(A+4,4)==D+0x13c&&clients[0][0x204+8]==0x2b&&clients[0][0x204+12]==0xce&&clients[0][0x204+13]==0x0f);
 // Exact foreign-memset clear retains the counted extent and all physical tails.
 CharacterModel character;character.runtime_prepared=true;character.bone_map={0};
 MissionAnimations animations({{kCharacterInstanceBase,&character,{{0,0,0,false,0,0,0,0}}}});
 auto& actor=state.tables.groups[0][0];actor.field_18=kCharacterInstanceBase;
 state.bind_animation({0,0},animations.controller(kCharacterInstanceBase),animations.actions(kCharacterInstanceBase));
 auto& runtime=state.runtime_for({0,0});runtime.byte_6e=7;runtime.roll_38=19;runtime.vertical_44=-22;runtime.ground_100=0x778899;runtime.link_90=0xaabbcc;runtime.anim_flags_48=17;runtime.stance_64=3;
 state.actor_extras[0][0].field_f8=0xabcdef;state.person_encoding[0][0]=ActorPersonEncoding::handle;
 live.write(A,4,2); // Keep a mixed raw index/definition header during clear.
 const auto person_owner=state.people[0].owner;const auto before_callbacks=person_called;
 const auto actor_capacity=state.tables.groups[0].capacity(),image_capacity=state.stored_actor_images[0].capacity(),extra_capacity=state.actor_extras[0].capacity(),runtime_capacity=state.runtimes[0].capacity(),encoding_capacity=state.person_encoding[0].capacity();
 const auto* actor_address=&state.tables.groups[0][0];
 state.actor_extras[0].push_back({});invalid([&]{live.clear_physical_pool();});
 assert(state.tables.groups[0][0].active&&state.tables.groups[0][0].position[0]==22&&state.runtimes[0][0]->driver&&state.people[0].owner==person_owner);
 state.actor_extras[0].pop_back();assert(live.read(A,4)==2&&live.read(A+1023*0x114+0x110,4)==0xaabbccdd);
 live.clear_physical_pool();
 assert(live.read(0x8cfa1c,4)==2&&state.tables.groups[0].size()==2&&state.stored_actor_images[0].size()==2&&state.actor_extras[0].size()==2&&state.runtimes[0].size()==2&&state.person_encoding[0].size()==2);
 assert(actor_address==&state.tables.groups[0][0]&&actor_capacity==state.tables.groups[0].capacity()&&image_capacity==state.stored_actor_images[0].capacity()&&extra_capacity==state.actor_extras[0].capacity()&&runtime_capacity==state.runtimes[0].capacity()&&encoding_capacity==state.person_encoding[0].capacity());
 for(U offset=0;offset<0x45000;offset+=4)assert(live.read(A+offset,4)==0);
 for(std::size_t slot=0;slot<2;++slot){assert(snapshot_mission_actor(state,{0,slot})==AiActorImage{});assert(state.stored_actor_images[0][slot]==AiActorImage{});assert(!state.tables.groups[0][slot].person_slot&&!state.runtimes[0][slot]);assert(state.actor_extras[0][slot].field_f8==0&&state.person_encoding[0][slot]==ActorPersonEncoding::slot);}
 assert(state.people[0].owner==person_owner&&person_called==before_callbacks&&animations.size()==1&&animations.pose(kCharacterInstanceBase));
 live.clear_physical_pool();assert(live.read(0x8cfa1c,4)==2);
 // Count-zero and full-count owners retain their own extents on the same call.
 allocation.clear_physical_pool();assert(allocated.tables.groups[0].size()==1&&allocation.read(A+4,4)==0);
 full.clear_physical_pool();assert(full_state.tables.groups[0].size()==1024&&full.read(A+1023*0x114,4)==0);
 MissionTickState empty;init_mission_tick(empty,{},AiActorTables{},AiItemDefinitionTable(definitions()),{},0);MissionPrimaryActorLive empty_owner(empty,b,std::vector<AiActorImage>(1024));empty_owner.write(A+0x110,4,0xffffffff);empty_owner.clear_physical_pool();assert(empty.tables.groups[0].empty()&&empty_owner.read(A+0x110,4)==0);
}
