#include "dfre/mission_client_outbox.hpp"
#include "dfre/mission_logic.hpp"
#include "dfre/mission_peer_budget.hpp"
#include "dfre/mission_peer_frame.hpp"
#include "dfre/mission_peer_parser.hpp"
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
void reliable_packet(){
    std::map<std::uint32_t,std::uint32_t> cells{{0x67f528,1},{0x50133c,7},{0x501340,7}};
    GameMessageStage messages;MissionServerMessageBuffer server;
    std::array<std::uint16_t,kGameMessageDedupSlots> dedup{};
    std::uint32_t received=0,idle=0;
    GameMessageLiveBindings live;live.read_global=[&](auto at){return cells[at];};
    live.dedup=&dedup;live.received_bytes_bcfc24=&received;live.update_counter_bc5d20=&idle;
    messages.set_live_bindings(live);
    std::vector<AssetBytes> clients(17,AssetBytes(mission_client_record_size));
    const auto put=[&](unsigned at,std::uint32_t value,unsigned width){for(unsigned i=0;i<width;++i)clients[0].at(at+i)=std::uint8_t(value>>(8*i));};
    const auto get=[&](unsigned at,unsigned width){std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(clients[0].at(at+i))<<(8*i);return value;};
    put(0,7,4);put(0x18c,1,1);put(0x1c4,5,2);
    MissionClientOutboxBindings transport;
    transport.cell=[&](auto at){return cells[at];};
    transport.client=[&](auto slot){return MissionClientRecordView{clients.at(slot).data(),clients.at(slot).size()};};
    MissionClientOutbox outbox(messages,server,transport);
    MissionPeerFrameBindings peer;peer.cell=transport.cell;peer.store=[&](auto at,auto value){cells[at]=value;};
    MissionPeerParserBindings parser;parser.client=transport.client;
    parser.read=[&](auto at,auto width){check(width==4,"Unexpected packet cell width");return cells[at];};
    parser.write=[&](auto at,auto width,auto value){check(width==4,"Unexpected packet store width");cells[at]=value;};
    parser.resolve=[&](auto at,std::size_t length)->std::uint8_t*{
        const auto offset=std::uint32_t(at-0x8ec1ccu);
        if(at<0x8ec1ccu||std::uint64_t(offset)+length>messages.peer_frame_buffer().bytes_8ec1cc.size())return nullptr;
        return messages.peer_frame_buffer().data()+offset;
    };
    // A client without an actor still receives an ACK for its reliable shot.
    // The source actor guard skips the shot handler for this input.
    messages.enqueue_shot();messages.flush_queue();
    check(messages.queue().active_count()==1&&messages.network_packet().size()==3,"Reliable producer lost its queued shot");
    const auto sequence=std::uint16_t(messages.network_packet().data()[1]|(std::uint16_t(messages.network_packet().data()[2])<<8));
    check(mission_peer_frame_receive(messages.peer_frame_buffer(),peer)==0x8ec1cc,"Shared peer receive did not publish its buffer");
    mission_peer_parser_parse(outbox,messages.table(),parser);
    check(get(0x9b04,2)==sequence&&get(0x204+8,1)==0x14&&get(0x204+10,2)==sequence,"Parser lost current ACK or dedup state");
    outbox.tick();check(!messages.pending_local_echo().empty(),"ACK did not reach the shared local echo");
    check(messages.check_receive(),"ACK echo was not received");
    check(messages.dispatch_received().empty(),"Live ACK produced inspection actions");
    check(messages.queue().active_count()==0,"Live ACK did not cancel the producer record");
}
}
int main(){try{run();reliable_packet();std::cout<<"live peer frame checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
