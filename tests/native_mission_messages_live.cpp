#include "dfre/game_messages.hpp"
#include "dfre/mission_projectile_spawn.hpp"
#include <iostream>
#include <stdexcept>
using namespace dfre;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void run(){
    MissionTickState state;AiActorTables tables;tables.groups[3].resize(2);
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},0);
    state.projectiles.resize(512);state.globals.mission_67f528=1;
    std::vector<RuntimeItemDefinition> items(4);items[1].id=0xfa1;items[2].id=0xfc1;items[3].id=0xfca;
    MissionProjectileSpawnCells cells;cells.classes[4].field_30=0x80;cells.classes[4].field_18=7;cells.classes[4].field_20=0;
    GameMessageStage messages;std::uint32_t received=0,idle=9;std::array<std::uint16_t,kGameMessageDedupSlots> dedup{};
    std::uint8_t* packet=nullptr;unsigned heights=0,queues=0,deliveries=0;
    MissionProjectileSpawnBindings effects;
    effects.ground=[&](auto&,auto,auto){
        check(packet&&messages.pending_local_echo().empty()&&received==16,"Message delivery did not publish current receive state");
        packet[8]=1;packet[10]=0x34;packet[11]=0x12;++heights;return 0;
    };
    effects.queue=[&](auto&,auto id,const auto& payload){
        check(id==0x17&&payload==std::vector<std::uint8_t>({1,0}),"Receive lost its saved owner or current packet bytes");
        check(messages.queue().enqueue(messages.table(),std::uint8_t(id),payload.data(),std::uint16_t(payload.size())),"Reply queue refused the source packet");
        packet[14]=4;++queues;
    };
    effects.loudness=[](auto&,auto){return 0;};
    MissionProjectileSpawn spawn(state,cells,items,effects);
    GameMessageLiveBindings bindings;bindings.dedup=&dedup;bindings.received_bytes_bcfc24=&received;bindings.update_counter_bc5d20=&idle;
    bindings.read_global=[&](auto address){
        if(address==0x67f528)return state.globals.mission_67f528;
        check(address==0x67f520,"Unexpected message global read");return state.globals.multiplayer_67f520;
    };
    bindings.projectile_09=[&](auto* bytes,auto length){
        check(length==15&&idle==9,"Message parent counter changed before its body");packet=bytes;spawn.receive(bytes,length);packet=nullptr;++deliveries;
    };
    messages.set_live_bindings(std::move(bindings));
    std::array<std::uint8_t,16> frame{};frame[0]=9;frame[15]=3;
    messages.submit_local_echo(frame.data(),frame.size());
    check(messages.check_receive(),"Local shot message was not received");
    check(messages.dispatch_received().empty(),"Live message produced deferred actor actions");
    const auto& shot=state.projectiles[1];
    check(deliveries==1&&heights==1&&queues==1&&received==16&&idle==9,"Shot message ran in the wrong order");
    check(cells.ring_index==1&&shot.owner_slot==0&&shot.aim_yaw==0x12340000&&shot.ttl==7,"Shot spawn did not read the live message bytes");
    check(messages.queue().active_count()==1,"Inactive-owner reply is missing");
    check(!messages.check_receive(),"A consumed message was received twice");
}
}
int main(){try{run();std::cout<<"live message and projectile checks passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
