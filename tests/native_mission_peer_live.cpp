#include "dfre/mission_client_outbox.hpp"
#include "dfre/mission_logic.hpp"
#include "dfre/mission_peer_budget.hpp"
#include "dfre/mission_peer_frame.hpp"
#include "dfre/mission_peer_timers.hpp"
#include "dfre/mission_round_services.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

using namespace dfre;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void run(){
    std::map<std::uint32_t,std::uint8_t> bytes;
    const auto read=[&](std::uint32_t at,unsigned width=4){std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(bytes[at+i])<<(8*i);return value;};
    const auto write=[&](std::uint32_t at,unsigned width,std::uint32_t value){for(unsigned i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(8*i));};
    const auto word=[&](auto at){return read(at);};
    const auto store=[&](auto at,auto value){write(at,4,value);};
    std::vector<AssetBytes> clients(17,AssetBytes(mission_client_record_size));
    GameMessageStage messages;MissionServerMessageBuffer server;
    MissionClientOutboxBindings transport;
    transport.client=[&](auto slot){return MissionClientRecordView{clients.at(slot).data(),clients.at(slot).size()};};
    transport.cell=word;MissionClientOutbox outbox(messages,server,transport);
    MissionPeerFrameBindings peer{word,store,{}};
    MissionPeerTimerBindings timers;timers.read=word;timers.write=store;
    MissionPeerBudgetBindings budget{read,write};
    MissionRoundServiceBindings round;round.store=write;
    MissionLogicFrameBindings logic;logic.cell=word;logic.store=store;
    logic.client=[&](auto slot,auto at,auto width){std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(clients.at(slot).at(at+i))<<(8*i);return value;};
    logic.client_store=[&](auto slot,auto at,auto width,auto value){for(unsigned i=0;i<width;++i)clients.at(slot).at(at+i)=std::uint8_t(value>>(8*i));};
    std::vector<std::uint32_t> calls;
    logic.call=[&](auto site,const auto&)->std::uint32_t{
        calls.push_back(site);
        switch(site){
        case 0x460e40:return mission_peer_frame_receive(messages.peer_frame_buffer(),peer);
        case 0x4477f0:mission_peer_timers(timers);break;
        case 0x444e30:mission_peer_budget_update(budget);break;
        case 0x444f60:mission_round_clear_frame_flag(round);break;
        case 0x4421d0:outbox.tick();break;
        default:throw std::runtime_error("Unbound peer frame body");
        }
        return 0;
    };
    // Current single-player frame inputs: no actor groups, no received packet,
    // no expired timer, no timed result, and all client slots inactive.
    store(0x67f528,1);store(0x67dbe8,0xffffffff);store(0x6e5570,3);store(0x6e5574,10000);
    store(0x6e5784,3);store(0x6e5880,0);
    mission_logic_frame(logic);
    check(calls==std::vector<std::uint32_t>({0x460e40,0x4477f0,0x444e30,0x444f60,0x4421d0}),"Peer frame order changed");
    check(read(0x6e5784)==2&&read(0x6e5880)==0xffffffff,"Frame lost current timer stores");
    check(read(0x6e5584)==3&&read(0x6e5578)==3&&read(0x6e5570)==0,"Budget and round clear lost shared state");
    for(unsigned i=0;i<17;++i)check(logic.client(i,0x94,4)==1,"Frame skipped a client idle counter");
    calls.clear();mission_logic_frame(logic);
    check(read(0x6e5784)==1&&read(0x6e5584)==0&&read(0x6e5880)==0xfffffffe,"Second frame used stale cells");
}
}
int main(){try{run();std::cout<<"live peer frame checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
