#include "dfre/mission_periodic_services.hpp"
#include "dfre/mission_client_live.hpp"
#include "dfre/mission_round_checks.hpp"
#include "dfre/mission_live.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace dfre;
namespace {
using U=std::uint32_t;
void check(bool value,const char* error){if(!value)throw std::runtime_error(error);}
void run(){
    constexpr U frame=0x1d000000,definition=0x18000000;
    AiActorTables tables;tables.groups[0].resize(3);tables.groups[3].resize(2);
    for(auto group:{0u,3u})for(auto& actor:tables.groups[group]){actor.active=true;actor.definition_index=1;actor.field_6c=0x5a;actor.position[0]=123;actor.position[1]=456;actor.position[2]=789;}
    std::vector<AiItemDefinition> definitions(2);definitions[1].field_28=2;
    MissionTickState state;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),{},1);
    for(auto group:{0u,3u})for(std::size_t i=0;i<state.tables.groups[group].size();++i){auto image=snapshot_mission_actor(state,{std::uint8_t(group),i});image[4]=1;image[0x6d]=0xff;apply_mission_actor_image(state,{std::uint8_t(group),i},image);}
    MissionClientLive clients;std::array<std::uint8_t,0x500> scratch{};std::map<U,std::uint8_t> memory;
    const auto raw_read=[&](U at,U width){U value=0;for(U i=0;i<width;++i)value|=U(memory[at+i])<<(i*8);return value;};
    const auto raw_write=[&](U at,U width,U value){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(value>>(i*8));};
    MissionPeriodicBindings b;
    b.read=[&](U at,U width){if(at>=frame&&std::uint64_t(at-frame)+width<=scratch.size()){U value=0;for(U i=0;i<width;++i)value|=U(scratch[at-frame+i])<<(8*i);return value;}if(clients.claims(at,width))return clients.read(at,width);
        if(width==4&&(at==0x8cfa18||at==0x8cfa1c||at==0x8cf9a0))return U(state.tables.groups[at==0x8cfa18?3:0].size());
        for(const std::uint8_t group:{std::uint8_t(0),std::uint8_t(3)}){const U base=mission_actor_source_address(group,0);if(at<base)continue;const U delta=at-base,slot=delta/0x114,offset=delta%0x114;if(slot>=state.tables.groups[group].size()||offset+width>0x114)continue;const auto image=snapshot_mission_actor(state,{group,slot});U value=0;for(U i=0;i<width;++i)value|=U(image[offset+i])<<(8*i);return offset==4&&width==4?(value?definition+value*0x13c:0):value;}
        return raw_read(at,width);};
    b.write=[&](U at,U width,U value){if(at>=frame&&std::uint64_t(at-frame)+width<=scratch.size()){for(U i=0;i<width;++i)scratch[at-frame+i]=std::uint8_t(value>>(8*i));return;}if(clients.claims(at,width)){clients.write(at,width,value);return;}
        for(const std::uint8_t group:{std::uint8_t(0),std::uint8_t(3)}){const U base=mission_actor_source_address(group,0);if(at<base)continue;const U delta=at-base,slot=delta/0x114,offset=delta%0x114;if(slot>=state.tables.groups[group].size()||offset+width>0x114)continue;auto image=snapshot_mission_actor(state,{group,slot});for(U i=0;i<width;++i)image[offset+i]=std::uint8_t(value>>(8*i));apply_mission_actor_image(state,{group,slot},image);return;}
        raw_write(at,width,value);};
    b.write(0x67f528,4,1);b.write(0x67f53c,4,0);b.write(0x501340,4,7);b.write(definition+0x13c+0x20,4,0xfc3);
    const U client=mission_client_source_base;clients.write(client,4,7);clients.write(client+8,4,mission_actor_source_address(3,1));clients.write(client+0x18c,1,1);clients.write(client+0x1c4,2,5);clients.write(client+0x1b8,4,1);clients.write(client+0x9c,4,1);clients.write(client+0x1a0,4,9);clients.write(client+0x1b4,4,1);clients.write(client+0x1b0,4,1);
    GameMessageStage messages;MissionServerMessageBuffer buffer;MissionClientOutboxBindings out;out.client=[&](auto slot){return clients.client(slot);};out.cell=[&](auto at){return b.read(at,4);};MissionClientOutbox outbox(messages,buffer,out);
    MissionRoundCheckBindings round;round.read=b.read;round.write=b.write;round.enqueue_441750=[&](auto slot,auto id,auto* bytes,auto length){check(outbox.enqueue(slot,std::int32_t(id),bytes,length),"Score outbox enqueue failed");};
    unsigned publishes=0;bool mutate_counter=false;
    b.call=[&](U site,const std::vector<U>& args)->U{switch(site){case 0x49a620:for(U i=0;i<args.at(2);++i)b.write(args.at(0)+i,1,args.at(1));return args.at(0);case 0x446da0:mission_periodic_team_scores(b);return 0;case 0x448130:return U(mission_periodic_compare_clients(b,args.at(0),args.at(1)));case 0x4423f0:check(args.at(1)>=frame&&std::uint64_t(args.at(1)-frame)+args.at(2)<=scratch.size(),"Score packet has no current frame");mission_round_broadcast(round,args.at(0),scratch.data()+args.at(1)-frame,args.at(2));++publishes;if(mutate_counter)b.write(0x4f8218,4,0xffffffff);return 0;case 0x441750:check(outbox.enqueue((args.at(0)-client)/mission_client_record_size,std::int32_t(args.at(1)),nullptr,args.at(3)),"Client poll enqueue failed");return 0;default:throw std::runtime_error("Unbound native periodic service");}};
    std::array<std::uint16_t,kGameMessageDedupSlots> dedup{};U received=0,idle=0;unsigned delivered=0;
    GameMessageLiveBindings live;live.dedup=&dedup;live.received_bytes_bcfc24=&received;live.update_counter_bc5d20=&idle;live.read_global=[&](auto at){return b.read(at,4);};
    live.message=[&](auto id,auto* bytes,auto length){check(id==0x15&&length==0x8f,"Score frame type changed");check(bytes[0x8e]==1&&bytes[13]==1&&bytes[0x2d]==0&&bytes[0x4d]==4&&bytes[0x4e]==0,"Score frame lost current actor or score");++delivered;return true;};messages.set_live_bindings(live);
    mission_periodic_scoreboard(b,frame);check(publishes==1,"Scoreboard did not publish");outbox.tick();check(messages.check_receive(),"Scoreboard did not reach actual local echo");check(messages.dispatch_received().empty()&&delivered==1,"Scoreboard produced inspection actions or missed the receive boundary");
    mission_periodic_clear_actor_flags(b);for(auto group:{0u,3u})for(std::size_t i=0;i<state.tables.groups[group].size();++i)check(state.actor_extras[group][i].field_6d==0xbf&&state.tables.groups[group][i].field_6c==0x5a,"Periodic clear changed another actor field");
    b.write(0x67f53c,4,5);b.write(0x4f8218,4,2);mutate_counter=true;mission_periodic_publish_flag(b,frame);check(publishes==2&&b.read(0x4f8218,4)==0,"Flag publisher lost callback changes to its current cursor");check(b.read(frame+12,2)==2&&b.read(frame,4)==123&&b.read(frame+4,4)==456&&b.read(frame+8,4)==789,"Flag publisher lost canonical actor fields");
    const U remote=client+mission_client_record_size;clients.write(remote+0x18c,1,1);clients.write(remote+0x1c4,2,5);clients.write(remote+8,4,mission_actor_source_address(3,0));
    const auto original_call=b.call;unsigned polls=0;b.call=[&](U site,const std::vector<U>& args){if(site==0x441750){check(args.at(0)==remote&&args.at(1)==0x4a&&args.at(2)==0&&args.at(3)==0,"Periodic poll changed source client or packet");++polls;}return original_call(site,args);};mission_periodic_poll_clients(b);check(polls==1,"Periodic poll did not use the current remote client");b.call=original_call;
    b.write(0x67f524,4,0);const auto before=publishes;mission_periodic_advertise(b,frame);check(publishes==before,"Source advertisement gate sent a packet");
    const auto calls=b.call;b.call={};bool failed=false;try{mission_periodic_scoreboard(b,frame);}catch(const std::runtime_error&){failed=true;}check(failed,"Missing score body silently succeeded");b.call=calls;
}
}
int main(){try{run();std::cout<<"Periodic canonical actor/client/outbox checks passed; receive15 is an explicit fixture\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
